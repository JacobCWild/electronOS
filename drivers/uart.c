#include "uart.h"

#define UART_BASE 0x3F201000  // Base address for UART on Raspberry Pi
#define UART_FR   (UART_BASE + 0x18)  // Flag Register
#define UART_DR   (UART_BASE + 0x00)   // Data Register
#define UART_IBRD (UART_BASE + 0x24)   // Integer Baud Rate Register
#define UART_FBRD (UART_BASE + 0x28)   // Fractional Baud Rate Register
#define UART_LCRH (UART_BASE + 0x2C)   // Line Control Register
#define UART_CR   (UART_BASE + 0x30)   // Control Register
#define UART_IMSC (UART_BASE + 0x38)   // Interrupt Mask Set/Clear Register
#define UART_MIS  (UART_BASE + 0x40)   // Masked Interrupt Status Register
#define UART_ICR  (UART_BASE + 0x44)   // Interrupt Clear Register

void uart_init(unsigned int baudrate) {
    // Disable the UART
    *(volatile unsigned int *)UART_CR = 0;

    // Set the baud rate
    unsigned int ibrd = 48000000 / (16 * baudrate);
    unsigned int fbrd = ((48000000 % (16 * baudrate)) * 64 + (baudrate / 2)) / baudrate;
    *(volatile unsigned int *)UART_IBRD = ibrd;
    *(volatile unsigned int *)UART_FBRD = fbrd;

    // Set the line control register for 8 bits, no parity, 1 stop bit
    *(volatile unsigned int *)UART_LCRH = (3 << 5);

    // Enable the UART, TXE, and RXE
    *(volatile unsigned int *)UART_CR = (1 << 0) | (1 << 8) | (1 << 9);
}

void uart_send(char c) {
    // Wait until the UART is ready to transmit
    while (*(volatile unsigned int *)UART_FR & (1 << 5));
    *(volatile unsigned int *)UART_DR = c;
}

char uart_receive(void) {
    // Wait until there is data to read
    while (*(volatile unsigned int *)UART_FR & (1 << 4));
    return *(volatile unsigned int *)UART_DR;
}