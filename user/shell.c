#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"
#include "framebuffer.h"
#include "font8x8.h"

#define MAX_INPUT_SIZE 1024

extern unsigned char font[128][8]; // 8x8 font

static int cursor_x = 0, cursor_y = 0;
extern int pitch;
extern unsigned char *fb;

void fb_putc(int x, int y, char c, uint32_t color) {
    for (int row = 0; row < 8; row++) {
        unsigned char bits = font[(int)c][row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << (7 - col))) {
                int px = x + col;
                int py = y + row;
                unsigned int pixel_offset = py * pitch + px * 4;
                *(uint32_t *)(fb + pixel_offset) = color;
            }
        }
    }
}

void fb_puts(const char *s, uint32_t color) {
    while (*s) {
        if (*s == '\n') {
            cursor_x = 0;
            cursor_y += 8;
        } else {
            fb_putc(cursor_x, cursor_y, *s, color);
            cursor_x += 8;
        }
        s++;
    }
}

// Simple framebuffer-based input function
int fb_gets(char *buf, int maxlen, uint32_t color) {
    int len = 0;
    while (len < maxlen - 1) {
        char c = get_key(); // You need to implement this for your hardware
        if (c == '\r' || c == '\n') {
            fb_puts("\n", color);
            break;
        } else if (c == 8 || c == 127) { // Backspace
            if (len > 0) {
                len--;
                cursor_x -= 8;
                fb_putc(cursor_x, cursor_y, ' ', color); // Erase character
            }
        } else if (c >= 32 && c <= 126) { // Printable
            buf[len++] = c;
            char s[2] = {c, 0};
            fb_puts(s, color);
        }
    }
    buf[len] = 0;
    return len;
}

void execute_command(char *command) {
    if (strcmp(command, "exit") == 0) {
        fb_puts("Exiting...\n", 0xFFFFFF);
        // exit(0); // Uncomment if you have exit
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "Command not found: %s\n", command);
        fb_puts(buf, 0xFFFFFF);
    }
}

void shell_loop() {
    char input[MAX_INPUT_SIZE];
    while (1) {
        fb_puts("shell> ", 0xFFFFFF);
        fb_gets(input, sizeof(input), 0xFFFFFF);
        execute_command(input);
    }
}

int main() {
    shell_loop();
    return 0;
}