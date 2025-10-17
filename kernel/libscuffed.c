#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <asm.h>

#define VGA_ADDR ((uint16_t*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_COLOR 0x07  // light gray on black

#define HEAP_START 0x100000  // example start of heap (1MB)
#define HEAP_SIZE  0x100000  // 1MB heap size

__attribute__((noreturn))
void __stack_chk_fail(void) {
    while(1) {}
}

__attribute__((noreturn))
void __stack_chk_fail_local(void) {
    __stack_chk_fail();
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *pa = (const unsigned char*)a;
    const unsigned char *pb = (const unsigned char*)b;
    for (size_t i = 0; i < n; ++i) {
        if (pa[i] != pb[i])
            return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

// malloc.c - simple first-fit allocator for IA32 within fixed heap region
// Replace your bump-allocator section with this file's contents.
// Expects HEAP_START and HEAP_SIZE macros to be defined (as in your snippet).

#ifndef HEAP_START
#error "HEAP_START must be defined"
#endif
#ifndef HEAP_SIZE
#error "HEAP_SIZE must be defined"
#endif

// basic alignment for IA32·
#define ALIGN4(x) (((x) + 3) & ~3U)

typedef struct header {
    uint32_t size;          // total size of this block including header (bytes)
    struct header *next;    // pointer to next physical block or NULL
    uint8_t free;           // 1 if free, 0 if allocated
    uint8_t pad[3];         // padding to make header 12 bytes (multiple of 4)
} header_t;

#define HEADER_SIZE (sizeof(header_t))

// heap region (provided by user)
static uint8_t * const _heap_start = (uint8_t *) (uintptr_t) HEAP_START;
static const uint32_t _heap_size = (uint32_t) HEAP_SIZE;
static uint8_t _heap_inited = 0;
static header_t *heap_head = NULL;

// internal helpers

static void heap_init(void) {
    if (_heap_inited) return;
    // place one big free block covering the whole region
    heap_head = (header_t *)_heap_start;
    heap_head->size = _heap_size;
    heap_head->next = NULL;
    heap_head->free = 1;
    _heap_inited = 1;
}

// find a free block (first-fit)
static header_t *find_free_block(uint32_t total_size, header_t **prev_out) {
    header_t *cur = heap_head;
    header_t *prev = NULL;
    while (cur) {
        if (cur->free && cur->size >= total_size) {
            if (prev_out) *prev_out = prev;
            return cur;
        }
        prev = cur;
        cur = cur->next;
    }
    if (prev_out) *prev_out = prev;
    return NULL;
}

// split block into allocated part (size = total_size) and remainder (if enough space)
static void split_block(header_t *block, uint32_t total_size) {
    // only split if remaining space is large enough for a header + 4 bytes payload
    if (block->size >= total_size + HEADER_SIZE + 4) {
        uint8_t *new_block_addr = (uint8_t *)block + total_size;
        header_t *new_block = (header_t *) new_block_addr;
        new_block->size = block->size - total_size;
        new_block->next = block->next;
        new_block->free = 1;
        // shrink current block
        block->size = total_size;
        block->next = new_block;
    }
}

// coalesce block with next if next is free
static void try_coalesce_next(header_t *block) {
    if (!block || !block->next) return;
    if (block->next->free) {
        block->size += block->next->size;
        block->next = block->next->next;
    }
}

// get pointer to payload (user pointer)
static void *header_to_payload(header_t *h) {
    return (void *)((uint8_t *)h + HEADER_SIZE);
}

// get header from payload pointer
static header_t *payload_to_header(void *p) {
    if (!p) return NULL;
    return (header_t *)((uint8_t *)p - HEADER_SIZE);
}

// PUBLIC API

void *malloc(size_t size) {
    if (size == 0) return NULL;
    heap_init();

    uint32_t req_payload = (uint32_t) ALIGN4(size);
    uint32_t total_size = req_payload + (uint32_t)HEADER_SIZE;

    header_t *prev = NULL;
    header_t *blk = find_free_block(total_size, &prev);
    if (!blk) {
        // out of memory within fixed heap
        return NULL;
    }

    // If perfect fit or small leftover, just use block
    split_block(blk, total_size);
    blk->free = 0;

    return header_to_payload(blk);
}

void free(void *ptr) {
    if (!ptr) return;
    header_t *h = payload_to_header(ptr);

    // basic sanity: ensure header lies within heap range
    uint8_t *hb = (uint8_t *)h;
    if (hb < _heap_start || hb >= _heap_start + _heap_size) {
        // invalid pointer for this allocator; ignore quietly
        return;
    }

    h->free = 1;

    // coalesce with next if possible
    try_coalesce_next(h);

    // coalesce with previous by scanning from head (cheap but fine for small heaps)
    header_t *cur = heap_head;
    while (cur && cur->next && cur->next != h) cur = cur->next;
    if (cur && cur->next == h) {
        if (cur->free) {
            cur->size += h->size;
            cur->next = h->next;
            h = cur; // now coalesced region; try also to coalesce with following one
        }
    }

    // also try to coalesce with next of resulting block
    try_coalesce_next(h);
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return NULL; }

    header_t *h = payload_to_header(ptr);
    uint32_t req_payload = (uint32_t) ALIGN4(size);
    uint32_t req_total = req_payload + (uint32_t)HEADER_SIZE;

    // if current block big enough, maybe split
    if (h->size >= req_total) {
        split_block(h, req_total);
        return ptr;
    }

    // try to expand into next free block
    if (h->next && h->next->free) {
        uint32_t combined = h->size + h->next->size;
        if (combined >= req_total) {
            // merge with next
            h->size = combined;
            h->next = h->next->next;
            split_block(h, req_total);
            return ptr;
        }
    }

    // otherwise allocate a new block and copy
    void *newp = malloc(size);
    if (!newp) return NULL;
    // copy the lesser of old payload and new size
    uint32_t old_payload = h->size - HEADER_SIZE;
    uint32_t to_copy = (old_payload < req_payload) ? old_payload : req_payload;
    // use memcpy from your libc (or a small loop if you don't want header include)
    memcpy(newp, ptr, to_copy);
    free(ptr);
    return newp;
}

void *calloc(size_t nmemb, size_t size) {
    // check overflow
    uint64_t total = (uint64_t)nmemb * (uint64_t)size;
    if (total == 0 || total > (uint64_t)0xFFFFFFFFU) return NULL;
    void *p = malloc((size_t)total);
    if (!p) return NULL;
    // zero memory
    uint8_t *b = (uint8_t *)p;
    for (size_t i = 0; i < (size_t)total; ++i) b[i] = 0;
    return p;
}

/* ---------------- STRING ---------------- */

// Return length of a null-terminated string
size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len] != 0) {len++;}
    return len;
}

// Compare two strings. Returns 0 if equal, <0 if a<b, >0 if a>b
int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

// Copy string src to dest. Assumes dest is large enough
char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

// Copy at most n characters from src to dest. Pads with '\0' if needed
char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}

/* ---------------- INTEGER STRING CONVERSIONS ---------------- */

// Convert integer to string (base 10), returns dest
char *itoa(int value, char *dest, int base) {
    if (base < 2 || base > 16) {
        dest[0] = '\0';
        return dest;
    }

    char buf[33]; // Enough for 32-bit int + sign
    int i = 0;
    int is_negative = 0;

    if (value == 0) {
        dest[0] = '0';
        dest[1] = '\0';
        return dest;
    }

    if (value < 0 && base == 10) {
        is_negative = 1;
        value = -value;
    }

    while (value != 0) {
        int rem = value % base;
        buf[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value /= base;
    }

    if (is_negative) buf[i++] = '-';

    // Reverse into dest
    int j;
    for (j = 0; j < i; j++)
        dest[j] = buf[i - j - 1];
    dest[i] = '\0';
    return dest;
}

// Convert string to integer (base 10)
int atoi(const char *str) {
    int res = 0;
    int sign = 1;

    // Skip whitespace
    while (*str == ' ' || *str == '\t' || *str == '\n') str++;

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }

    return sign * res;
}

static int cursor_pos = 0;

static void update_cursor() {
    uint16_t pos = (uint16_t)cursor_pos;

    outb(0x3D4, 0x0F);          // low cursor byte
    outb(0x3D5, (uint8_t)(pos & 0xFF));

    outb(0x3D4, 0x0E);          // high cursor byte
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void putchar(char c) {
    if (c == '\n') {
        cursor_pos += VGA_WIDTH - (cursor_pos % VGA_WIDTH); // move to next line
        if (cursor_pos >= VGA_WIDTH * VGA_HEIGHT) {
            cursor_pos = 0; // wrap around (or scroll)
        }
        update_cursor();
        return;
    }

    VGA_ADDR[cursor_pos] = (VGA_COLOR << 8) | c;
    cursor_pos++;
    if (cursor_pos >= VGA_WIDTH * VGA_HEIGHT) {
        cursor_pos = 0; // wrap around (or scroll)
    }
    update_cursor();
}

void puts(const char* str) {
    while (*str) {
        putchar(*str++);
    }
    putchar('\n'); // automatic newline like standard puts
}

/* LIBsCuffed shows itself up a LOT here */

void printf(const char* s) {
    while (*s) putchar(*s++);
}

void clear() {
    for (int i=0;i<VGA_WIDTH*VGA_HEIGHT;i++) {
        putchar(' ');
    }
    cursor_pos = 0;
}