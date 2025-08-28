#define SSFN_CONSOLEBIT_TRUECOLOR
#include <kernel/ssfn/ssfn.h>
#include <kernel/stdio/kstdio.h>
#include <limine.h>

extern unsigned char FreeSerif_sfn[];
extern unsigned int FreeSerif_sfn_len;

ssfn_buf_t ssfn_dst;

void init_ssfn()
{
  struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

  // Attach framebuffer to SSFN
  ssfn_dst.ptr = fb->address;
  ssfn_dst.w = fb->width;
  ssfn_dst.h = fb->height;
  ssfn_dst.p = fb->pitch;
  ssfn_dst.x = 0;
  ssfn_dst.y = 0;
  ssfn_dst.fg = 0xFFFFFFFF; // white text
  ssfn_dst.bg = 0x00000000; // transparent
}

void load_font()
{
  ssfn_src = (ssfn_font_t *)FreeSerif_sfn;
}

void kout(const char *str, int x, int y)
{
  ssfn_dst.x = x;
  ssfn_dst.y = y;
  while (*str)
  {
    ssfn_putc(*str++);
  }
}