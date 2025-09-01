#include <stdint.h>
#include "framebuffer.h"

volatile unsigned int __attribute__((aligned(16))) mbox[36];

unsigned int width = 1024;   // Set your desired width
unsigned int height = 768;   // Set your desired height
unsigned int pitch;
unsigned int isrgb;          // RGB order
unsigned char *fb;

int framebuffer_init() {
    mbox[0] = 35 * 4;
    mbox[1] = 0;
    mbox[2] = 0x48003; mbox[3] = 8; mbox[4] = 8; mbox[5] = width; // width
    mbox[6] = 0x48004; mbox[7] = 8; mbox[8] = 8; mbox[9] = height; // height
    mbox[10] = 0x48009; mbox[11] = 8; mbox[12] = 8; mbox[13] = 32; // depth
    mbox[14] = 0x40001; mbox[15] = 8; mbox[16] = 8; mbox[17] = 0;  // allocate buffer
    mbox[18] = 0x40008; mbox[19] = 8; mbox[20] = 8; mbox[21] = 0;  // pitch
    mbox[22] = 0; // end tag

    if (mbox_call(8, mbox)) {
        fb = (unsigned char *)((unsigned long)mbox[17]);
        pitch = mbox[21];
        isrgb = 1; // Assume RGB
        return 0;
    }
    return -1;
}