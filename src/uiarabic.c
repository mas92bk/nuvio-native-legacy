#include "uiarabic.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int uiar_tem_arabe(const char *s) {
  if (!s) return 0;
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    /* Arabic, Arabic Supplement/Extended, and Arabic presentation forms. */
    if ((p[0] >= 0xd8 && p[0] <= 0xdb && (p[1] & 0xc0) == 0x80) ||
        (p[0] == 0xe0 && p[1] >= 0xa0 && p[1] <= 0xa3 && p[2]) ||
        (p[0] == 0xef && p[1] >= 0xad && p[1] <= 0xbb && p[2])) return 1;
    p++;
  }
  return 0;
}

/* Drop only the two supported UI emphasis tags; preserve Unicode controls. */
void uiar_sem_negrito(const char *src, char *dst, size_t capacity) {
  size_t n=0;
  if (!capacity) return;
  while (*src) {
    if (!strncmp(src,"<b>",3)) {src+=3;continue;}
    if (!strncmp(src,"</b>",4)) {src+=4;continue;}
    unsigned char c=(unsigned char)*src;
    size_t bytes=c<0x80?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;
    if (n+bytes>=capacity || strlen(src)<bytes) break;
    memcpy(dst+n,src,bytes); n+=bytes; src+=bytes;
  }
  dst[n]=0;
}

#ifdef NV_ASS_LIBASS
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <fribidi.h>
#include <limits.h>

/* These are the same static libraries already used by the subtitle renderer.
   UI calls are confined to the main thread; no shared libass renderer/state. */
static FT_Library library;
static FT_Face faces[4];
static hb_font_t *fonts[4];
static int last_size[4];
static char paths[4][1024];
static int failed[4];

void uiar_encerrar(void) {
  for (int i=0;i<4;i++) {
    if (fonts[i]) hb_font_destroy(fonts[i]);
    if (faces[i]) FT_Done_Face(faces[i]);
    fonts[i]=NULL; faces[i]=NULL; last_size[i]=failed[i]=0;
  }
  if (library) FT_Done_FreeType(library);
  library=NULL;
}
void uiar_iniciar(const char *base) {
  uiar_encerrar();
  snprintf(paths[0],sizeof paths[0],"%sfonts/NotoSansArabic-Regular.ttf",base);
  snprintf(paths[1],sizeof paths[1],"%sfonts/NotoSansArabic-Bold.ttf",base);
  snprintf(paths[2],sizeof paths[2],"%sfonts/InterDisplay-Regular.ttf",base);
  snprintf(paths[3],sizeof paths[3],"%sfonts/InterDisplay-Bold.ttf",base);
}
static int prepare(int px,int bold) {
  if (px<1 || px>512 || failed[bold]) return 0;
  if (!library && FT_Init_FreeType(&library)) return 0;
  if (!faces[bold]) {
    if (FT_New_Face(library,paths[bold],0,&faces[bold])) {
      fprintf(stderr,"[ui-ar] Missing UI font: %s\n",paths[bold]); failed[bold]=1; return 0;
    }
    fonts[bold]=hb_ft_font_create_referenced(faces[bold]);
  }
  if (last_size[bold]!=px) {
    if (FT_Set_Pixel_Sizes(faces[bold],0,(FT_UInt)px)) return 0;
    hb_ft_font_changed(fonts[bold]); last_size[bold]=px;
  }
  return 1;
}
typedef struct { unsigned glyph; int x,y,face; } Glyph;
typedef struct { Glyph *g; unsigned n; int left,right,top,bottom,advance; } Layout;
typedef struct { int start,end,visual,face; } Run;
static int run_cmp(const void *a,const void *b) {
  const Run *x=a,*y=b; return (x->visual>y->visual)-(x->visual<y->visual);
}
static int floor64(int v) { return v>=0 ? v/64 : -((-v+63)/64); }
static int ceil64(int v) { return -floor64(-v); }

static int layout(const char *s,int px,int bold,int italic,Layout *l) {
  memset(l,0,sizeof *l);
  if (!s || strlen(s)>16384 || !prepare(px,bold)) return 0;
  int fallback = prepare(px,bold+2);
  size_t cap=strlen(s)+1;
  FriBidiChar *chars=calloc(cap,sizeof *chars);
  FriBidiStrIndex *map=calloc(cap,sizeof *map);
  FriBidiLevel *levels=calloc(cap,sizeof *levels);
  Run *runs=calloc(cap,sizeof *runs);
  if (!chars || !map || !levels || !runs) goto bad;
  int n=fribidi_charset_to_unicode(FRIBIDI_CHAR_SET_UTF8,s,(int)strlen(s),chars);
  if (n<=0) goto bad;
  FriBidiParType base=FRIBIDI_PAR_ON;
  if (!fribidi_log2vis(chars,n,&base,NULL,map,NULL,levels)) goto bad;
  int nr=0;
  for (int i=0;i<n;) {
    int face = fallback && !FT_Get_Char_Index(faces[bold],chars[i]) &&
               FT_Get_Char_Index(faces[bold+2],chars[i]) ? bold+2 : bold;
    int end=i+1, v=map[i];
    while (end<n && levels[end]==levels[i]) {
      int next = fallback && !FT_Get_Char_Index(faces[bold],chars[end]) &&
                 FT_Get_Char_Index(faces[bold+2],chars[end]) ? bold+2 : bold;
      if (next != face) break;
      if(map[end]<v)v=map[end];
      end++;
    }
    runs[nr++]=(Run){i,end,v,face}; i=end;
  }
  qsort(runs,(size_t)nr,sizeof *runs,run_cmp);
  /* HarfBuzz can expand one codepoint into multiple glyphs. Grow by the actual
     output count rather than assuming one glyph per character. */
  int pen=0;
  FT_Matrix matrix={0x10000,italic?0x3600:0,0,0x10000};
  FT_Set_Transform(faces[bold],&matrix,NULL);
  if (fallback) FT_Set_Transform(faces[bold+2],&matrix,NULL);
  for(int r=0;r<nr;r++) {
    Run run=runs[r];
    hb_buffer_t *b=hb_buffer_create();
    hb_buffer_add_utf32(b,chars,n,(unsigned)run.start,run.end-run.start);
    hb_buffer_set_direction(b,(levels[run.start]&1)?HB_DIRECTION_RTL:HB_DIRECTION_LTR);
    hb_buffer_guess_segment_properties(b);
    hb_shape(fonts[run.face],b,NULL,0);
    unsigned count=0;
    hb_glyph_info_t *info=hb_buffer_get_glyph_infos(b,&count);
    hb_glyph_position_t *pos=hb_buffer_get_glyph_positions(b,NULL);
    Glyph *newg=realloc(l->g,(l->n+count)*sizeof *newg);
    if(!newg && count) { hb_buffer_destroy(b); goto bad; }
    l->g=newg;
    for(unsigned j=0;j<count;j++) {
      Glyph g={info[j].codepoint,pen+pos[j].x_offset,-pos[j].y_offset,run.face};
      l->g[l->n++]=g;
      if(!FT_Load_Glyph(faces[g.face],g.glyph,FT_LOAD_DEFAULT) &&
         !FT_Render_Glyph(faces[g.face]->glyph,FT_RENDER_MODE_NORMAL)) {
        FT_GlyphSlot slot=faces[g.face]->glyph;
        int left=floor64(g.x)+slot->bitmap_left,top=floor64(g.y)-slot->bitmap_top;
        if(left<l->left) l->left=left;
        if(top<l->top) l->top=top;
        if(left+(int)slot->bitmap.width>l->right) l->right=left+(int)slot->bitmap.width;
        if(top+(int)slot->bitmap.rows>l->bottom) l->bottom=top+(int)slot->bitmap.rows;
      }
      pen+=pos[j].x_advance;
    }
    hb_buffer_destroy(b);
  }
  l->advance=ceil64(pen);
  if(l->advance>l->right) l->right=l->advance;
  free(chars);free(map);free(levels);free(runs);
  return 1;
bad:
  free(chars);free(map);free(levels);free(runs);free(l->g);l->g=NULL;return 0;
}
int uiar_largura(const char *s,int px,int bold,int italic) {
  Layout l;
  if(!layout(s,px,!!bold,italic,&l)) return -1;
  int w=l.right-l.left;free(l.g);return w;
}
SDL_Surface *uiar_render(const char *s,int px,int bold,int italic,
                         int ascent,int height,SDL_Color color) {
  Layout l;bold=!!bold;
  if(!layout(s,px,bold,italic,&l))return NULL;
  int top=l.top < -ascent ? l.top : -ascent;
  int bottom=l.bottom > height-ascent ? l.bottom : height-ascent;
  int w=l.right-l.left,h=bottom-top;
  if(w<1)w=1;
  if(h<1)h=1;
  if(w>32768 || h>2048) {free(l.g);return NULL;}
  SDL_Surface *sf=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ABGR8888);
  if(!sf) {free(l.g);return NULL;}
  SDL_FillRect(sf,NULL,0);
  for(unsigned i=0;i<l.n;i++) {
    Glyph g=l.g[i];
    if(FT_Load_Glyph(faces[g.face],g.glyph,FT_LOAD_DEFAULT) ||
       FT_Render_Glyph(faces[g.face]->glyph,FT_RENDER_MODE_NORMAL))continue;
    FT_GlyphSlot slot=faces[g.face]->glyph; FT_Bitmap *bm=&slot->bitmap;
    int x=floor64(g.x)+slot->bitmap_left-l.left,y=floor64(g.y)-slot->bitmap_top-top;
    for(unsigned by=0;by<bm->rows;by++) for(unsigned bx=0;bx<bm->width;bx++) {
      int dx=x+(int)bx,dy=y+(int)by;
      if(dx<0||dy<0||dx>=w||dy>=h)continue;
      const unsigned char *row=bm->buffer+(bm->pitch>=0 ? (int)by*bm->pitch : ((int)bm->rows-1-(int)by)*(-bm->pitch));
      unsigned a=bm->pixel_mode==FT_PIXEL_MODE_MONO ? ((row[bx/8]&(0x80>>(bx%8)))?255:0) : row[bx];
      a=a*color.a/255;
      unsigned char *dst=(unsigned char *)sf->pixels+dy*sf->pitch+dx*4;
      dst[0]=color.r;dst[1]=color.g;dst[2]=color.b;
      dst[3]=(unsigned char)(a+dst[3]*(255-a)/255);
    }
  }
  free(l.g);return sf;
}
#else
void uiar_iniciar(const char *base) {(void)base;}
void uiar_encerrar(void) {}
int uiar_largura(const char *s,int p,int b,int i) {(void)s;(void)p;(void)b;(void)i;return -1;}
SDL_Surface *uiar_render(const char *s,int p,int b,int i,int a,int h,SDL_Color c) {
  (void)s;(void)p;(void)b;(void)i;(void)a;(void)h;(void)c;return NULL;
}
#endif
