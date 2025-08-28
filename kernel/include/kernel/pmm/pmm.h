#ifndef _H_PMM
#define _H_PMM 1

#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

static inline unsigned long irq_save(void);

static inline void irq_restore(unsigned long flags);

void *memset(void *s, int c, size_t n);

#define PAGE_SIZE 4096

#define BIT_SET(i) (pmm_bitmap[(i) / 8] |= (1u << ((i) % 8)))
#define BIT_CLEAR(i) (pmm_bitmap[(i) / 8] &= ~(1u << ((i) % 8)))
#define BIT_TEST(i) (pmm_bitmap[(i) / 8] & (1u << ((i) % 8)))

extern char _kernel_end;

/* ---- PMM bitmap globals ---- */
static uint8_t *pmm_bitmap; // virtual pointer to bitmap storage
static size_t pmm_total_pages;
static size_t pmm_bitmap_bytes;
static uintptr_t pmm_bitmap_phys;  // physical base of bitmap
static uintptr_t pmm_bitmap_pages; // number of pages used by bitmap

/* bitmap helpers */
#define BIT_SET(i) (pmm_bitmap[(i) / 8] |= (1u << ((i) % 8)))
#define BIT_CLEAR(i) (pmm_bitmap[(i) / 8] &= ~(1u << ((i) % 8)))
#define BIT_TEST(i) (pmm_bitmap[(i) / 8] & (1u << ((i) % 8)))

static inline uintptr_t hhdm_offset(void);
/* Convert a physical address to a kernel virtual (HHDM) pointer */
static inline void *phys_to_virt(uintptr_t phys);

/* Convert a kernel virtual (HHDM) pointer back to physical */
static inline uintptr_t virt_to_phys(const void *virt);

static inline uintptr_t align_up(uintptr_t a, uintptr_t align);
static inline uintptr_t align_down(uintptr_t a, uintptr_t align);

void pmm_init_after_kernel(void);

uintptr_t pmm_alloc_pages(size_t pages);
void pmm_free_pages(uintptr_t phys_addr, size_t pages);

#endif