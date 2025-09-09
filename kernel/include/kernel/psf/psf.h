#ifndef _H_PSF
#define _H_PSF 1

#include <stdint.h>

extern char _binary_zap_ext_light32_psf_start;
extern char _binary_zap_ext_light32_psf_end;

#define PSF_FONT_MAGIC 0x864ab572

typedef struct
{
  uint32_t magic;         /* magic bytes to identify PSF */
  uint32_t version;       /* zero */
  uint32_t headersize;    /* offset of bitmaps in file, 32 */
  uint32_t flags;         /* 0 if there's no unicode table */
  uint32_t numglyph;      /* number of glyphs */
  uint32_t bytesperglyph; /* size of each glyph */
  uint32_t height;        /* height in pixels */
  uint32_t width;         /* width in pixels */
} PSF_font;

void psf_init();
void putc(struct limine_framebuffer *fb, char c, int cx, int cy, uint32_t fg, uint32_t bg);

#endif