#ifndef _H_FRAMEBUFFER
#define _H_FRAMEBUFFER 1

#include <limine.h>

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

static volatile struct limine_framebuffer_request framebuffer_request;

#endif
