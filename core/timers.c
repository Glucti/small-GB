#include <stdint.h>
#include "timers.h"

void timers_init(Timers_t *timers) {
  *timers = (Timers_t){0};
  timers->TAC = 0;
}

static uint8_t select_tima(uint8_t tac) {
  static uint16_t cycles[4] = {1024, 16, 64, 256};
  return cycles[tac & 0x03];
}

void tick_timers(Timers_t *timers, uint32_t cycles, uint8_t *IF_REG) {
  timers->div_count += cycles;

  while (timers->div_count >= 256) {
    timers->div_count -= 256;
    timers->DIV++;
  }
  
  if (timers->TAC & 0x04) {
    uint16_t period = select_tima(timers->TAC);
    timers->tima_count += period;
    while (timers->tima_count >= period) {
      timers->tima_count -= period;
      uint8_t curr = timers->TIMA; 
      timers->TIMA++;
      if (curr == 0xFF) {
	timers->TIMA = timers->TMA;
	*IF_REG |= 0x04;
      }
    }
  }
}

uint8_t timers_read(Timers_t *t, uint16_t addy) {
  switch(addy) {
    case 0xFF04:
      return t->DIV;
    case 0xFF05:
      return t->TIMA;
    case 0xFF06:
      return t->TMA;
    case 0xFF07:
      return t->TAC & 0x07;
    default:
      return 0xFF;
  }
}

void timers_write(Timers_t *t, uint16_t addy, uint8_t val) {
  switch(addy) {
    case 0xFF04:
      t->DIV = 0;
      t->div_count = 0;
      return;
    case 0xFF05:
      t->TIMA = val;
      return;
    case 0xFF06:
      t->TMA = val;
      return;
    case 0xFF07:
      t->TAC = val & 0x07;
      t->tima_count = 0;
      return;
  }
}



