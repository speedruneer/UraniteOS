#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ata.h>
#include <fat.h>
#include <klib.h>

uint8_t buffer[512]; // scratch buffer for one sector

char errorbuffer[4];
uint32_t sector;
uint32_t buffer2;

void ReadBPB(FAT32_BPB *bpb, MBR *mbr, uint8_t partition)
{
    itoa((int)42, (char*)buffer2, 10);
    printf((char*)buffer2);
    switch (partition)
    {
    case 1:
        sector = mbr->part1.lba;
        break;
    case 2:
        sector = mbr->part2.lba;
        break;
    case 3:
        sector = mbr->part3.lba;
        break;
    case 4:
        sector = mbr->part4.lba;
        break;

    default:
        return;
    }
    if (!bpb)
        return;
    if (sector >= (1U << 28))
        return;

    ATA_READ((uint8_t *)bpb, sector, 1);
    return;
}

void ReadMBR(MBR *mbr)
{
    // read the first sector (MBR)
    ATA_READ(buffer, 1, 1);

    // MBR signature check (optional but good)
    if (buffer[510] != 0x55 || buffer[511] != 0xAA)
    {
        return;
    }

    // copy bootloader / UUID part
    memcpy(mbr->uuidstuffidk, buffer, sizeof(mbr->uuidstuffidk));

    // partition table starts at offset 0x1BE
    uint8_t *part_ptr = buffer + 0x1BE;

    memcpy(&mbr->part1, part_ptr, sizeof(Partition));
    memcpy(&mbr->part2, part_ptr + 16, sizeof(Partition));
    memcpy(&mbr->part3, part_ptr + 32, sizeof(Partition));
    memcpy(&mbr->part4, part_ptr + 48, sizeof(Partition));
}