#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ppu.h"
#include "memory.h"
#include "mbc.h"
#include "logging.h"


uint32_t bw_palette[4] = {
    0xC4CFA1, 0x8B956D, 0x4D533C, 0x1F1F1F
};

void start_display(Ppu_t *display, Bus_t *bus, int scale) {
  memset(display, 0, sizeof(Ppu_t));
  display->bus = bus;

  display->LCDC = 0x91;
  display->SCY = 0;
  display->SCX = 0;
  display->LY = 0;
  display->LYC = 0;
  display->BGP = 0xFC;
  display->OBP0 = 0xFF;
  display->OBP1 = 0xFF;
  display->WY = 0;
  display->WX = 0;

  for (int i = 0; i <= 3; i++) {
    display->pallete[i] = bw_palette[i];
  }

  display->framebuffer = (uint32_t*)calloc(GB_WIDTH*GB_HEIGHT, 4);
  display->temp_framebuffer = (uint32_t*)calloc(GB_WIDTH*GB_HEIGHT, 4);
  display->background_buffer = (uint32_t*)calloc(256*256, 4);
  if (scale <= 1) {
    display->scaled_framebuffer = display->framebuffer;
  } else {
    display->scaled_framebuffer = (uint32_t*)calloc(GB_WIDTH*GB_HEIGHT, 4*scale*scale);
  }
}

static void render_bg_scanline(Ppu_t *d) {
  if (!(d->LCDC & 0x01))
    return; // BG display enable

  uint16_t tile_data_addr = (d->LCDC & 0x10) ? 0x8000 : 0x8800;
  uint16_t bg_map_addr = (d->LCDC & 0x08) ? 0x9C00 : 0x9800;

  int y = (d->SCY + d->LY) & 0xFF;

  for (int x = 0; x < GB_WIDTH; x++) {
    int scx = (d->SCX + x) & 0xFF;
    uint16_t map_index = bg_map_addr + ((y / 8) * 32) + (scx / 8);
    uint8_t tile_num = read_byte_bus(d->bus, map_index);

    uint16_t tile_addr;
    if (d->LCDC & 0x10)
      tile_addr = tile_data_addr + (tile_num * 16);
    else
      tile_addr = tile_data_addr + ((int8_t)tile_num + 128) * 16;

    int line = y % 8;
    uint8_t low = read_byte_bus(d->bus, tile_addr + (line * 2));
    uint8_t high = read_byte_bus(d->bus, tile_addr + (line * 2) + 1);

    int bit = 7 - (scx % 8);
    int color_id = ((high >> bit) & 1) << 1 | ((low >> bit) & 1);

    uint32_t color = d->pallete[(d->BGP >> (color_id * 2)) & 3];
    d->framebuffer[d->LY * GB_WIDTH + x] = 0xFF000000 | color;
  }
}

static void render_window_scanline(Ppu_t *d) {
  if (!(d->LCDC & 0x20))
    return;

  if (d->WY > d->LY)
    return;

  uint16_t tile_data_addr = (d->LCDC & 0x10) ? 0x8000 : 0x8800; uint16_t win_map_addr = (d->LCDC & 0x40) ? 0x9C00 : 0x9800;
  // window line counter (relative to WY)
  int win_y = d->LY - d->WY;

  for (int x = 0; x < GB_WIDTH; x++) {
    // window X position (offset by 7)
    int win_x = x - (d->WX - 7);

    if (win_x < 0)
      continue;

    // get tile
    uint16_t map_index = win_map_addr + ((win_y / 8) * 32) + (win_x / 8);
    uint8_t tile_num = read_byte_bus(d->bus, map_index);

    uint16_t tile_addr;
    if (d->LCDC & 0x10)
      tile_addr = tile_data_addr + (tile_num * 16);
    else
      tile_addr = tile_data_addr + ((int8_t)tile_num + 128) * 16;

    int line = win_y % 8;
    uint8_t low = read_byte_bus(d->bus, tile_addr + (line * 2));
    uint8_t high = read_byte_bus(d->bus, tile_addr + (line * 2) + 1);

    int bit = 7 - (win_x % 8);
    int color_id = ((high >> bit) & 1) << 1 | ((low >> bit) & 1);

    uint32_t color = d->pallete[(d->BGP >> (color_id * 2)) & 3];
    d->framebuffer[d->LY * GB_WIDTH + x] = 0xFF000000 | color;
  }
}

static void render_sprites_scanline(Ppu_t *d) {
  if (!(d->LCDC & 0x02))
    return; 

  if (d->dma_active)
    return;

  int sprite_height = (d->LCDC & 0x04) ? 16 : 8;
  int sprites_drawn = 0;
  

  for (int i = 39; i >= 0; i--) {
    int oam_addr = i * 4;
    uint8_t oam_y = d->bus->oam[oam_addr];
    uint8_t oam_x = d->bus->oam[oam_addr + 1];
    uint8_t tile_num = d->bus->oam[oam_addr + 2];
    uint8_t attributes = d->bus->oam[oam_addr + 3];

    //  Y=0 off screen, Y=16 top 
    // Y >= 160 off screen
    if (oam_y == 0 || oam_y >= 160) continue;
    
    int sprite_y = oam_y - 16;
    int sprite_x = oam_x - 8;

    int sprite_top = sprite_y;
    int sprite_bottom = sprite_y + sprite_height;
    
    if (d->LY < sprite_top || d->LY >= sprite_bottom)
      continue;

    if (sprites_drawn >= 10)
      continue;
    
    sprites_drawn++;

    int palette = (attributes & 0x10) ? d->OBP1 : d->OBP0;
    int flip_x = attributes & 0x20;
    int flip_y = attributes & 0x40;
    int priority = attributes & 0x80; 

    int line = d->LY - sprite_y;
    
    if (line < 0 || line >= sprite_height) {
      continue;
    }
    
    if (flip_y)
      line = sprite_height - 1 - line;

    if (sprite_height == 16) {
      tile_num &= 0xFE; 
      if (line >= 8) {
        tile_num |= 0x01;
        line -= 8;
      }
    }

    uint16_t tile_addr = 0x8000 + (tile_num * 16) + (line * 2);
    
    uint8_t low = read_byte_bus(d->bus, tile_addr);
    uint8_t high = read_byte_bus(d->bus, tile_addr + 1);

    for (int px = 0; px < 8; px++) {
      int screen_x = sprite_x + px;

      if (screen_x < 0 || screen_x >= GB_WIDTH)
        continue;

      int bit = flip_x ? px : (7 - px);
      int color_id = ((high >> bit) & 1) << 1 | ((low >> bit) & 1);

      if (color_id == 0)
        continue;

      int fb_index = d->LY * GB_WIDTH + screen_x;
      if (priority) {
        uint32_t bg_pixel = d->framebuffer[fb_index];
        uint32_t bg_color = bg_pixel & 0x00FFFFFF;
        uint32_t bg_color0 = d->pallete[0] & 0x00FFFFFF;
        if (bg_color != bg_color0) {
          continue; 
        }
      }
      int shade = (palette >> (color_id * 2)) & 0x03;
      uint32_t color = d->pallete[shade];
      d->framebuffer[fb_index] = 0xFF000000 | color;
    }
  }
}

void display_cycle(Ppu_t *d, Bus_t *b, int cycles) {
  if (!(d->LCDC & LCDC_ENABLE))
    return;
  // fprintf(stderr, "[LCDC=%02X SCX=%02X SCY=%02X]\n", d->LCDC, d->SCX,
  // d->SCY);

  d->cycles_in_line += cycles;

  if (d->dma_pending) {
    d->dma_pending = false;
    d->dma_active = true;
    d->dma_counter = 0;
    d->dma_source = ((uint16_t)d->DMA) << 8;
  }
  static int dma_cycle_counter = 0;
  
  if (d->dma_active) {
    dma_cycle_counter += cycles;
    
    while (dma_cycle_counter >= 4 && d->dma_counter < 160) {
      dma_cycle_counter -= 4;
      
      uint16_t src_addr = d->dma_source + d->dma_counter;
      uint8_t byte;
      
      if (src_addr < 0x8000) {
        byte = cart_read(b->cartridge, src_addr);
      } else if (src_addr >= 0x8000 && src_addr < 0xA000) {
        byte = b->vram[src_addr - 0x8000];
      } else if (src_addr >= 0xA000 && src_addr < 0xC000) {
        // External RAM (cartridge RAM)
        byte = cart_read(b->cartridge, src_addr);
      } else if (src_addr >= 0xC000 && src_addr < 0xE000) {
        byte = b->wram[src_addr - 0xC000];
      } else if (src_addr >= 0xE000 && src_addr < 0xFE00) {
        byte = b->wram[src_addr - 0xE000];
      } else if (src_addr >= 0xFF80 && src_addr <= 0xFFFE) {
        byte = b->hram[src_addr - 0xFF80];
      } else {
        byte = 0xFF;
      }
      
      b->oam[d->dma_counter] = byte;
      d->dma_counter++;
    }
    
    if (d->dma_counter >= 160) {
      d->dma_active = false;
      d->dma_counter = 0;
      dma_cycle_counter = 0;
    }
  } else {
    dma_cycle_counter = 0;
  }

  if (d->cycles_in_line >= 456) {
    d->cycles_in_line -= 456;
    d->LY++;
    if (d->LY == 0) {
    }
    if (d->LY < 144) {
      uint32_t bg_color = d->pallete[0]; 
      for (int x = 0; x < GB_WIDTH; x++) {
        d->framebuffer[d->LY * GB_WIDTH + x] = 0xFF000000 | bg_color;
      }
      render_bg_scanline(d);
      render_window_scanline(d);
      render_sprites_scanline(d);
    }

    if (d->LY == 144) {
      d->STAT = (d->STAT & ~0x03) | 1; // mode 1 = VBlank
      b->IF |= 0x01;                   // request VBlank interrupt

      if (d->STAT & 0x10) // STAT bit 4 = VBlank interrupt enable
        b->IF |= 0x02;
      d->frame_ready = true;
    } else if (d->LY > 153) {
      d->LY = 0;
      d->STAT = (d->STAT & ~0x03) | 2; 
      if (d->STAT & 0x20)              
        b->IF |= 0x02;
    } else if (d->LY < 144) {
      d->STAT = (d->STAT & ~0x03) | 2;
      if (d->STAT & 0x20)
        b->IF |= 0x02;
    }

    if (d->LY == d->LYC) {
      d->STAT |= 0x04;
      if (d->STAT & 0x40) 
        b->IF |= 0x02;
    } else {
      d->STAT &= ~0x04;
    }
  }

  if (d->LY < 144) {
    if (d->cycles_in_line < 80) {
      // mode 2: OAM scan
      d->STAT = (d->STAT & ~0x03) | 2;
    } else if (d->cycles_in_line < 252) {
      // mode 3: drawing (OAM+VRAM)
      d->STAT = (d->STAT & ~0x03) | 3;
    } else {
      // mode 0: HBlank
      if ((d->STAT & 0x03) != 0) {
        if (d->STAT & 0x08) 
          b->IF |= 0x02;
      }
      d->STAT = (d->STAT & ~0x03) | 0;
    }
  }
}

bool ppu_is_mode2(Ppu_t *ppu) {
  if (!ppu) return false;
  // Mode 2 = OAM scan (STAT bits 0-1 == 2)
  return (ppu->STAT & 0x03) == 2;
}

uint8_t ppu_vram_read(Ppu_t *ppu, uint16_t addr) {
  if (!ppu || !ppu->bus)
    return 0xFF;
  if (addr < 0x8000 || addr > 0x9FFF)
    return 0xFF;

  uint16_t offset = addr - 0x8000;
  uint8_t bank = 0;
  size_t index = (size_t)offset + (size_t)bank * 0x2000u;
  if (index >= 0x2000u) {
    //write_log("[PPU VRAM READ] out-of-range index=%zu addr=0x%04X\n", index, addr);
    return 0xFF;
  }
  //uint8_t v = ppu->bus->vram[index];
  //write_log("[PPU VRAM READ] off=0x%04X indx=%zu VRAM_VALUE=0x%02X\n", offset, index, v);

  return ppu->bus->vram[index];
}

void ppu_vram_write(Ppu_t *ppu, uint16_t addr, uint8_t byte) {
  if (!ppu || !ppu->bus)
    return;
  if (addr < 0x8000 || addr > 0x9FFF)
    return;

  uint16_t offset = addr - 0x8000;
  uint8_t bank = 0;
  size_t index = (size_t)offset + (size_t)bank * 0x2000u;
  if (index >= 0x2000u) {
    //write_log("[PPU VRAM WRITE] out-of-range index=%zu addr=0x%04X\n", index,addr);
    return;
  }

  //write_log("[PPU VRAM WRITE] off=0x%04X indx=%zu addr=0x%04X val=0x%02X\n",offset, index, addr, byte);
  ppu->bus->vram[index] = byte;
}

