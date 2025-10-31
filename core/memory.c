#include <string.h> 
#include <stdio.h>
#include "memory.h"
#include "mbc.h"

void init_bus(Bus_t* b) {
  memset(b, 0, sizeof(*b));
  timers_init(&b->timers);
  b->JOYP = 0xFF; // all buttons released
}


int bus_load_rom(Bus_t *bus, const char *path) {
  bus->cartridge = load_cart(path);
  return bus->cartridge ? 0 : 1;
}

uint8_t read_byte_bus(Bus_t *bus, uint16_t addy) {
  if (addy < 0x8000) return cart_read(bus->cartridge, addy);
  if (addy <= 0x9FFF) return bus->vram[addy - 0x8000];
  if (addy >= 0xA000 && addy <= 0xBFFF) return cart_read(bus->cartridge, addy); 
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
    case 0xFF05: 
    case 0xFF06:
    case 0xFF07:
      return timers_read(&bus->timers, addy);
    case 0xFF0F:
      return (uint8_t)(0xE0 | (bus->IF & 0x1F));
    default: break;
  }

  if (addy >= 0xFF80 && addy <= 0xFFFE)
    return bus->hram[addy - 0xFF80];
  if (addy == 0xFFFF) return bus->IE;

  return 0xFF;
}

void write_byte_bus(Bus_t *bus, uint16_t addy, uint8_t val) {
  if (addy < 0x8000) {
    cart_write(bus->cartridge, addy, val);
    return;
  };
  if (addy <= 0x9FFF) {
    bus->vram[addy - 0x8000] = val;
    return;
  } 
  if (addy >= 0xA000 && addy <= 0xBFFF) {
    cart_write(bus->cartridge, addy, val);
    return;
  };
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
      //fprintf(stderr, "[SB<=%03X '%c']\n", val, (val>=32&&val<127)?val:'.');
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
    case 0xFF05: 
    case 0xFF06:
    case 0xFF07:
      timers_write(&bus->timers, addy, val);
      return;
    case 0xFF0F:
      bus->IF = (bus->IF & ~0x1F) | (val & 0x1F); 
      //fprintf(stderr, "[IF<=%02X] IF now %02X IE %02X\n", val, bus->IF, bus->IE);
      return;
    default: break;
  }

  if (addy >= 0xFF80 && addy <= 0xFFFE) {
    bus->hram[addy - 0xFF80] = val; 
    return;
  }
  if (addy == 0xFFFF) {
    bus->IE = (val & 0x1F); 
    return;
  }
}
