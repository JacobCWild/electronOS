#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(void);
void timer_delay(uint32_t milliseconds);
uint32_t timer_get_time(void);

#endif // TIMER_H