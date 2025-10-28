#include <stdio.h>
#include <stdlib.h>
#include "cpu.h"
#include "memory.h"

int main(int argc, char *argv[]) {
#ifdef TESTING
    run_tests();
#endif

    if (argc < 2) {
      fprintf(stderr, "LOL\n");
      return 1;
    }

    Bus_t *bus = (Bus_t*)malloc(sizeof(Bus_t));

    if (!bus) {
      perror("malloc fail");
      return 1;
    }

    init_bus(bus);
    if (bus_load_rom(bus, argv[1]) != 0) return 1;

    registers_t cpu; 
    RESET_CPU(&cpu);
    cpu.bus = bus;
    unsigned long long max_cycles = 50000000000ULL;

    for (;;) {
      if (cpu.cycle > max_cycles) exit(1);
      helper(&cpu);
  }
}
