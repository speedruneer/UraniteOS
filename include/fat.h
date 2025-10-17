#ifndef FAT_H
#define FAT_H

#include <stdint.h>
#include <string.h>
#include <ata.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
extern uint8_t buffer[512];

typedef struct __attribute__((packed))
{
    uint8_t bootable;
    uint8_t chsstuff1[3];
    uint8_t fs_type; // 0x0C = FAT32
    uint8_t chsstuff2[3];
    uint32_t lba;
    uint32_t count;
} Partition;

typedef struct __attribute__((packed))
{
    uint8_t jmpBoot[3];
    uint8_t OEMName[8];
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint16_t reservedSectorCount;
    uint8_t numFATs;
    uint16_t rootEntryCount;
    uint16_t totalSectors16;
    uint8_t media;
    uint16_t FATSize16;
    uint16_t sectorsPerTrack;
    uint16_t numHeads;
    uint32_t hiddenSectors;
    uint32_t totalSectors32;
    uint32_t FATSize32;
    uint16_t extFlags;
    uint16_t fsVersion;
    uint32_t rootCluster;
    uint16_t fsInfo;
    uint16_t backupBootSector;
    uint8_t reserved[12];
    uint8_t driveNumber;
    uint8_t reserved1;
    uint8_t bootSignature;
    uint32_t volumeID;
    uint8_t volumeLabel[11];
    uint8_t fsType[8];
} FAT32_BPB;

typedef struct __attribute__((packed))
{
    uint8_t uuidstuffidk[10];
    Partition part1;
    Partition part2;
    Partition part3;
    Partition part4;
} MBR;

void ReadBPB(FAT32_BPB *bpb, MBR *mbr, uint8_t partition);
void ReadMBR(MBR *mbr);

#endif