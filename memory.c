#include <string.h> 
#include "memory.h"
#include <stdio.h>

void init_bus(Bus_t* b) {
  memset(b, 0, sizeof(*b));
  b->JOYP = 0xFF; // all buttons released
}

int bus_load_rom(Bus_t *bus, const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) { 
    perror("Failed to open ROM"); 
    return -1;
  }
  size_t size = fread(bus->rom, 1, sizeof(bus->rom), f);
  fclose(f);
  if (size == 0) {
    fprintf(stderr, "ROM is empty\n");
    return -1;
  }
  return 0;
}

uint8_t read_byte_bus(Bus_t *bus, uint16_t addy) {
  if (addy < 0x8000) return bus->rom[addy];
  if (addy <= 0x9FFF) return bus->vram[addy - 0x8000];
  if (addy >= 0xA000 && addy <= 0xBFFF) return 0xFF; // cart ram not implemented
  if (addy >= 0xC000 && addy <= 0xDFFF) return bus->wram[addy - 0xC000];
  if (addy >= 0xE000 && addy <= 0xFDFF) return bus->wram[addy - 0xE000];
  if (addy >= 0xFE00 && addy <= 0xFE9F) return bus->oam[addy - 0xFE00];
  if (addy >= 0xFEA0 && addy <= 0xFEFF) return 0xFF;

  switch(addy) {
    case 0xFF00: 
      return bus->JOYP;
    case 0xFF01: 
      return bus->SB;
    case 0xFF02:
      return bus->SC;
    case 0xFF04:
      return bus->DIV;
    case 0xFF05: 
      return bus->TIMA;
    case 0xFF06:
      return bus->TMA;
    case 0xFF07:
      return bus->TAC;
    case 0xFF0F:
      return bus->IF;
    default: break;
  }

  if (addy >= 0xFF80 && addy <= 0xFFFE)
    return bus->hram[addy - 0xFF80];
  if (addy == 0xFFFF) return bus->IE;

  return 0xFF;
}

void write_byte_bus(Bus_t *bus, uint16_t addy, uint8_t val) {
  if (addy < 0x8000) return;
  if (addy <= 0x9FFF) {
    bus->vram[addy - 0x8000] = val;
    return;
  } 
  if (addy >= 0xA000 && addy <= 0xBFFF) return; // cart ram not implemented
  if (addy >= 0xC000 && addy <= 0xDFFF) {
    bus->wram[addy - 0xC000] = val;
    return;
  } 
  if (addy >= 0xE000 && addy <= 0xFDFF) {
    bus->wram[addy - 0xE000] = val;
    return;
  } 
  if (addy >= 0xFE00 && addy <= 0xFE9F) {
   bus->oam[addy - 0xFE00] = val;
   return;
  } 
  if (addy >= 0xFEA0 && addy <= 0xFEFF) return;

  switch(addy) {
    case 0xFF00: 
      bus->JOYP = val; return;
    case 0xFF01: 
      bus->SB = val;
      //fprintf(stderr, "[SB<=%02X '%c']\n", val, (val>=32&&val<127)?val:'.');
      return;
    case 0xFF02:
      bus->SC = val;
      //fprintf(stderr, "[SC<=%02X] PC=%04X\n", val, /* you can’t see cpu here */ 0);
      if (val & 0x80) {
	putchar((char)bus->SB);
	fflush(stdout);
	bus->SC &= ~0x80;
	bus->IF |= 0x08;
      }
      return;
    case 0xFF04:
      bus->DIV = 0; return;
    case 0xFF05: 
      bus->TIMA = val; return;
    case 0xFF06:
      bus->TMA = val; return;
    case 0xFF07:
      bus->TAC = val & 0x07; return;
    case 0xFF0F:
      bus->IF = val & 0x1F; return;
    default: break;
  }

  if (addy >= 0xFF80 && addy <= 0xFFFE) {
    bus->hram[addy - 0xFF80] = val; 
    return;
  }
  if (addy == 0xFFFF) {
    bus->IE = val & 0x1F; return;
  }
}
