#ifndef NV_UIARABIC_H
#define NV_UIARABIC_H
#include <SDL.h>
/* UI-only logical text. Subtitle code must never pass through this renderer. */
void uiar_iniciar(const char *base);
void uiar_encerrar(void);
int uiar_tem_arabe(const char *s);
void uiar_sem_negrito(const char *src, char *dst, size_t capacity);
int uiar_largura(const char *s, int pixels, int bold, int italic);
SDL_Surface *uiar_render(const char *s, int pixels, int bold, int italic,
                         int ascent, int height, SDL_Color color);
#endif
