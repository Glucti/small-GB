#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "memory.h"

typedef uint16_t u16;
typedef uint8_t u8;

#define REGISTER(A, B) 	\
  union { 		\
    struct {		\
      u8 B;		\
      u8 A;		\
    };			\
    u16 A##B;		\
  }			\

typedef struct {

  Bus_t *bus;
  u8 mem[0x10000];

  u8 A; 

  REGISTER(B, C);
  REGISTER(D, E); 
  REGISTER(H, L); 

  u16 SP; 
  u16 PC; 

  struct {bool Z, N, H, C;} F;

  unsigned long cycle;
  
  bool stopped;
  bool halt;
  bool IME;
  bool ime_pending;

} registers_t; 

void cpu_go(registers_t *cpu);
void RESET_CPU(registers_t *cpu);
void halt_wake(registers_t *cpu);
int service_interrupt(registers_t *cpu);
u8 fetch8(registers_t *cpu);
void stop_wake(registers_t *cpu);
u16 fetch16(registers_t *cpu);
void helper(registers_t *cpu);




