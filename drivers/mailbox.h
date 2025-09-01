#pragma once
#include <stdint.h>

// Mailbox channels
#define MBOX_CH_PROP 8

int mbox_call(uint8_t ch, volatile uint32_t *mbox);