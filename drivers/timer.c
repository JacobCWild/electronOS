#include "timer.h"

volatile uint32_t *timer_base = (uint32_t *)0x3F003000; // Base address for the timer

void timer_init() {
    // Disable the timer
    timer_base[1] = 0;

    // Set the timer load value
    timer_base[0] = 0xFFFFFFFF; // Load maximum value for a 32-bit timer

    // Enable the timer
    timer_base[1] = 0x00000001; // Timer control register
}

void timer_delay(uint32_t delay) {
    // Load the timer with the delay value
    timer_base[0] = delay;

    // Wait until the timer reaches zero
    while (timer_base[1] & 0x00000001) {
        // Busy wait
    }
}

uint32_t timer_get_current_value() {
    return timer_base[0]; // Return the current value of the timer
}