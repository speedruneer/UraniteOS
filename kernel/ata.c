// ata_safe.c
#include <stdint.h>
#include "asm.h"

// ATA ports for primary channel
#define ATA_DATA       0x1F0
#define ATA_ERROR      0x1F1
#define ATA_SECCOUNT   0x1F2
#define ATA_LBA_LOW    0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HIGH   0x1F5
#define ATA_DRIVE      0x1F6
#define ATA_STATUS     0x1F7
#define ATA_COMMAND    0x1F7
#define ATA_ALTSTATUS  0x3F6

// Status bits
#define ATA_SR_ERR  0x01
#define ATA_SR_DRQ  0x08
#define ATA_SR_DF   0x20
#define ATA_SR_BSY  0x80

// Error codes returned by functions (negative for failures)
#define ATA_OK               0
#define ATA_ERR_TIMEOUT     -1
#define ATA_ERR_DEV         -2
#define ATA_ERR_WRITE_FAIL  -3
#define ATA_ERR_READ_FAIL   -4

// Small I/O delay: historically read port 0x80 or alt-status several times.
// Here we read alternate status a few times for ~400ns-ish delay.
static inline void io_wait(void) {
    (void)inb(ATA_ALTSTATUS);
    (void)inb(ATA_ALTSTATUS);
    (void)inb(ATA_ALTSTATUS);
    (void)inb(ATA_ALTSTATUS);
}

// Poll status until not BSY and optionally until DRQ set. timeout is loop iterations.
static int ata_poll(uint32_t timeout_loops, int wait_for_drq) {
    uint32_t timeout = timeout_loops;
    while (timeout--) {
        uint8_t st = inb(ATA_STATUS);
        if (!(st & ATA_SR_BSY)) {
            if (st & ATA_SR_ERR) return ATA_ERR_DEV;
            if (st & ATA_SR_DF ) return ATA_ERR_DEV;
            if (wait_for_drq) {
                if (st & ATA_SR_DRQ) return ATA_OK;
            } else {
                return ATA_OK;
            }
        }
        io_wait();
    }
    return ATA_ERR_TIMEOUT;
}

// Write 'count' sectors (count: 1..255; 0 => 256) from 'source' to LBA
int ATA_WRITE(const void *source, uint32_t lba, uint8_t count) {
    if (count == 0) count = 0; // caller knows semantics; ATA treats 0 as 256

    // wait until controller ready
    if (ata_poll(1000000, 0) != ATA_OK) return ATA_ERR_TIMEOUT;

    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F)); // LBA, master, top LBA nibble
    io_wait();
    outb(ATA_SECCOUNT, count);
    outb(ATA_LBA_LOW,  lba & 0xFF);
    outb(ATA_LBA_MID,  (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    io_wait();

    outb(ATA_COMMAND, 0x30);  // WRITE SECTORS (28-bit LBA)
    io_wait();

    for (uint32_t i = 0; i < (count ? count : 256); ++i) {
        int r = ata_poll(1000000, 1); // wait for DRQ
        if (r != ATA_OK) return (r == ATA_ERR_TIMEOUT) ? ATA_ERR_TIMEOUT : ATA_ERR_WRITE_FAIL;

        // write 256 words (512 bytes)
        const uint16_t *words = (const uint16_t *)((const uint8_t *)source + (i * 512));
        for (uint32_t w = 0; w < 256; ++w) {
            outw(ATA_DATA, words[w]);
        }
    }

    // Optional: send cache flush (0xE7) and poll for completion to ensure data persists
    outb(ATA_COMMAND, 0xE7); // FLUSH CACHE
    if (ata_poll(1000000, 0) != ATA_OK) return ATA_ERR_TIMEOUT;

    return ATA_OK;
}

// Read 'count' sectors (count: 1..255; 0 => 256) into 'buffer' from LBA
int ATA_READ(void *buffer, uint32_t lba, uint8_t count) {
    if (count == 0) count = 0; // ATA semantics: 0 -> 256

    if (ata_poll(1000000, 0) != ATA_OK) return ATA_ERR_TIMEOUT;

    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    io_wait();
    outb(ATA_SECCOUNT, count);
    outb(ATA_LBA_LOW,  lba & 0xFF);
    outb(ATA_LBA_MID,  (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    io_wait();

    outb(ATA_COMMAND, 0x20);  // READ SECTORS
    io_wait();

    for (uint32_t i = 0; i < (count ? count : 256); ++i) {
        int r = ata_poll(1000000, 1); // wait for DRQ
        if (r != ATA_OK) return (r == ATA_ERR_TIMEOUT) ? ATA_ERR_TIMEOUT : ATA_ERR_READ_FAIL;

        uint16_t *words = (uint16_t *)((uint8_t *)buffer + (i * 512));
        for (uint32_t w = 0; w < 256; ++w) {
            words[w] = inw(ATA_DATA);
        }
    }

    return ATA_OK;
}
