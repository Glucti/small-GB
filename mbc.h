#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
  MBC_0 = 0,
  MBC_1,
  MBC_3, 
  MBC_5
} mbc_t;

typedef struct Cartridge {
  mbc_t type;
  uint8_t *rom;
  size_t rom_size;

  uint8_t *ram; 
  size_t ram_size; 
  bool ram_enable;

  uint8_t rom_bank;
  uint8_t ram_bank;
  uint8_t mode;

  uint16_t rom_banks;
  uint16_t ram_banks;
} Cartridge_t;

Cartridge_t *load_cart(const char *path);
void free_cart(Cartridge_t *cart);

