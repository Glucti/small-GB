#pragma once
#include <stdint.h> 

typedef struct Timers {
  uint8_t DIV, TIMA, TMA, TAC;

  uint32_t div_count;
  uint32_t tima_count;
} Timers_t;

void tick_timers(Timers_t *timers, uint32_t cycles, uint8_t *IF_REG);
void timers_init(Timers_t *timers);
uint8_t timers_read(Timers_t *t, uint16_t addy);
void timers_write(Timers_t *t, uint16_t addy, uint8_t val);
