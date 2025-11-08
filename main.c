#include <stdio.h>
#include <stdlib.h>
#include "cpu.h"
#include "ppu.h"
#include "memory.h"
#include "logging.h"
#include <SDL2/SDL.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s rom.gb\n", argv[0]);
    return 1;
  }

  Bus_t *bus = malloc(sizeof(Bus_t));
  init_bus(bus);

  FILE *bf = fopen("dmg_boot.bin", "rb");
  if (bf) {
    bus->bootrom = malloc(256);
    if (bus->bootrom && fread(bus->bootrom, 1, 256, bf) == 256) {
      bus->bootrom_enabled = true;
      fprintf(stderr, "[BOOT] loaded dmg_boot.bin\n");
    } else {
      free(bus->bootrom);
      bus->bootrom = NULL;
      bus->bootrom_enabled = false;
      fprintf(stderr, "[BOOT] failed to read dmg_boot.bin\n");
    }
    fclose(bf);
    } else {
      fprintf(stderr, "[BOOT] no dmg_boot.bin (skipping BIOS)\n");
    }

    if (bus_load_rom(bus, argv[1]) != 0)
      return 1;

    if (!bus->cartridge) {
      fprintf(stderr, "[ROM] failed to load '%s'\n", argv[1]);
    } else {
      fprintf(stderr, "[ROM] loaded '%s' size=%zu bytes\n", argv[1],
              bus->cartridge->rom_size);
      fprintf(stderr, "[ROM] header bytes: ");
      for (int i = 0; i < 16; ++i)
        fprintf(stderr, "%02X ", bus->cartridge->rom[i]);
      fprintf(stderr, "\n[ROM] title: ");
      for (int i = 0x0134; i <= 0x0143; ++i) {
        unsigned char c = bus->cartridge->rom[i];
        fprintf(stderr, "%c", (c >= 32 && c < 127) ? c : '.');
    }
    fprintf(stderr, "\n");
    }

    Ppu_t *ppu = malloc(sizeof(Ppu_t));
    start_display(ppu, bus, 4);

    bus->ppu = ppu;

    registers_t cpu;
    RESET_CPU(&cpu);
    cpu.bus = bus;
    cpu.ppu = ppu;

    if (bus->bootrom_enabled && bus->bootrom) {
      cpu.PC = 0x0000; 
      cpu.IME = 0;    
    }

  // sdl
  int scale = 4;
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window *win =
      SDL_CreateWindow("Game Boy", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, GB_WIDTH * scale, GB_HEIGHT * scale, 0);
  SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
  SDL_Texture *tex =
      SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING, GB_WIDTH, GB_HEIGHT);

  bool running = true;
  unsigned long long max_cycles = 5000000000000000ULL;

  bus->buttons_dir = 0x0F;   
  bus->buttons_action = 0x0F; 
  
  // logs
  set_log_file("log.txt");
  write_log("[MAIN] Starting...\n");
  write_log("[MAIN] ROM: %s\n", argv[1]);

  while (running && cpu.cycle < max_cycles) {
    SDL_Event e;
    int events_processed = 0;
    while (SDL_PollEvent(&e)) {
      events_processed++;
      static int total_events = 0;
      total_events++;
      if (total_events <= 10) {
        write_log("[SDL] Event #%d: type=%d\n", total_events, e.type);
      }
      
      if (e.type == SDL_QUIT)
        running = false;
      
      if (e.type == SDL_KEYDOWN) {
        if (!e.key.repeat) {
          write_log("[SDL] Key down: SDL_Keycode=%d, sym=%d, repeat=%d\n", 
                    e.key.keysym.scancode, e.key.keysym.sym, e.key.repeat);
        }
        
        if (!e.key.repeat) {
          uint8_t old_dir = bus->buttons_dir;
          uint8_t old_action = bus->buttons_action;
          const char *key_name = NULL;
        
        // Direction buttons (0=pressed, 1=released)
        if (e.key.keysym.sym == SDLK_RIGHT) {
          bus->buttons_dir &= ~0x01;
          key_name = "RIGHT";
        }
        if (e.key.keysym.sym == SDLK_LEFT) {
          bus->buttons_dir &= ~0x02;
          key_name = "LEFT";
        }
        if (e.key.keysym.sym == SDLK_UP) {
          bus->buttons_dir &= ~0x04;
          key_name = "UP";
        }
        if (e.key.keysym.sym == SDLK_DOWN) {
          bus->buttons_dir &= ~0x08;
          key_name = "DOWN";
        }
        
        if (e.key.keysym.sym == SDLK_x) {
          bus->buttons_action &= ~0x01;
          key_name = "A (X)";
        }
        if (e.key.keysym.sym == SDLK_z) {
          bus->buttons_action &= ~0x02;
          key_name = "B (Z)";
        }
        if (e.key.keysym.sym == SDLK_RSHIFT) {
          bus->buttons_action &= ~0x04;
          key_name = "SELECT (RSHIFT)";
        }
        if (e.key.keysym.sym == SDLK_RETURN) {
          bus->buttons_action &= ~0x08;
          key_name = "START (RETURN)";
        }

        // Trigger joypad interrupt on button press
        if ((bus->buttons_dir != old_dir) || (bus->buttons_action != old_action)) {
          bus->IF |= 0x10; // JOYP interrupt
          write_log(
              "[INPUT] Key pressed: %s | dir=%02X->%02X action=%02X->%02X | "
              "IF=%02X->%02X IE=%02X IME=%d (PC=%04X cycle=%lu)\n",
              key_name ? key_name : "UNKNOWN", old_dir, bus->buttons_dir,
              old_action, bus->buttons_action, bus->IF & ~0x10, bus->IF,
              bus->IE, cpu.IME, cpu.PC, cpu.cycle);
        }
        }
      }
      
      if (e.type == SDL_KEYUP) {
        const char *key_name = NULL;
        uint8_t old_dir = bus->buttons_dir;
        uint8_t old_action = bus->buttons_action;
        
        if (e.key.keysym.sym == SDLK_RIGHT) {
          bus->buttons_dir |= 0x01;
          key_name = "RIGHT";
        }
        if (e.key.keysym.sym == SDLK_LEFT) {
          bus->buttons_dir |= 0x02;
          key_name = "LEFT";
        }
        if (e.key.keysym.sym == SDLK_UP) {
          bus->buttons_dir |= 0x04;
          key_name = "UP";
        }
        if (e.key.keysym.sym == SDLK_DOWN) {
          bus->buttons_dir |= 0x08;
          key_name = "DOWN";
        }
        if (e.key.keysym.sym == SDLK_x) {
          bus->buttons_action |= 0x01;
          key_name = "A (X)";
        }
        if (e.key.keysym.sym == SDLK_z) {
          bus->buttons_action |= 0x02;
          key_name = "B (Z)";
        }
        if (e.key.keysym.sym == SDLK_RSHIFT) {
          bus->buttons_action |= 0x04;
          key_name = "SELECT (RSHIFT)";
        }
        if (e.key.keysym.sym == SDLK_RETURN) {
          bus->buttons_action |= 0x08;
          key_name = "START (RETURN)";
        }
        
        if (key_name && ((bus->buttons_dir != old_dir) || (bus->buttons_action != old_action))) {
          write_log("[INPUT] Key released: %s | dir=%02X action=%02X\n",
                    key_name, bus->buttons_dir, bus->buttons_action);
        }
      }
    }

    helper(&cpu);

    if (ppu->frame_ready) {
      SDL_UpdateTexture(tex, NULL, ppu->framebuffer,
                        GB_WIDTH * sizeof(uint32_t));
      SDL_RenderClear(ren);
      SDL_RenderCopy(ren, tex, NULL, NULL);
      SDL_RenderPresent(ren);

      ppu->frame_ready = false;
    }
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    
    write_log("[MAIN] Emulator shutting down\n");
    close_log_file();

    return 0;
}
