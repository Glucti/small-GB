#pragma once
#include <stdint.h>
#include <stdbool.h>

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

} registers_t; 

void cpu_go(registers_t *cpu);




