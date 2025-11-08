#include <string.h> 
#include <stdio.h>
#include "memory.h"
#include "mbc.h"
#include "ppu.h"
#include "logging.h"

void init_bus(Bus_t* b) {
  memset(b, 0, sizeof(*b));
  timers_init(&b->timers);
  b->JOYP = 0xFF; 
  b->buttons_dir = 0x0F; 
  b->buttons_action = 0x0F;
  
  memset(b->hram, 0x00, sizeof(b->hram));
  
  // just to be safe
  memset(b->oam, 0x00, sizeof(b->oam));
}

int bus_load_rom(Bus_t *bus, const char *path) {
  bus->cartridge = load_cart(path);
  return bus->cartridge ? 0 : 1;
}

uint8_t read_byte_bus(Bus_t *bus, uint16_t addy) {
  if (bus->ppu && bus->ppu->dma_active) {
    if (addy >= 0xFF80 && addy <= 0xFFFE) {
      return bus->hram[addy - 0xFF80];
    }
    if (addy >= 0xFE00 && addy <= 0xFE9F) {
      return 0xFF;
    }
    return 0xFF;
  }
  
  if (addy < 0x0100 && bus->bootrom_enabled && bus->bootrom) {
    uint8_t v = bus->bootrom[addy];
    //fprintf(stderr, "[BOOT READ] %04X -> %02X\n", addy, v);
    return v;
  }
  if (addy < 0x8000) {
    uint8_t v = cart_read(bus->cartridge, addy);
    static int rom_reads = 0;
    if (rom_reads < 80) {
      //fprintf(stderr, "[ROM READ] %04X -> %02X\n", addy, v);
      rom_reads++;
    }
    return v;
  }
  if (addy <= 0x9FFF) {
    if (bus->ppu) return ppu_vram_read(bus->ppu, addy);
    return bus->vram[addy - 0x8000];
  }
  if (addy >= 0xA000 && addy <= 0xBFFF) return cart_read(bus->cartridge, addy); 
  if (addy >= 0xC000 && addy <= 0xDFFF) return bus->wram[addy - 0xC000];
  if (addy >= 0xE000 && addy <= 0xFDFF) return bus->wram[addy - 0xE000];
  if (addy >= 0xFE00 && addy <= 0xFE9F) return bus->oam[addy - 0xFE00];
  if (addy >= 0xFEA0 && addy <= 0xFEFF) return 0xFF;

  switch(addy) {
    case 0xFF00: {
      // JOYP register: bits 7-6 always 1, bits 5-4 are selection, bits 3-0 are button states
      uint8_t select = bus->JOYP & 0x30; 
      uint8_t buttons = 0x0F; 
      
      if ((select & 0x20) == 0) {
        // Action buttons
        buttons = bus->buttons_action & 0x0F;
      }
      if ((select & 0x10) == 0) {
        // Direction buttons
        if ((select & 0x20) == 0) {
          // Both selected
          buttons = (bus->buttons_action & bus->buttons_dir) & 0x0F;
        } else {
          // Only direction
          buttons = bus->buttons_dir & 0x0F;
        }
      }
      return 0xC0 | select | buttons;
    }
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
    case 0xFF40: return bus->ppu->LCDC;
    case 0xFF41: return bus->ppu->STAT;
    case 0xFF42: return bus->ppu->SCY;
    case 0xFF43: return bus->ppu->SCX;
    case 0xFF44: return bus->ppu->LY;
    case 0xFF45: return bus->ppu->LYC;
    case 0xFF46: return bus->ppu->DMA;
    case 0xFF47: return bus->ppu->BGP;
    case 0xFF48: return bus->ppu->OBP0;
    case 0xFF49: return bus->ppu->OBP1;
    case 0xFF4A: return bus->ppu->WY;
    case 0xFF4B: return bus->ppu->WX;
    default: break;
  }

  if (addy >= 0xFF80 && addy <= 0xFFFE)
    return bus->hram[addy - 0xFF80];
  if (addy == 0xFFFF) return bus->IE;

  return 0xFF;
}

void write_byte_bus(Bus_t *bus, uint16_t addy, uint8_t val) {
  if (bus->ppu && bus->ppu->dma_active) {
    if (addy >= 0xFF80 && addy <= 0xFFFE) {
      bus->hram[addy - 0xFF80] = val;
    }
    return;
  }

  if (addy < 0x8000) {
    cart_write(bus->cartridge, addy, val);
    return;
  };
  if (addy <= 0x9FFF) {
    if (bus->ppu) {
      //fprintf(stderr, "[WRITE -> VRAM] ADDR=0x%04X VAL=0x%02X\n", addy, val);
      ppu_vram_write(bus->ppu, addy, val);
      return;
    }
    //fprintf(stderr, "[WRITE -> VRAM (direct)] ADDR=0x%04X VAL=0x%02X\n", addy,
     //       val);
    bus->vram[addy - 0x8000] = val;
    return;
  }

  if (addy >= 0xA000 && addy <= 0xBFFF) {
    cart_write(bus->cartridge, addy, val);
    return;
  }
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
      bus->JOYP = (bus->JOYP & 0xCF) | (val & 0x30);
      return;
    case 0xFF01: 
      bus->SB = val;
      return;
    case 0xFF02:
      bus->SC = val;
      if (val & 0x80) {
	putchar((char)bus->SB);
	fflush(stdout);
	bus->SC &= ~0x80;
	bus->IF |= 0x08;
      }
      return;
    case 0xFF04: case 0xFF05: case 0xFF06:
    case 0xFF07:
      timers_write(&bus->timers, addy, val);
      return;
    case 0xFF0F:
      bus->IF = (bus->IF & ~0x1F) | (val & 0x1F); 
      return;
    case 0xFF40: {
      uint8_t old_lcdc = bus->ppu->LCDC;
      bus->ppu->LCDC = val;
      
      bool was_enabled = (old_lcdc & 0x80) != 0;
      bool is_enabled = (val & 0x80) != 0;
      
      if (!was_enabled && is_enabled) {
        bus->ppu->LY = 0;
        bus->ppu->cycles_in_line = 0;
        bus->ppu->STAT = (bus->ppu->STAT & ~0x03) | 2; // Start in mode 2 (OAM scan)
      } else if (was_enabled && !is_enabled) {
        bus->ppu->LY = 0;
        bus->ppu->cycles_in_line = 0;
        bus->ppu->STAT = (bus->ppu->STAT & ~0x03) | 0; // Mode 0
      }
      return;
    }
    case 0xFF41: bus->ppu->STAT = (val & 0x78) | (bus->ppu->STAT & 0x07); return;
    case 0xFF42: bus->ppu->SCY = val; return;
    case 0xFF43: bus->ppu->SCX = val; return;
    case 0xFF44: return; // LY is read only
    case 0xFF45: bus->ppu->LYC = val; return;
    case 0xFF46:
      bus->ppu->DMA = val;
      bus->ppu->dma_pending = true;
      return;
    case 0xFF47: bus->ppu->BGP = val; return;
    case 0xFF48: bus->ppu->OBP0 = val; return;
    case 0xFF49: bus->ppu->OBP1 = val; return;
    case 0xFF50:
      if (bus->bootrom_enabled) {
        bus->bootrom_enabled = false;
        fprintf(stderr,
                "[BOOT] bootrom disabled via write to 0xFF50, PC=%04X\n",
                bus->ppu ? 0 : 0);
      }
      return;
    case 0xFF4A: bus->ppu->WY = val; return;
    case 0xFF4B: bus->ppu->WX = val; return;
    default: break;
  }

  if (addy >= 0xFF80 && addy <= 0xFFFE) {
    bus->hram[addy - 0xFF80] = val; 
    return;
  }
  if (addy == 0xFFFF) {
    uint8_t old_IE = bus->IE;
    bus->IE = (val & 0x1F);
    static int ie_write_count = 0;
    if (ie_write_count < 20 || (old_IE & 0x10) != (bus->IE & 0x10)) {
      const char *enabled = "";
      if (bus->IE & 0x01) enabled = " VBLANK";
      if (bus->IE & 0x02) enabled = " STAT";
      if (bus->IE & 0x04) enabled = " TIMER";
      if (bus->IE & 0x08) enabled = " SERIAL";
      if (bus->IE & 0x10) enabled = " JOYPAD";
      write_log("[IE WRITE] #%d | old=%02X new=%02X%s | IF=%02X\n",
                ie_write_count, old_IE, bus->IE, enabled, bus->IF);
    }
    ie_write_count++; 
    return;
  }
}
