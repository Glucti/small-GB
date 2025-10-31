#include "interrupts.h"
#include "cpu.h"
#include "memory.h"

#define GET_FLAG(num) ( (uint8_t)(1u << ( ((num)-0x40) / 8)) )

#define IF_ADDY 0xFF0F
#define IE_ADDY 0xFFFF


// TODO make a stack.h with needed push pop funcs
static inline void push_16(registers_t *cpu, u16 val) {
  cpu->SP--;
  write_byte_bus(cpu->bus, cpu->SP, (u8)(val >> 8));
  cpu->SP--;
  write_byte_bus(cpu->bus, cpu->SP, (u8)(val & 0xFF));
}

u8 read_interrupt(registers_t *cpu, uint16_t addy) {
  if (addy == IF_ADDY) {
    return (cpu->bus->IF & 0x1F) | 0xE0;
  }
  if (addy == IE_ADDY) {
    return (cpu->bus->IE & 0x1F);
  }
  return 0xFF;
}

void write_interrupt(registers_t *cpu, uint16_t addy, uint8_t val) {
  if (addy == IF_ADDY) {
    cpu->bus->IF = val & 0x1F;
  }
  if (addy == IE_ADDY) {
    cpu->bus->IE = val & 0x1F;
  }
}

void interrupt_req(registers_t *cpu, interrupt_source interrupt) {
  cpu->bus->IF = cpu->bus->IF | GET_FLAG(interrupt);
}

bool interrupt_isset(registers_t *cpu, interrupt_source interrupt) {
  return cpu->bus->IF & GET_FLAG(interrupt);
}

bool interrupt_isEnabled(registers_t *cpu, interrupt_source interrupt) {
  return cpu->bus->IE & GET_FLAG(interrupt);
}

void set_IME(registers_t *cpu, bool val) {
  cpu->IME = val;
}

static inline void handle_interrupt(registers_t *cpu, interrupt_source interrupt) {
  cpu->halt = false;
  cpu->IME = 0;

  cpu->bus->IF &= (uint8_t)~GET_FLAG(interrupt);

  push_16(cpu, cpu->PC);
  cpu->PC = (uint16_t)interrupt;
}

u8 handle_interrupts(registers_t *cpu) {
  for (interrupt_source i = VBLANK; i <= JOYPAD; i+=0x8) {
    if (interrupt_isset(cpu, i) && interrupt_isEnabled(cpu, i)) {
      cpu->halt = false;

      if (!cpu->IME)
	return 0;

      handle_interrupt(cpu, i);

      write_interrupt(cpu, IF_ADDY, read_interrupt(cpu, IF_ADDY) & ~GET_FLAG(i));
      return 20;
    }
  }
  return 0;
}


