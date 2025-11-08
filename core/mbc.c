#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "mbc.h"


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
    return (size_t)0x8000u << val; 
  switch (val) {
    case 0x52: 
      return 1152 * 1024;
    case 0x53:
      return 1280 * 1024;
    case 0x54:
      return 1536 * 1024;
    default: 
      return 0;
  }
}

size_t get_cartridge_ram_size(uint8_t val) {
  switch(val) {
    case 0x00:
      return 0;
    case 0x01:
      return 2 * 1024;
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
  if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
    fclose(f);
    free(buf);
    return NULL;
  }
  fclose(f);

  Cartridge_t *cart = (Cartridge_t*)calloc(1, sizeof(Cartridge_t));

  if (!cart) {
    fprintf(stderr, "failed to allocate cart\n");
    free(buf);
    return NULL;
  }


  uint8_t cart_type = buf[0x0147];
  uint8_t rom_size_code = buf[0x148];
  uint8_t ram_size_code = buf[0x149];

  size_t file_size = (size_t)len;
  size_t header_size = get_cartridge_rom_size(rom_size_code);

  cart->rom = buf;
  cart->rom_size = file_size;

  cart->type = get_cartridge_type(cart_type);
  
  cart->rom_banks = (uint16_t)(cart->rom_size / 0x4000);
  if (cart->rom_banks == 0) {
    cart->rom_banks = 1;
  }

  if (header_size && header_size != file_size) {
    fprintf(stderr, "[warn] ROM header size (%zu) != file size (%zu)\n"
	,header_size, file_size);
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
  fprintf(stderr,
    "[cart] type=%d (hdr=%02X)  rom_banks=%u  file=%zu bytes  "
    "rom_code=%02X  ram_code=%02X  ram_banks=%u\n",
    cart->type, cart_type,
    cart->rom_banks, cart->rom_size,
    rom_size_code, ram_size_code, cart->ram_banks ? cart->ram_banks : 0
  );
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
    return (addy < cart->rom_size) ? cart->rom[addy] : 0xFF;
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
  //bank_num %= cart->rom_banks;
  uint32_t base = bank_num * 0x4000u;
  uint32_t offset = base + (addy & 0x3FFF);

  return (offset < cart->rom_size) ? cart->rom[offset] : 0xFF;
}

/* --------------- MBC1 --------------- */

uint8_t read_mbc1(Cartridge_t *cart, uint16_t addy) {
  bool large_rom = (cart->rom_banks >= 32); 

  if (addy < 0x4000) {
    if (cart->mode == 1 && large_rom) {
      uint32_t bank = ((uint32_t)(cart->ram_bank & 0x03)) << 5;
      if (bank >= cart->rom_banks) bank %= cart->rom_banks;
      return read_mbc_bytes(cart, bank, addy);
    }
    return read_mbc_bytes(cart, 0, addy);
}


  if (addy >= 0x4000 && addy <= 0x7FFF) {  
    uint32_t low5 = (uint32_t)(cart->rom_bank & 0x1F);
    uint32_t hi2  = large_rom ? (uint32_t)(cart->ram_bank & 0x03) : 0;
    uint32_t bank = (hi2 << 5) | low5;

    if (bank >= cart->rom_banks) bank %= cart->rom_banks;
    if ((bank & 0x1F) == 0) bank |= 1;          

    return read_mbc_bytes(cart, bank, addy);
}

  if (addy >= 0xA000 && addy <= 0xBFFF) {
    if (!cart->ram || !cart->ram_enable) return 0xFF;
    uint32_t bank = 0;
    if (!large_rom && cart->mode == 1) bank = (uint32_t)(cart->ram_bank & 0x03);
    bank %= (cart->ram_banks ? cart->ram_banks : 1);
    size_t off = bank * 0x2000u + (addy - 0xA000);
    return (off < cart->ram_size) ? cart->ram[off] : 0xFF;
  }

  return 0xFF;
}

void write_mbc1(Cartridge_t *cart, uint16_t addy, uint8_t val) {
  bool large_rom = (cart->rom_banks >= 32); // larger than 1MB
  if (addy < 0x2000) {
    cart->ram_enable = ((val & 0x0F) == 0xA);
    //fprintf(stderr, "[MBC1] RAM_ENABLE=%d\n", cart->ram_enable);
    return; 
  }

  if (addy >= 0x2000 && addy <= 0x3FFF) {
    uint8_t low5 = val & 0x1F;
    if (low5 == 0) low5 = 1;                  
    cart->rom_bank = (cart->rom_bank & ~0x1F) | low5;
    //fprintf(stderr, "[MBC1] ROM_BANK lower 5 = %u\n", cart->rom_bank & 0x1F);
    return;
    }
  
  if (addy >= 0x4000 && addy <= 0x5FFF) {
    cart->ram_bank = (val & 0x03);
    return;
  }

  if (addy >= 0x6000 && addy <= 0x7FFF) {
    cart->mode = (val & 0x01);
    //fprintf(stderr, "[MBC1] MODE = %u\n", cart->mode);
    return;
  }

  if (addy >= 0xA000 && addy <= 0xBFFF) {
    if (!cart->ram || !cart->ram_enable) 
      return;

    uint32_t bank = (!large_rom && cart->mode == 1) ? (uint32_t)(cart->ram_bank & 0x03) : 0;
    bank %= (cart->ram_banks ? cart->ram_banks : 1);

    size_t off = bank * 0x2000u + (addy - 0xA000);
    if (off < cart->ram_size) cart->ram[off] = val;
  }
}

uint8_t cart_read(Cartridge_t *cart, uint16_t addy) {
  switch (cart->type) {
    case MBC_0:
      return read_mbc0(cart, addy);
    case MBC_1:
      return read_mbc1(cart, addy);
    default:
      return read_mbc0(cart, addy);
  }
}

void cart_write(Cartridge_t *cart, uint16_t addy, uint8_t val) {
  switch (cart->type) {
    case MBC_0:
      return write_mbc0(cart, addy, val);
    case MBC_1:
      return write_mbc1(cart, addy, val);
    default:
      return write_mbc0(cart, addy, val);
  }
}


