// ata.h
#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include <asm.h>

// ATA I/O ports
#define ATA_DATA      0x1F0
#define ATA_ERROR     0x1F1
#define ATA_SECCOUNT  0x1F2
#define ATA_LBA_LOW   0x1F3
#define ATA_LBA_MID   0x1F4
#define ATA_LBA_HIGH  0x1F5
#define ATA_DRIVE     0x1F6
#define ATA_COMMAND   0x1F7

// Read 'count' sectors starting at 'lba' into 'buffer'
void ATA_READ(uint8_t* buffer, uint32_t lba, uint8_t count);

// Write 'count' sectors starting at 'lba' from 'source'
void ATA_WRITE(uint8_t* source, uint32_t lba, uint8_t count);


#endif // ATA_H
