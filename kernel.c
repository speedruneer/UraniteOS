#include <asm.h>
#include <ata.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <klib.h>
#include <fat.h>

MBR main_mbr;
FAT32_BPB fatbpb;
char lclbuffer[32];

void kentry()
{
    clear();
    ReadMBR(&main_mbr);
    ReadBPB(&fatbpb, &main_mbr, 1);
    itoa((int)main_mbr.part1.bootable, lclbuffer, 2);
    puts(lclbuffer);
    while (1);
    return;
}