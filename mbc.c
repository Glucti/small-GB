#include "mbc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

mbc_t get_cartridge_type(uint8_t type) {
  switch(type) {
    case 0x00: 
      return MBC_0;
    case 0x01:
    case 0x02:
    case 0x03:
      return MBC_1;
    case 0x0F:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
      return MBC_3;
    case 0x19:
    case 0x1A:
    case 0x1B:
      return MBC_5;
    default: return MBC_0;
  }
}

size_t get_cartridge_rom_size(uint8_t val) {
  if (val <= 8)
    return (size_t)0x8000u << val; // should work, ignoring legacy
  return 0;
}

size_t get_cartridge_ram_size(uint8_t val) {
  switch(val) {
    case 0x00:
      return 0;
    case 0x02:
      return 8 * 1024;
    case 0x03: 
      return 32 * 1024;
    case 0x04:
      return 128 * 1024;
    case 0x05:
      return 64 * 1024;
    default: return 0;
  }
}

Cartridge_t *load_cart(const char *path) {

  FILE *f = fopen(path, "rb");
  if (!f) { perror("Open ROM"); return NULL; }
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (len <= 0) {
    fclose(f);
    return NULL;
  }
  
  uint8_t *buf = (uint8_t*)malloc((size_t)len);
  if (!buf) {
    fprintf(stderr, "Failed to allocate buffer for cartridge\n");
    fclose(f);
    return NULL;
  }

  Cartridge_t *cart = (Cartridge_t*)calloc(1, sizeof(Cartridge_t));
  if (!cart) {
    fprintf(stderr, "failed to allocate cart\n");
    free(buf);
    return NULL;
  }

  cart->rom = buf;
  cart->rom_size = (size_t)len;

  uint8_t cart_type = buf[0x0147];
  uint8_t rom_size_code = buf[0x148];
  uint8_t ram_size_code = buf[0x149];

  cart->type = get_cartridge_type(cart_type);
  cart->rom_size = get_cartridge_rom_size(rom_size_code);
  
  cart->rom_banks = (uint16_t)(cart->rom_size / 0x4000);
  if (cart->rom_banks == 0) {
    cart->rom_banks = 1;
  }

  cart->ram_size = get_cartridge_ram_size(ram_size_code);
  if (cart->ram_size) {
    cart->ram = (uint8_t*)calloc(1, cart->ram_size);
    cart->ram_banks = (uint8_t)(cart->ram_size / 0x2000);
    if (cart->ram_banks == 0) cart->ram_banks = 1;
  }

  // default
  cart->rom_bank = 1;
  cart->ram_bank = 0;
  cart->mode = 0;
  cart->ram_enable = false;

  return cart;
}

void free_cart(Cartridge_t *cart) {
  if (!cart) return;
  free(cart->ram);
  free(cart->rom);
  free(cart);
}

static inline uint8_t read_mbc0(Cartridge_t *cart, uint16_t addy) {
  if (addy < 0x8000) {
    return (addy <= cart->rom_size) ? cart->rom[addy] : 0xFF;
  }

  if (addy >= 0xA000 && addy <= 0xBFFF) {
    if (!cart->ram || cart->ram_size == 0) return 0xFF;
    size_t offset = (size_t)(addy - 0xA000);
    return (offset < cart->ram_size) ? cart->ram[offset] : 0xFF;
  }
  return 0xFF;
}

static inline void write_mbc0(Cartridge_t *cart, uint16_t addy, uint8_t val) {
  if (addy >= 0xA000 && addy <= 0xBFFF && cart->ram && cart->ram_size) {
    size_t offset = (size_t)(addy - 0xA000);
    cart->ram[offset] = val;
  }
}

static inline uint8_t read_mbc_bytes(Cartridge_t *cart, uint32_t bank_num, uint16_t addy) {
  // skip legacy
  bank_num %= cart->rom_banks;

  uint32_t base = bank_num * 0x4000u;
  uint32_t offset = base + (addy & 0x3FFF);

  return (offset < cart->rom_size) ? cart->rom[offset] : 0xFF;
}


