//#define LUA_IMPL
//#include <minilua.h>
#include <asm.h>
#include <ata.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <klib.h>

void kentry() {
    clear();
    printf("[KERNEL LOADED]");
    while(1) {};
    return;
}