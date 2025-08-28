#include <kernel/pmm/pmm.h>

#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

__attribute__((used, section(".limine_requests"))) static volatile struct limine_memmap_request memmap_req = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_hhdm_request hhdm_req = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0};

static inline unsigned long irq_save(void)
{
  unsigned long flags;
  __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags)::"memory");
  return flags;
}

static inline void irq_restore(unsigned long flags)
{
  __asm__ __volatile__("pushq %0; popfq" ::"r"(flags) : "memory", "cc");
}
enum vga_color
{
  VGA_COLOR_BLACK = 0,
  VGA_COLOR_BLUE = 1,
  VGA_COLOR_GREEN = 2,
  VGA_COLOR_CYAN = 3,
  VGA_COLOR_RED = 4,
  VGA_COLOR_MAGENTA = 5,
  VGA_COLOR_BROWN = 6,
  VGA_COLOR_LIGHT_GREY = 7,
  VGA_COLOR_DARK_GREY = 8,
  VGA_COLOR_LIGHT_BLUE = 9,
  VGA_COLOR_LIGHT_GREEN = 10,
  VGA_COLOR_LIGHT_CYAN = 11,
  VGA_COLOR_LIGHT_RED = 12,
  VGA_COLOR_LIGHT_MAGENTA = 13,
  VGA_COLOR_LIGHT_BROWN = 14,
  VGA_COLOR_WHITE = 15,
};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg)
{
  return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color)
{
  return (uint16_t)uc | (uint16_t)color << 8;
}

size_t strlen(const char *str)
{
  size_t len = 0;
  while (str[len])
    len++;
  return len;
}

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t *terminal_buffer;

void terminal_initialize(void)
{
  terminal_row = 0;
  terminal_column = 0;
  terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal_buffer = (uint16_t *)0xB8000;
  for (size_t y = 0; y < VGA_HEIGHT; y++)
  {
    for (size_t x = 0; x < VGA_WIDTH; x++)
    {
      const size_t index = y * VGA_WIDTH + x;
      terminal_buffer[index] = vga_entry(' ', terminal_color);
    }
  }
}

void terminal_setcolor(uint8_t color)
{
  terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = vga_entry(c, color);
}

void terminal_putchar(char c)
{
  terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
  if (++terminal_column == VGA_WIDTH)
  {
    terminal_column = 0;
    if (++terminal_row == VGA_HEIGHT)
      terminal_row = 0;
  }
}

void terminal_write(const char *data, size_t size)
{
  for (size_t i = 0; i < size; i++)
    terminal_putchar(data[i]);
}

void terminal_writestring(const char *data)
{
  terminal_write(data, strlen(data));
}

void *memset(void *s, int c, size_t n)
{
  uint8_t *p = (uint8_t *)s;

  for (size_t i = 0; i < n; i++)
  {
    p[i] = (uint8_t)c;
  }

  return s;
}

static inline uintptr_t hhdm_offset(void)
{
  // Must check response is non-NULL before calling in real init path.
  return (uintptr_t)hhdm_req.response->offset;
}

/* Convert a physical address to a kernel virtual (HHDM) pointer */
static inline void *phys_to_virt(uintptr_t phys)
{
  return (void *)(phys + hhdm_offset());
}

/* Convert a kernel virtual (HHDM) pointer back to physical */
static inline uintptr_t virt_to_phys(const void *virt)
{
  return (uintptr_t)virt - hhdm_offset();
}

static inline uintptr_t align_up(uintptr_t a, uintptr_t align)
{
  return (a + align - 1) & ~(align - 1);
}
static inline uintptr_t align_down(uintptr_t a, uintptr_t align)
{
  return a & ~(align - 1);
}

void pmm_init_after_kernel(void)
{
  struct limine_memmap_response *memmap = memmap_req.response;
  if (!memmap)
  {
    // No memmap! kernel can't continue.
    while (1)
    {
      __asm__ __volatile__("hlt");
    }
  }

  /* 1) Determine highest physical address to size bitmap (same as earlier example) */
  uintptr_t max_addr = 0;
  for (uint64_t i = 0; i < memmap->entry_count; i++)
  {
    struct limine_memmap_entry *e = memmap->entries[i];
    uintptr_t top = e->base + e->length;
    if (top > max_addr)
      max_addr = top;
  }
  pmm_total_pages = max_addr / PAGE_SIZE;
  pmm_bitmap_bytes = (pmm_total_pages + 7) / 8;
  pmm_bitmap_pages = (pmm_bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

  /* 2) Find a physical address just after the kernel image for the bitmap.
     _kernel_end is a virtual address if kernel is linked in higher half (typical).
     Convert it to physical via HHDM. */
  uintptr_t kernel_end_virt = (uintptr_t)&_kernel_end;
  uintptr_t kernel_end_phys;

  if (hhdm_req.response)
  {
    /* If kernel is loaded high and HHDM is available, get the phys */
    kernel_end_phys = virt_to_phys((void *)kernel_end_virt);
  }
  else
  {
    /* If no HHDM (rare in this flow) assume identity mapping */
    kernel_end_phys = kernel_end_virt;
  }

  /* Align the bitmap start to page boundary */
  uintptr_t bitmap_phys_candidate = align_up(kernel_end_phys, PAGE_SIZE);

  /* 3) Sanity: ensure the candidate area lies within some USABLE memmap entry.
     If it doesn't, try to find a usable entry that can fit the bitmap. */
  bool fits_in_map = false;
  for (uint64_t i = 0; i < memmap->entry_count; i++)
  {
    struct limine_memmap_entry *e = memmap->entries[i];
    if (e->type != LIMINE_MEMMAP_USABLE)
      continue;
    uintptr_t e_start = e->base;
    uintptr_t e_end = e->base + e->length;
    if (bitmap_phys_candidate >= e_start &&
        (bitmap_phys_candidate + pmm_bitmap_pages * PAGE_SIZE) <= e_end)
    {
      fits_in_map = true;
      break;
    }
  }

  if (!fits_in_map)
  {
    /* fallback: scan memmap and pick the first usable chunk that can fit it */
    uintptr_t found = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++)
    {
      struct limine_memmap_entry *e = memmap->entries[i];
      if (e->type != LIMINE_MEMMAP_USABLE)
        continue;
      uintptr_t e_start = align_up(e->base, PAGE_SIZE);
      uintptr_t e_end = e->base + e->length;
      if ((e_end - e_start) >= pmm_bitmap_pages * PAGE_SIZE)
      {
        found = e_start;
        break;
      }
    }
    if (found == 0)
    {
      /* nowhere to put the bitmap — panic/halt */
      while (1)
      {
        __asm__ __volatile__("hlt");
      }
    }
    bitmap_phys_candidate = found;
  }

  pmm_bitmap_phys = bitmap_phys_candidate;
  /* Map it to kernel virtual via HHDM so we can write it */
  pmm_bitmap = (uint8_t *)phys_to_virt(pmm_bitmap_phys);

  /* 4) Initialize bitmap memory to all 1s (used) then mark usable regions free,
        but also mark the bitmap pages themselves as used so we won't hand them out. */
  memset(pmm_bitmap, 0xFF, pmm_bitmap_pages * PAGE_SIZE); // map may be larger than bitmap_bytes
  // initialize entire bitmap to 1s first
  // but make sure we only consider pmm_bitmap_bytes logically (some bytes in the page may be unused)
  // Next, mark usable ranges as free:
  for (uint64_t i = 0; i < memmap->entry_count; i++)
  {
    struct limine_memmap_entry *e = memmap->entries[i];
    if (e->type == LIMINE_MEMMAP_USABLE)
    {
      uintptr_t start_page = e->base / PAGE_SIZE;
      uintptr_t end_page = (e->base + e->length) / PAGE_SIZE;
      for (uintptr_t p = start_page; p < end_page; p++)
      {
        size_t byte_index = p / 8;
        if (byte_index < pmm_bitmap_bytes)
        {
          pmm_bitmap[byte_index] &= ~(1u << (p & 7));
        }
      }
    }
  }

  /* 5) Mark kernel pages used (so they aren't allocated later) */
  uintptr_t kernel_start_phys = /* if you have symbol for start, use it */ 0;
  // If you don't have start symbol, you can at least mark from 0..kernel_end if kernel is at low memory.
  // Ideally have _kernel_start provided by linker. I'll assume identity: kernel_start_phys = 0x100000;
  // extern char _kernel_start; // optional; if not available, set sane default
  // kernel_start_phys = (uintptr_t)&_kernel_start;
  if (hhdm_req.response)
    kernel_start_phys = virt_to_phys((void *)kernel_start_phys);

  uintptr_t kstart_page = kernel_start_phys / PAGE_SIZE;
  uintptr_t kend_page = align_up(kernel_end_phys, PAGE_SIZE) / PAGE_SIZE;
  for (uintptr_t p = kstart_page; p < kend_page; p++)
  {
    if ((p / 8) < pmm_bitmap_bytes)
      BIT_SET(p);
  }

  /* 6) Mark bitmap pages themselves used in the bitmap */
  uintptr_t bstart = pmm_bitmap_phys / PAGE_SIZE;
  for (uintptr_t p = bstart; p < bstart + pmm_bitmap_pages; p++)
  {
    if ((p / 8) < pmm_bitmap_bytes)
      BIT_SET(p);
  }
}

uintptr_t pmm_alloc_pages(size_t pages)
{
  if (pages == 0)
    return 0;

  size_t run = 0;
  size_t start_page = 0;

  for (size_t i = 0; i < pmm_total_pages; i++)
  {
    if (!BIT_TEST(i))
    {
      if (run == 0)
        start_page = i;
      run++;

      if (run == pages)
      {
        // Mark pages as used
        for (size_t j = start_page; j < start_page + pages; j++)
        {
          BIT_SET(j);
        }
        return start_page * PAGE_SIZE; // return physical address
      }
    }
    else
    {
      run = 0; // reset run
    }
  }

  return 0; // no suitable contiguous run found
}

void pmm_free_pages(uintptr_t phys_addr, size_t pages)
{
  if (pages == 0)
    return;

  size_t start_page = phys_addr / PAGE_SIZE;

  for (size_t i = start_page; i < start_page + pages; i++)
  {
    if (i < pmm_total_pages)
      BIT_CLEAR(i);
  }
}