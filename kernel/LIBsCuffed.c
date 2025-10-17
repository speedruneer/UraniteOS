/* This file is named libscuffed.c, it is an implementation of libc for UraniteOS */

#include <asm.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#define VGA_ADDR ((uint16_t*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_COLOR 0x07  // light gray on black

#define HEAP_START 0x100  // 1MB start
#define HEAP_SIZE  0x1000  // 1MB heap

__attribute__((noreturn))
void __stack_chk_fail(void) { while(1); }

__attribute__((noreturn))
void __stack_chk_fail_local(void) { __stack_chk_fail(); }

// ----------------------
// Memory functions
// ----------------------
void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *pa = (const unsigned char*)a;
    const unsigned char *pb = (const unsigned char*)b;
    for (size_t i = 0; i < n; ++i) if (pa[i] != pb[i]) return (int)pa[i] - (int)pb[i];
    return 0;
}

// ----------------------
// Simple kernel allocator
// ----------------------
#define ALIGN4(x) (((x) + 3) & ~3U)
typedef struct header {
    uint32_t size;          // block size including header
    struct header *next;
    uint8_t free;
    uint8_t pad[3];
} header_t;
#define HEADER_SIZE (sizeof(header_t))

static uint8_t * const _heap_start = (uint8_t *)HEAP_START;
static const uint32_t _heap_size = HEAP_SIZE;
static header_t *heap_head = NULL;

static void heap_init(void) {
    if (heap_head) return;
    heap_head = (header_t *)_heap_start;
    heap_head->size = _heap_size;
    heap_head->next = NULL;
    heap_head->free = 1;
}

static void split_block(header_t *b, uint32_t total_size) {
    if (b->size < total_size + HEADER_SIZE + 4) return;
    header_t *newb = (header_t *)((uint8_t *)b + total_size);
    newb->size = b->size - total_size;
    newb->next = b->next;
    newb->free = 1;
    b->size = total_size;
    b->next = newb;
}

static void try_coalesce_next(header_t *b) {
    if (!b || !b->next) return;
    if (b->next->free) {
        b->size += b->next->size;
        b->next = b->next->next;
    }
}

static header_t *find_free_block(uint32_t total_size) {
    header_t *cur = heap_head;
    while (cur) {
        if (cur->free && cur->size >= total_size) return cur;
        cur = cur->next;
    }
    return NULL;
}

static void *header_to_payload(header_t *h) { return (void *)((uint8_t *)h + HEADER_SIZE); }
static header_t *payload_to_header(void *p) { if(!p) return NULL; return (header_t *)((uint8_t *)p - HEADER_SIZE); }

void *malloc(size_t size) {
    if (!size) return NULL;
    heap_init();
    uint32_t req = ALIGN4(size);
    uint32_t total = req + HEADER_SIZE;
    header_t *blk = find_free_block(total);
    if (!blk) return NULL;
    split_block(blk, total);
    blk->free = 0;
    return header_to_payload(blk);
}

void free(void *ptr) {
    if (!ptr) return;
    header_t *h = payload_to_header(ptr);
    uint8_t *hb = (uint8_t *)h;
    if (hb < _heap_start || hb >= _heap_start + _heap_size) return;
    h->free = 1;
    try_coalesce_next(h);
    header_t *cur = heap_head;
    while (cur && cur->next && cur->next != h) cur = cur->next;
    if (cur && cur->next == h && cur->free) { cur->size += h->size; cur->next = h->next; try_coalesce_next(cur); }
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size==0) { free(ptr); return NULL; }
    header_t *h = payload_to_header(ptr);
    uint32_t req = ALIGN4(size), total=req+HEADER_SIZE;
    if (h->size >= total) { split_block(h,total); return ptr; }
    if (h->next && h->next->free && h->size+h->next->size>=total) { h->size+=h->next->size; h->next=h->next->next; split_block(h,total); return ptr; }
    void *newp=malloc(size);
    if(!newp) return NULL;
    uint32_t copy_size=(h->size-HEADER_SIZE<req)?h->size-HEADER_SIZE:req;
    uint8_t *src=(uint8_t*)ptr,*dst=(uint8_t*)newp; for(uint32_t i=0;i<copy_size;i++) dst[i]=src[i];
    free(ptr); return newp;
}

void *calloc(size_t nmemb, size_t size) {
    uint64_t total = (uint64_t)nmemb*size;
    if(!total || total>0xFFFFFFFF) return NULL;
    void *p=malloc((size_t)total); if(!p) return NULL;
    uint8_t *b=(uint8_t*)p; for(size_t i=0;i<(size_t)total;i++) b[i]=0;
    return p;
}

// ----------------------
// Strings
// ----------------------
size_t strlen(const char *str) { size_t len=0; while(str[len]) len++; return len; }
int strcmp(const char *a,const char *b){while(*a&&(*a==*b)){a++;b++;}return (unsigned char)*a-(unsigned char)*b;}
char *strcpy(char *d,const char *s){char *r=d;while((*d++=*s++));return r;}
char *strncpy(char *d,const char *s,size_t n){size_t i;for(i=0;i<n&&s[i];i++)d[i]=s[i];for(;i<n;i++)d[i]='\0';return d;}
char *strdup(const char *s){size_t len=strlen(s);char *c=malloc(len+1);if(!c)return NULL;for(size_t i=0;i<=len;i++)c[i]=s[i];return c;}
char *strchr(const char *s,int c){while(*s){if(*s==(char)c)return (char*)s;s++;}if(c==0)return (char*)s;return NULL;}

// ----------------------
// Int <-> string
// ----------------------
char *itoa(int val, char *dest, int base){
    if(base<2||base>16){dest[0]='\0'; return dest;}
    char buf[33];
    int i=0;
    unsigned int uval;

    if(val==0){dest[0]='0'; dest[1]='\0'; return dest;}

    int isneg = (val<0);
    uval = isneg > 0 ? -(unsigned int)val : (unsigned int)val;

    while(uval > 1){
        int r = uval % base;
        buf[i++] = (r>9) ? r-10+'a' : r+'0';
        uval /= base;
    }

    if(isneg) buf[i++]='-';
    for(int j=0;j<i;j++) dest[j]=buf[i-j-1];
    dest[i]='\0';
    return dest;
}
int atoi(const char *s){int r=0,sign=1;while(*s==' '||*s=='\t'||*s=='\n') s++;if(*s=='-'){sign=-1;s++;}else if(*s=='+'){s++;}while(*s>='0'&&*s<='9'){r=r*10+(*s-'0');s++;}return sign*r;}

// ----------------------
// VGA console
// ----------------------
static uint16_t cursor_pos = 0;
static void update_cursor() {
    uint16_t pos = (uint16_t)cursor_pos;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void clear();

void putchar(char c) 
{
    if (c == '\n') {
        cursor_pos = (cursor_pos / VGA_WIDTH + 1) * VGA_WIDTH;
    } else if (c == '\r') {
        cursor_pos -= cursor_pos % VGA_WIDTH;
    } else {
        VGA_ADDR[cursor_pos] = (VGA_COLOR << 8) | c;
        cursor_pos++;
    }

    if (cursor_pos >= VGA_WIDTH * VGA_HEIGHT) {
        memcpy(VGA_ADDR, VGA_ADDR + VGA_WIDTH, (VGA_HEIGHT - 1) * VGA_WIDTH * 2);
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_WIDTH * VGA_HEIGHT; i++)
            VGA_ADDR[i] = (VGA_COLOR << 8) | ' ';
        cursor_pos -= VGA_WIDTH;
    }

    update_cursor();
}

void clear() {
    cursor_pos = 0;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        putchar(' ');
    cursor_pos = 0;
    update_cursor();
}

void puts(const char *s) {
    while (*s) putchar(*s++);
    putchar('\n');
}

// ----------------------
// printf helpers
// ----------------------
static void print_char_dest(char **dest, char c) {
    if (dest) {
        **dest = c;
        (*dest)++;
    } else {
        putchar(c);
    }
}

static void print_str_dest(char **dest, const char *s) {
    if (!s) s = "(null)";
    while (*s) print_char_dest(dest, *s++);
}

static void print_num_dest(char **dest, long num, int base, int is_signed) {
    char buf[32];
    int i = 0;
    unsigned long n = num;

    if (is_signed && num < 0) {
        print_char_dest(dest, '-');
        n = -num;
    }

    if (n == 0) {
        print_char_dest(dest, '0');
        return;
    }

    while (n) {
        int rem = n % base;
        buf[i++] = rem < 10 ? '0' + rem : 'a' + rem - 10;
        n /= base;
    }

    while (i--) print_char_dest(dest, buf[i]);
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
    char *ptr = buf;
    char *end = buf ? buf + size : NULL;

    while (*fmt) {
        if (*fmt != '%') {
            if (!buf) putchar(*fmt);
            else if (ptr && ptr < end) *ptr++ = *fmt;
            fmt++;
            continue;
        }

        fmt++;
        if (!*fmt) break;

        switch (*fmt) {
            case 'c': print_char_dest(&ptr, (char)va_arg(args, int)); break;
            case 's': print_str_dest(&ptr, va_arg(args, char*)); break;
            case 'd':
            case 'i': print_num_dest(&ptr, va_arg(args, int), 10, 1); break;
            case 'u': print_num_dest(&ptr, va_arg(args, unsigned int), 10, 0); break;
            case 'x': case 'X': print_num_dest(&ptr, va_arg(args, unsigned int), 16, 0); break;
            case '%': print_char_dest(&ptr, '%'); break;
            default: print_char_dest(&ptr, *fmt); break;
        }
        fmt++;
    }

    if (ptr && size > 0) {
        if (ptr < end) *ptr = '\0';
        else buf[size - 1] = '\0';
    }

    return ptr ? (int)(ptr - buf) : 0;
}

// ----------------------
// printf wrappers
// ----------------------
int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    return r;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = vsnprintf(buf, 0xFFFFFFF, fmt, args);
    va_end(args);
    return r;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return r;
}