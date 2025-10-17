#ifndef KLIB_H
#define KLIB_H
#include <stdio.h>

void clear(void);
char *itoa(int value, char *dest, int base);
int atoi(const char *str);

#endif