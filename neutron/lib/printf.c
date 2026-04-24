/* ===========================================================================
 * Neutron Kernel — Minimal printf Implementation
 * ===========================================================================
 * Supports: %d, %u, %x, %X, %p, %s, %c, %%, %ld, %lu, %lx, %lX
 * Also supports zero-padding and width specifiers (e.g., %08x).
 * =========================================================================== */

#include <neutron/console.h>
#include <neutron/string.h>

typedef __builtin_va_list va_list;
#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_end(ap)          __builtin_va_end(ap)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)

/* ---- Internal helpers --------------------------------------------------- */

static int print_char(char c)
{
    console_putc(c);
    return 1;
}

static int print_str(const char *s)
{
    int count = 0;
    if (!s) s = "(null)";
    while (*s) {
        console_putc(*s++);
        count++;
    }
    return count;
}

static int print_unsigned(u64 val, int base, int uppercase, int width,
                          char pad)
{
    char buf[24];
    const char *digits = uppercase ? "0123456789ABCDEF"
                                   : "0123456789abcdef";
    int pos = 0;
    int count = 0;

    if (val == 0) {
        buf[pos++] = '0';
    } else {
        while (val > 0) {
            buf[pos++] = digits[val % base];
            val /= base;
        }
    }

    /* Pad */
    while (pos < width) {
        console_putc(pad);
        count++;
        width--;
    }

    /* Print digits in reverse */
    while (pos > 0) {
        console_putc(buf[--pos]);
        count++;
    }

    return count;
}

static int print_signed(s64 val, int width, char pad)
{
    int count = 0;
    if (val < 0) {
        console_putc('-');
        count++;
        val = -val;
        if (width > 0) width--;
    }
    count += print_unsigned((u64)val, 10, 0, width, pad);
    return count;
}

/* ---- Public API --------------------------------------------------------- */

static int kvprintf(const char *fmt, va_list ap)
{
    int count = 0;

    while (*fmt) {
        if (*fmt != '%') {
            count += print_char(*fmt++);
            continue;
        }

        fmt++; /* skip '%' */

        /* Parse flags */
        char pad = ' ';
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }

        /* Parse width */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Parse length modifier */
        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
            if (*fmt == 'l') {
                fmt++; /* 'll' treated same as 'l' on 64-bit */
            }
        }

        /* Parse conversion */
        switch (*fmt) {
        case 'd':
        case 'i':
            if (is_long)
                count += print_signed(va_arg(ap, s64), width, pad);
            else
                count += print_signed((s64)(s32)va_arg(ap, s32), width, pad);
            break;

        case 'u':
            if (is_long)
                count += print_unsigned(va_arg(ap, u64), 10, 0, width, pad);
            else
                count += print_unsigned((u64)(u32)va_arg(ap, u32), 10, 0,
                                        width, pad);
            break;

        case 'x':
            if (is_long)
                count += print_unsigned(va_arg(ap, u64), 16, 0, width, pad);
            else
                count += print_unsigned((u64)(u32)va_arg(ap, u32), 16, 0,
                                        width, pad);
            break;

        case 'X':
            if (is_long)
                count += print_unsigned(va_arg(ap, u64), 16, 1, width, pad);
            else
                count += print_unsigned((u64)(u32)va_arg(ap, u32), 16, 1,
                                        width, pad);
            break;

        case 'p':
            count += print_str("0x");
            count += print_unsigned(va_arg(ap, u64), 16, 0, 16, '0');
            break;

        case 's':
            count += print_str(va_arg(ap, const char *));
            break;

        case 'c':
            count += print_char((char)va_arg(ap, int));
            break;

        case '%':
            count += print_char('%');
            break;

        default:
            count += print_char('%');
            count += print_char(*fmt);
            break;
        }

        fmt++;
    }

    return count;
}

int kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = kvprintf(fmt, ap);
    va_end(ap);
    return ret;
}

void panic(const char *fmt, ...)
{
    /* Disable interrupts */
    __asm__ volatile("msr daifset, #0xf");

    console_puts("\n\n*** NEUTRON KERNEL PANIC ***\n");

    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);

    console_putc('\n');

    /* Halt all execution */
    for (;;)
        __asm__ volatile("wfi");
}
