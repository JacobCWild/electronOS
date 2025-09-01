#pragma once

#include <stdint.h>

int framebuffer_init();
void fb_putc(int x, int y, char c, uint32_t color);
void fb_puts(const char *s, uint32_t color);