#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

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

static uint8_t *heap = (uint8_t*)HEAP_START;
static size_t used = 0;

void *malloc(size_t size) {
    if (used + size > HEAP_SIZE)
        return NULL; // out of memory

    void *ptr = heap + used;
    used += size;
    return ptr;
}

void free(void *ptr) {
    // bump allocators can't free, enjoy your leaks.
    (void)ptr;
}

void *realloc(void *ptr, size_t size) {
    // just malloc new space and copy old data if you have memcpy
    void *new_ptr = malloc(size);
    if (!new_ptr) return NULL;
    if (ptr) {
        // TODO: copy old data if you track block sizes
    }
    return new_ptr;
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
