#include "../src/uiarabic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(int argc,char **argv) {
  (void)argc;
  assert(SDL_Init(0)==0);
  const char *base=getenv("UI_AR_FONT_BASE");
  uiar_iniciar(base ? base : "deploy/app/");
  const char *samples[]={"الإعدادات","متابعة المشاهدة","مصادر Trakt · 5 أفلام · 1080p",
      "هل تريد حذف هذا الملف؟","(الموسم 1) الحلقة 12: البداية","لا إله إلا الله",
      "تغيير اللغة…","العربية English 123 (اختبار)","عَرَبِيّ","← → تغيير القيمة · ↑ ↓ التنقل · ✓ محفوظ",NULL};
  assert(!uiar_tem_arabe("Settings 123"));assert(uiar_tem_arabe(samples[0]));
  SDL_Surface *page=SDL_CreateRGBSurfaceWithFormat(0,1000,680,32,SDL_PIXELFORMAT_ABGR8888);
  assert(page);SDL_FillRect(page,NULL,SDL_MapRGBA(page->format,20,24,34,255));
  int y=15;
  for(int i=0;samples[i];i++) {
    int w=uiar_largura(samples[i],32,i%2,0);
    assert(w>0 && w<1000);
    SDL_Surface *s=uiar_render(samples[i],32,i%2,0,35,44,(SDL_Color){245,245,245,255});
    assert(s && s->w==w && s->h>=44);
    SDL_SetSurfaceBlendMode(s,SDL_BLENDMODE_BLEND);
    SDL_Rect dst={980-w,y,0,0};SDL_BlitSurface(s,NULL,page,&dst);y+=60;SDL_FreeSurface(s);
  }
  /* Logical ligatures, combining marks, long text, controls, scaling and repeated
     size/weight changes must remain measurable without memory corruption. */
  const char *edge[]={"", "ا", "لا", "ل ا", "\xe2\x80\x8f- اختبار 5", "مرحبا \xe2\x81\xa6" "English 123\xe2\x81\xa9",NULL};
  for(int k=0;k<30;k++) for(int i=1;edge[i];i++) {
    SDL_Surface *s=uiar_render(edge[i],16+k,k%2,k%3==0,40,60,(SDL_Color){255,255,255,255});
    assert(s);assert(s->w==uiar_largura(edge[i],16+k,k%2,k%3==0));SDL_FreeSurface(s);
  }
  char plain[80];
  uiar_sem_negrito("افتح <b>الإعدادات</b> الآن",plain,sizeof plain);
  assert(!strcmp(plain,"افتح الإعدادات الآن"));
  uiar_sem_negrito("عربي",plain,2); assert(!plain[0]);
  assert(uiar_largura("لا",32,0,0)<uiar_largura("ل ا",32,0,0));
  if(argv[1] && *argv[1])assert(SDL_SaveBMP(page,argv[1])==0);
  SDL_FreeSurface(page);uiar_encerrar();SDL_Quit();puts("Arabic UI layout: passed");return 0;
}
