#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include <limine.h>
#include <string.h>

#include <kernel/pmm/pmm.h>
#include <kernel/stdio/kstdio.h>

// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests"))) static volatile LIMINE_BASE_REVISION(3);

static unsigned long liballoc_irqflags;

int liballoc_lock(void)
{
    liballoc_irqflags = irq_save();
    return 0;
}

int liballoc_unlock(void)
{
    // Release lock first, then restore IF for this CPU.
    irq_restore(liballoc_irqflags);
    return 0;
}

void *liballoc_alloc(size_t pages)
{
    if (pages == 0)
        return NULL;

    uintptr_t phys = pmm_alloc_pages(pages);
    if (!phys)
        return NULL;

    return phys_to_virt(phys);
}

int liballoc_free(void *ptr, size_t pages)
{
    if (!ptr || pages == 0)
        return 0;

    uintptr_t phys = virt_to_phys(ptr);
    pmm_free_pages(phys, pages);
    return 0;
}

// #include "../include/kernel/pmm/pmm.h"

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start"))) static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end"))) static volatile LIMINE_REQUESTS_END_MARKER;

__attribute__((used, section(".limine_requests"))) static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0};

// // Halt and catch fire function.
// i dont know why this cant be somewhere else, it just doesnt work for whatever reason
static void hcf(void)
{
    for (;;)
    {
#if defined(__x86_64__)
        asm("hlt");
#elif defined(__aarch64__) || defined(__riscv)
        asm("wfi");
#elif defined(__loongarch64)
        asm("idle 0");
#endif
    }
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void)
{
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false)
    {
        hcf();
    }

    pmm_init_after_kernel();
    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1)
    {
        hcf();
    }
    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    for (size_t i = 0; i < 100; i++)
    {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[(framebuffer->pitch / 4) + i] = 0xffffff;
    }
    // We're done, just hang...
    hcf();
}