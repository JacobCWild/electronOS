/* ===========================================================================
 * Neutron Kernel — PL011 UART Driver
 * ===========================================================================
 * Minimal driver for the ARM PL011 UART used by QEMU virt and RPi5.
 * Provides early console output before the full driver framework is up.
 * =========================================================================== */

#include <neutron/console.h>
#include <neutron/arch/aarch64.h>

/* ---- PL011 registers ---------------------------------------------------- */
#define UART_DR         0x000   /* Data register */
#define UART_FR         0x018   /* Flag register */
#define UART_IBRD       0x024   /* Integer baud rate divisor */
#define UART_FBRD       0x028   /* Fractional baud rate divisor */
#define UART_LCRH       0x02C   /* Line control */
#define UART_CR         0x030   /* Control register */
#define UART_IMSC       0x038   /* Interrupt mask */
#define UART_ICR        0x044   /* Interrupt clear */

/* Flag register bits */
#define FR_TXFF         BIT(5)  /* TX FIFO full */
#define FR_RXFE         BIT(4)  /* RX FIFO empty */
#define FR_BUSY         BIT(3)  /* UART busy */

/* Control register bits */
#define CR_UARTEN       BIT(0)  /* UART enable */
#define CR_TXE          BIT(8)  /* TX enable */
#define CR_RXE          BIT(9)  /* RX enable */

/* Line control bits */
#define LCRH_WLEN_8     (3 << 5)  /* 8-bit word length */
#define LCRH_FEN        BIT(4)    /* FIFO enable */

static vaddr_t uart_base = UART0_BASE;

/* ---- Public API --------------------------------------------------------- */

void console_init(void)
{
    /* Disable UART */
    mmio_write32(uart_base + UART_CR, 0);

    /* Clear all interrupts */
    mmio_write32(uart_base + UART_ICR, 0x7FF);

    /* Set baud rate: 115200 @ 24 MHz reference clock
     * IBRD = 24000000 / (16 * 115200) = 13
     * FBRD = ((0.0208 * 64) + 0.5) = 1 */
    mmio_write32(uart_base + UART_IBRD, 13);
    mmio_write32(uart_base + UART_FBRD, 1);

    /* 8N1, FIFO enabled */
    mmio_write32(uart_base + UART_LCRH, LCRH_WLEN_8 | LCRH_FEN);

    /* Disable all interrupts for now */
    mmio_write32(uart_base + UART_IMSC, 0);

    /* Enable UART, TX, RX */
    mmio_write32(uart_base + UART_CR, CR_UARTEN | CR_TXE | CR_RXE);
}

void console_putc(char c)
{
    /* Wait for TX FIFO to have space */
    while (mmio_read32(uart_base + UART_FR) & FR_TXFF)
        ;

    mmio_write32(uart_base + UART_DR, (u32)c);

    /* Also emit \r before \n for proper terminal display */
    if (c == '\n') {
        while (mmio_read32(uart_base + UART_FR) & FR_TXFF)
            ;
        mmio_write32(uart_base + UART_DR, '\r');
    }
}

void console_puts(const char *s)
{
    while (*s)
        console_putc(*s++);
}
