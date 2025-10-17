#include <asm.h>
#include <ata.h>
#include <stdio.h>
#include <klib.h>

void kentry() {
    clear();
    printf("[KERNEL LOADED]");
    while(1) {};
    return;
}