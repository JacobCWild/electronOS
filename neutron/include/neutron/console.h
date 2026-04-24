/* ===========================================================================
 * Neutron Kernel — Console / Early Print
 * =========================================================================== */

#ifndef NEUTRON_CONSOLE_H
#define NEUTRON_CONSOLE_H

#include <neutron/types.h>

/* Early UART character output (arch-specific) */
void console_putc(char c);
void console_puts(const char *s);
void console_init(void);

/* Formatted printing */
int kprintf(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

/* Panic — prints message and halts */
void panic(const char *fmt, ...) NORETURN
    __attribute__((format(printf, 1, 2)));

#endif /* NEUTRON_CONSOLE_H */
