#include "mailbox.h"

#define MBOX_BASE      0x3F00B880
#define MBOX_READ      ((volatile uint32_t *)(MBOX_BASE + 0x00))
#define MBOX_STATUS    ((volatile uint32_t *)(MBOX_BASE + 0x18))
#define MBOX_WRITE     ((volatile uint32_t *)(MBOX_BASE + 0x20))

#define MBOX_FULL      0x80000000
#define MBOX_EMPTY     0x40000000

int mbox_call(uint8_t ch, volatile uint32_t *mbox) {
    uint32_t addr = ((uint32_t)((uintptr_t)mbox) & ~0xF) | (ch & 0xF);

    // Wait until mailbox is not full
    while (*MBOX_STATUS & MBOX_FULL);

    // Write address of message + channel to mailbox
    *MBOX_WRITE = addr;

    // Wait for response
    while (1) {
        // Wait for message
        while (*MBOX_STATUS & MBOX_EMPTY);
        uint32_t resp = *MBOX_READ;
        if ((resp & 0xF) == ch && (resp & ~0xF) == ((uint32_t)((uintptr_t)mbox) & ~0xF)) {
            // Check if response is successful
            return mbox[1] == 0x80000000;
        }
    }
}