#include <kernel/psf/psf.h>
#include <limine.h>

static unsigned char *font_glyphs;

void psf_init()
{
  PSF_font *font = (PSF_font *)&_binary_zap_ext_light32_psf_start;
  font_glyphs = (char *)((unsigned char *)&_binary_zap_ext_light32_psf_start + font->headersize + font->numglyph * font->bytesperglyph);
}

void putc(struct limine_framebuffer *fb, char c, int cx, int cy, uint32_t fg, uint32_t bg)
{
  PSF_font *font = (PSF_font *)&_binary_zap_ext_light32_psf_start;
  int bytesperline = (font->width + 7) / 8;

  unsigned char *glyph = (unsigned char *)&_binary_zap_ext_light32_psf_start + font->headersize + (c > 0 && c < font->numglyph ? c : 0) * font->bytesperglyph;

  int offs =
      (cy * font->height * (fb->pitch / 4)) +
      (cx * (font->width + 1) * sizeof(uint32_t));

  int x, y, line, mask;
  for (y = 0; y < font->height; y++)
  {
    line = offs;
    mask = 1 << (font->width - 1);
    for (x = 0; x < font->width; x++)
    {
      *((uint32_t *)(fb + line)) = *((unsigned int *)glyph) & mask ? fg : bg;
      mask >>= 1;
      line += sizeof(uint32_t);
    }
    glyph += bytesperline;
    offs += (fb->pitch / 4);
  }
}