#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cpu.h"
#include "memory.h"
#include "timers.h"

#define TRACE_LEN 4096
typedef struct {
  u16 pc;
  u8  op;
  u8  a,b,c,d,e,h,l;
  u16 sp, hl, de, bc;
  u8  fz, fn, fh, fc;
  unsigned long cyc;
} Trace;

static Trace tracebuf[TRACE_LEN];
static size_t tracepos = 0;

static inline void trace_log(registers_t *cpu, u8 op) {
  Trace *t = &tracebuf[tracepos++ & (TRACE_LEN-1)];
  t->pc = cpu->PC-1; t->op = op;
  t->a = cpu->A; t->b = cpu->B; t->c = cpu->C;
  t->d = cpu->D; t->e = cpu->E; t->h = cpu->H; t->l = cpu->L;
  t->sp = cpu->SP; t->hl = cpu->HL; t->de = cpu->DE; t->bc = cpu->BC;
  t->fz = cpu->F.Z; t->fn = cpu->F.N; t->fh = cpu->F.H; t->fc = cpu->F.C;
  t->cyc = cpu->cycle;
}

static void trace_dump_last(size_t n) {
  size_t start = (tracepos > n ? tracepos - n : 0);
  for (size_t i = start; i < tracepos; ++i) {
    Trace *t = &tracebuf[i & (TRACE_LEN-1)];
    fprintf(stderr,
      "PC=%04X OP=%02X A=%02X F=%d%d%d%d BC=%04X DE=%04X HL=%04X SP=%04X CYC=%lu\n",
      t->pc, t->op, t->a, t->fz,t->fn,t->fh,t->fc, t->bc, t->de, t->hl, t->sp, t->cyc);
  }
}


#define TICK(cpu, n) do {  \
  (cpu)->cycle += (n);    \
  if (!(cpu)->stopped)    \
    tick_timers(&(cpu)->bus->timers, (n), &(cpu)->bus->IF); \
} while(0)


#define SET_Z(cpu, n) ((cpu)->F.Z = ((n) == 0))
#define SET_N(cpu, n) ((cpu)->F.N = (n))
#define SET_H(cpu, n) ((cpu)->F.H = (n))
#define SET_C(cpu, n) ((cpu)->F.C = (n))


// 8-bit registers
#define REG_B 0
#define REG_C 1
#define REG_D 2
#define REG_E 3
#define REG_H 4
#define REG_L 5
#define REG_HLm 6  // (HL) memory indirect
#define REG_A 7

// 16-bit register pairs
#define REG_BC 0
#define REG_DE 1
#define REG_HL 2
#define REG_SP 3

#define JOYP_IF 0x10


void (*cb_ops[256])(registers_t *cpu);

// helpers
static inline u8 read8(registers_t *cpu, u16 addy) {
  return read_byte_bus(cpu->bus, addy);
}

static inline void write8(registers_t *cpu, u16 addy, u8 val) {
  return write_byte_bus(cpu->bus, addy, val);
}

static inline u16 read16(registers_t *cpu, u16 addy) {
  return bus_read16(cpu->bus, addy);
}

static inline void write16(registers_t *cpu, u16 addy, u16 val) {
  bus_write16(cpu->bus, addy, val);
}

u8 fetch8(registers_t *cpu) {
  return read8(cpu, cpu->PC++);
}

u16 fetch16(registers_t *cpu) {
  u16 val = read16(cpu, cpu->PC);
  cpu->PC += 2; 
  return val;
}

static inline u8 read_reg8(registers_t *cpu, int reg) {
  switch(reg) {
    case REG_B:
      return cpu->B;
    case REG_C:
      return cpu->C; 
    case REG_D:
      return cpu->D; 
    case REG_E:
      return cpu->E;
    case REG_H:
      return cpu->H;
    case REG_L:
      return cpu->L;
    case REG_HLm:
      return read8(cpu, cpu->HL);
    case REG_A:
      return cpu->A;
    default:
      printf("Invalid register %d\n", reg);
      return 0xFF;
  }
}

static inline void write_reg8(registers_t *cpu, int reg, u8 val) {
  switch(reg) {
    case REG_B:
      cpu->B = val; 
      break;
    case REG_C:
      cpu->C = val; 
      break;
    case REG_D:
      cpu->D = val;
      break;
    case REG_E:
      cpu->E = val; 
      break;
    case REG_H:
      cpu->H = val;
      break;
    case REG_L:
      cpu->L = val; 
      break;
    case REG_HLm:
      write8(cpu, cpu->HL, val);
      break;
    case REG_A:
      cpu->A = val; 
      break;
    default:
      printf("Invalid register %d\n", reg);
      break;
  }
}

static inline u16 read_reg16(registers_t *cpu, int reg) {
  switch(reg) {
    case REG_BC:
      return cpu->BC;
    case REG_DE:
      return cpu->DE;
    case REG_HL:
      return cpu->HL;
    case REG_SP:
      return cpu->SP;
    default:
      printf("Invalid register %d\n", reg);
      return 0;
  }
}

static inline void write_reg16(registers_t *cpu, int reg ,u16 val) {
  switch (reg) {
    case REG_BC:
      cpu->BC = val; 
      break;
    case REG_DE:
      cpu->DE = val;
      break;
    case REG_HL:
      cpu->HL = val; 
      break;
    case REG_SP:
      cpu->SP = val;
      break;
    default:
      printf("Invalid register %d\n", reg);
      break;
  }
}



// increments and decrements

static inline void inc_r(registers_t* cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = (opcode >> 3) & 7;

  u8 val = read_reg8(cpu, reg);
  u8 old = val;

  val++;

  SET_Z(cpu, val); 
  SET_N(cpu, 0);
  SET_H(cpu, ((old & 0x0F) + 1) > 0x0F);

  write_reg8(cpu, reg, val);

  TICK(cpu, (reg == REG_HLm) ? 12 : 4);
}


static inline void inc_rr(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = (opcode >> 4) & 3;

  u16 val = read_reg16(cpu, reg);
  val++;
  write_reg16(cpu, reg, val);
  TICK(cpu, 8);
}


static inline void dec_rr(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = (opcode >> 4) & 3;

  u16 val = read_reg16(cpu, reg);
  val--;
  write_reg16(cpu, reg, val);
  TICK(cpu, 8);
}


static inline void dec_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = (opcode >> 3) & 7;

  u8 val = read_reg8(cpu, reg);
  u8 old = val;

  val--;

  SET_Z(cpu, val); 
  SET_N(cpu, 1);
  //SET_H(cpu, ((old & 0x0F) - 1) > 0x0F);
  SET_H(cpu, (old & 0x0F) == 0);

  write_reg8(cpu, reg, val);

  TICK(cpu, (reg == REG_HLm) ? 12 : 4);
}

// loads
static inline void ld_r_immediate(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = (opcode >> 3) & 7;
  u8 val = fetch8(cpu); 

  write_reg8(cpu, reg, val);
  TICK(cpu, (reg == REG_HLm) ? 12 : 8);
}

static inline void ld_rr_immediate(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = (opcode >> 4) & 3;
  u16 val = fetch16(cpu); 

  write_reg16(cpu, reg, val);

  TICK(cpu, 12);
}

static inline void ld_bc_a(registers_t *cpu) {
  u16 bc = read_reg16(cpu, REG_BC);
  u8 a = read_reg8(cpu, REG_A);

  write8(cpu, bc, a);
  TICK(cpu, 8);
}

static inline void ld_a_bc(registers_t *cpu) {
  u16 bc = read_reg16(cpu, REG_BC);
  u8 val = read8(cpu, bc);

  write_reg8(cpu, REG_A, val);
  TICK(cpu, 8);
}

static inline void ld_a_de(registers_t *cpu) {
  u16 de = read_reg16(cpu, REG_DE);
  u8 val = read8(cpu, de);

  write_reg8(cpu, REG_A, val);
  TICK(cpu, 8);
}

static inline void ld_de_a(registers_t *cpu) {
  u16 de = read_reg16(cpu, REG_DE);
  u8 a = read_reg8(cpu, REG_A);

  write8(cpu, de, a);
  TICK(cpu, 8);
}

static inline void ld_a16_sp(registers_t *cpu) {
  u16 nn = fetch16(cpu);
  write8(cpu, nn, cpu->SP & 0xFF);
  write8(cpu, nn + 1, cpu->SP >> 8);
  TICK(cpu, 20);
}

static inline void ld_hlp_a(registers_t *cpu) {
  u8 a = read_reg8(cpu, REG_A);
  u16 hl = read_reg16(cpu, REG_HL);
  hl += 1;
  write8(cpu, cpu->HL, a);
  write_reg16(cpu, REG_HL, hl);
  TICK(cpu, 8);
}

static inline void ld_hlm_a(registers_t *cpu) {
  u8 a = read_reg8(cpu, REG_A);
  u16 hl = read_reg16(cpu, REG_HL);
  hl -= 1;
  write8(cpu, cpu->HL, a);
  write_reg16(cpu, REG_HL, hl);
  TICK(cpu, 8);
}

static inline void ld_a_hlp(registers_t *cpu) {
  u16 hl_addy = read_reg16(cpu, REG_HL);
  u8 hl_val = read8(cpu, hl_addy);

  write_reg8(cpu, REG_A, hl_val);
  write_reg16(cpu, REG_HL, hl_addy + 1);

  TICK(cpu, 8);
}

static inline void ld_a_hlm(registers_t *cpu) {
  u16 hl_addy = read_reg16(cpu, REG_HL);
  u8 hl_val = read8(cpu, hl_addy);

  write_reg8(cpu, REG_A, hl_val);
  write_reg16(cpu, REG_HL, hl_addy - 1);

  TICK(cpu, 8);
}

static inline void ld_r_r(registers_t *cpu) {
  //0b01xxxyyy
  u8 opcode = read8(cpu, cpu->PC - 1);
  int x = (opcode >> 3) & 7;
  int y = opcode & 7;

  u8 src = read_reg8(cpu, y);
  write_reg8(cpu, x, src);
  TICK(cpu, (x == REG_HLm || y == REG_HLm) ? 8 : 4);
}

static inline void halt(registers_t *cpu) {
  cpu->halt = true;
  TICK(cpu, 4);
}

// rotates
static inline void rlca(registers_t *cpu) {
  int reg = read_reg8(cpu, REG_A);
  u8 msb = (reg >> 7) & 1;
  reg = (reg << 1) | msb;
  cpu->F.Z = 0;
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, msb);
  write_reg8(cpu, REG_A, reg);
  TICK(cpu, 4);
}

static inline void rra(registers_t *cpu) {
  int reg = read_reg8(cpu, REG_A); 
  u8 lsb = reg & 1;
  int old_c = cpu->F.C;

  reg = (old_c << 7) | (reg >> 1);
  cpu->F.Z = 0;
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, lsb);
  write_reg8(cpu, REG_A, reg);
  TICK(cpu, 4);
}

static inline void rla(registers_t *cpu) {
  int reg = read_reg8(cpu, REG_A);
  u8 msb = (reg >> 7) & 1;
  int old_c = cpu->F.C;
  reg = (reg << 1) | old_c;
  cpu->F.Z = 0;
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, msb);
  write_reg8(cpu, REG_A, reg);
  TICK(cpu, 4);
}

static inline void rrca(registers_t *cpu) {
  int reg = read_reg8(cpu, REG_A);
  u8 lsb = reg & 1;
  reg = (reg >> 1) | (reg << 7);
  cpu->F.Z = 0;
  SET_N(cpu, 0); 
  SET_H(cpu, 0);
  SET_C(cpu, lsb);
  write_reg8(cpu, REG_A, reg);
  TICK(cpu, 4);
}


// arithmetic
static inline void add_hl_rr(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = (opcode >> 4) & 3;
  u16 valHL = read_reg16(cpu, REG_HL);
  u16 valREG = read_reg16(cpu, reg);
  
  uint32_t result = valHL + valREG;
  SET_N(cpu, 0);
  SET_H(cpu, ((valREG & 0x0FFF) + (valHL & 0x0FFF)) > 0x0FFF);
  SET_C(cpu, result > 0xFFFF);
  write_reg16(cpu, REG_HL, result);
  TICK(cpu, 8);
}

static inline void add_r_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;
  u8 a = read_reg8(cpu, REG_A);
  u8 r = read_reg8(cpu, reg);
  u16 result = a + r;

  SET_Z(cpu, (result & 0xFF));
  SET_N(cpu, 0);
  SET_H(cpu, ((a & 0x0F) + (r & 0x0F)) > 0x0F);
  SET_C(cpu, (result > 0xFF));
  write_reg8(cpu, REG_A, (u8)result);
  TICK(cpu, (reg == REG_HLm) ? 8 : 4);
}

static inline void sub_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;

  u8 a = read_reg8(cpu, REG_A);
  u8 r = read_reg8(cpu, reg);
  u16 result = a - r;

  SET_N(cpu, 1);
  SET_Z(cpu, (result & 0xFF));
  SET_H(cpu, (a & 0x0F) < (r & 0x0F));
  SET_C(cpu, (a < r));
  write_reg8(cpu, REG_A, result);
  TICK(cpu, (reg == REG_HLm) ? 8 : 4);
}


//static inline void sbc_r(registers_t *cpu) { 
//  u8 opcode = read8(cpu, cpu->PC - 1); 
//  int reg = opcode & 7; 
//  u8 a = read_reg8(cpu, REG_A); 
//  u8 r = read_reg8(cpu, reg); 
//  u16 result = a - r - cpu->F.C; 
//
//  SET_Z(cpu, (result & 0xFF)); 
//  SET_N(cpu, 1); 
//  //SET_H(cpu, ((a & 0x0F) < (r & 0x0F))); 
//  //SET_C(cpu, (a < r)); 
//  SET_H(cpu, ((a ^ r ^ result) & 0x10) != 0);
//  SET_C(cpu, result > 0xFF); 
//  write_reg8(cpu, REG_A, (u8)result); 
//
//  TICK(cpu, (reg == REG_HLm) ? 8 : 4); 
//}
static inline void sbc_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int idx = opcode & 7;           // which register
  u8 b = read_reg8(cpu, idx);     // its value
  u8 a = cpu->A, c = cpu->F.C;
  u16 res = (u16)a - b - c;

  SET_Z(cpu, (u8)res);
  SET_N(cpu, 1);
  SET_H(cpu, ((a ^ b ^ res) & 0x10) != 0);
  SET_C(cpu, res > 0xFF);

  cpu->A = (u8)res;
  TICK(cpu, (idx == REG_HLm) ? 8 : 4);
}


static inline void sbc_a_u8(registers_t *cpu) { 
  u8 imm = fetch8(cpu);
  u8 a = read_reg8(cpu, REG_A);
  u16 result = a - imm - cpu->F.C;

  SET_Z(cpu, (result & 0xFF));  
  SET_N(cpu, 1);                
  SET_H(cpu, ((a & 0x0F) < ((imm & 0x0F) + cpu->F.C))); 
  SET_C(cpu, (a < (imm + cpu->F.C))); 

  write_reg8(cpu, REG_A, (u8)result);
  TICK(cpu, 8);
}

static inline void adc_r(registers_t *cpu) {

  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;
  u8 a = read_reg8(cpu, REG_A);
  u8 r = read_reg8(cpu, reg);
  u16 result = a + r + cpu->F.C;

  SET_Z(cpu, (u8)result);
  SET_N(cpu, 0);
  SET_H(cpu, ((a & 0x0F) + (r & 0x0F) + cpu->F.C) > 0x0F);
  SET_C(cpu, (result > 0xFF));
  
  write_reg8(cpu, REG_A, (u8)result);
  TICK(cpu, (reg == REG_HLm) ? 8 : 4);
}

static inline void adc_u8(registers_t *cpu) {
  u8 imm = fetch8(cpu);
  u8 a = read_reg8(cpu, REG_A);
  u16 result = imm + a + cpu->F.C;

  SET_Z(cpu, (result & 0xFF));
  SET_N(cpu, 0);
  SET_H(cpu, ((a & 0x0F) + (imm & 0x0F) + cpu->F.C) > 0x0F);
  SET_C(cpu, (result > 0xFF));
  
  write_reg8(cpu, REG_A, (u8)result);
  TICK(cpu,  8);
}

// weird shit
static inline void nop(registers_t *cpu) {
  TICK(cpu, 4);
}

static inline void stop(registers_t *cpu) {
  cpu->bus->timers.DIV = 0;
  cpu->bus->timers.div_count = 0;

  cpu->stopped = true;
  TICK(cpu, 4);
}

static inline void cpl(registers_t *cpu) { 
  u8 a = read_reg8(cpu, REG_A);
  write_reg8(cpu, REG_A, ~a);

  SET_N(cpu, 1);
  SET_H(cpu, 1);
  TICK(cpu, 4);
}

//static inline void daa(registers_t *cpu) { 
//  uint8_t offset = 0;
//  uint8_t a = (uint8_t)read_reg8(cpu, REG_A);
//  bool carry = false;
//
//  if (!cpu->F.N) {
//    if (cpu->F.H || (a & 0x0F) > 0x09)  offset |= 0x06;
//    if (cpu->F.C || (a & 0xFF) > 0x99) { offset |= 0x60; carry=true; }
//    a += offset;
//  } else {
//    if (cpu->F.H) offset |= 0x06;
//    if (cpu->F.C) offset |= 0x60;
//    a -= offset;
//  }
//
//  write_reg8(cpu, REG_A, (u8)a);
//  SET_Z(cpu, a);
//  SET_H(cpu, 0);
//  SET_C(cpu, (carry == 1 ? 1 : 0));
//
//  TICK(cpu, 4);
//}

static inline void daa(registers_t *cpu) {
  uint8_t a = cpu->A;
  uint8_t corr = 0;
  uint8_t newC = cpu->F.C;  // preserve by default

  if (!cpu->F.N) { // after ADD/ADC
    if (cpu->F.H || (a & 0x0F) > 0x09) corr |= 0x06;
    if (cpu->F.C || a > 0x99) { corr |= 0x60; newC = 1; }
    a += corr;
  } else {         // after SUB/SBC
    if (cpu->F.H) corr |= 0x06;
    if (cpu->F.C) corr |= 0x60;
    a -= corr;
    // newC stays whatever it was
  }

  cpu->A = a;
  SET_Z(cpu, a);
  SET_N(cpu, cpu->F.N);    // unchanged
  SET_H(cpu, 0);
  SET_C(cpu, newC);
  TICK(cpu, 4);
}

static inline void scf(registers_t *cpu) {
  SET_C(cpu, 1);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  TICK(cpu, 4);
}

static inline void cp_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;

  u8 a = read_reg8(cpu, REG_A);
  u8 r = read_reg8(cpu, reg);
  u16 result = a - r;

  SET_N(cpu, 1);
  SET_Z(cpu, (result & 0xFF));
  SET_H(cpu, (a & 0x0F) < (r & 0x0F));
  SET_C(cpu, (a < r));
  TICK(cpu, (reg == REG_HLm) ? 8 : 4);
}


// jumps
static inline void jr_e(registers_t *cpu) {
  int8_t offset = (int8_t)fetch8(cpu);
  cpu->PC += offset;
  TICK(cpu, 12);
}


static inline void jr_nz(registers_t *cpu) {
  int8_t offset = (int8_t)fetch8(cpu);
  if (!cpu->F.Z) {
    cpu->PC += offset;
    TICK(cpu, 12);
  } else {
    TICK(cpu, 8);
  }
}

static inline void jr_z(registers_t *cpu) {
  int8_t offset = (int8_t)fetch8(cpu);
  if (cpu->F.Z) {
    cpu->PC += offset;
    TICK(cpu, 12);
  } else {
    TICK(cpu, 8);
  }
}

static inline void jr_nc(registers_t *cpu) {
  int8_t offset = (int8_t)fetch8(cpu);
  if (!cpu->F.C) {
    cpu->PC += offset;
    TICK(cpu, 12);
  } else {
    TICK(cpu, 8);
  }
}

static inline void jr_c(registers_t *cpu) {
  int8_t offset = (int8_t)fetch8(cpu);
  if (cpu->F.C) {
    cpu->PC += offset;
    TICK(cpu, 12);
  } else {
    TICK(cpu, 8);
  }
}

static inline void jp_nz_a16(registers_t *cpu) {
  u16 next = fetch16(cpu);
  
  if (!cpu->F.Z) {
    cpu->PC = next;
    TICK(cpu, 16);
  } else {
    //cpu->PC += 3; // skip
    TICK(cpu, 12);
  }
}

static inline void jp_nc_a16(registers_t *cpu) {
  u16 next = fetch16(cpu);
  
  if (!cpu->F.C) {
    cpu->PC = next;
    TICK(cpu, 16);
  } else {
    //cpu->PC += 3; // skip
    TICK(cpu, 12);
  }
}

static inline void jp_c_a16(registers_t *cpu) {
  u16 next = fetch16(cpu);
  
  if (cpu->F.C) {
    cpu->PC = next;
    TICK(cpu, 16);
  } else {
    //cpu->PC += 3; // skip
    TICK(cpu, 12);
  }
}

static inline void jp_z_a16(registers_t *cpu) {
  u16 next = fetch16(cpu);
  
  if (cpu->F.Z) {
    cpu->PC = next;
    TICK(cpu, 16);
  } else {
    //cpu->PC += 3; // skip
    TICK(cpu, 12);
  }
}

static inline void jp_a16(registers_t *cpu) {
  u16 next = fetch16(cpu);
  cpu->PC = next;
  TICK(cpu, 16);
}

static inline void ccf(registers_t *cpu) {
  SET_C(cpu, !cpu->F.C);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  TICK(cpu, 4);
}

//bit ops
static inline void and_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;
  u8 a = read_reg8(cpu, REG_A);
  u8 r = read_reg8(cpu, reg);

  u8 result = a & r;
  SET_Z(cpu, result);
  SET_N(cpu, 0);
  SET_H(cpu, 1);
  SET_C(cpu, 0);

  write_reg8(cpu, REG_A, result);
  TICK(cpu, (reg == REG_HLm) ? 8 : 4);
}

static inline void xor_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;
  u8 a = read_reg8(cpu, REG_A);
  u8 r = read_reg8(cpu, reg);

  u8 result = a ^ r;
  SET_Z(cpu, result);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, 0);

  write_reg8(cpu, REG_A, result);
  TICK(cpu, (reg == REG_HLm) ? 8 : 4);
}

static inline void or_r(registers_t *cpu) { /* untested */
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;
  u8 a = read_reg8(cpu, REG_A);
  u8 r = read_reg8(cpu, reg);

  u8 result = a | r;
  SET_Z(cpu, result);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, 0);

  write_reg8(cpu, REG_A, result);
  TICK(cpu, (reg == REG_HLm) ? 8 : 4);
}

static inline u16 pop(registers_t *cpu) {
  u8 lsb = read8(cpu, cpu->SP++);
  u8 msb = read8(cpu, cpu->SP++);
  return (msb << 8) | lsb;
}

static inline void ret_nz(registers_t *cpu) {
  if (!cpu->F.Z) {
    cpu->PC = pop(cpu);
    TICK(cpu, 20);
  } else {
    TICK(cpu, 8);
  }
}

static inline void ret_nc(registers_t *cpu) {
  if (!cpu->F.C) {
    cpu->PC = pop(cpu);
    TICK(cpu, 20);
  } else {
    TICK(cpu, 8);
  }
}

static inline void ret_z(registers_t *cpu) {
  if (cpu->F.Z) {
    cpu->PC = pop(cpu);
    TICK(cpu, 20);
  } else {
    TICK(cpu, 8);
  }
}

static inline void ret_c(registers_t *cpu) {
  if (cpu->F.C) {
    cpu->PC = pop(cpu);
    TICK(cpu, 20);
  } else {
    TICK(cpu, 8);
  }
}

static inline void ret(registers_t *cpu) {
    cpu->PC = pop(cpu);
    TICK(cpu, 16);
}



static inline void push(registers_t *cpu, u16 val) {
  cpu->SP--;
  write8(cpu, cpu->SP, (u8)(val >> 8));
  cpu->SP--;
  write8(cpu, cpu->SP, (u8)val & 0xFF);
}

static inline void pop_rr(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = (opcode >> 4) & 3;
  write_reg16(cpu, reg, pop(cpu));
  TICK(cpu, 12);
}

static inline void push_rr(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = (opcode >> 4) & 3;
  push(cpu, read_reg16(cpu, reg));
  TICK(cpu, 16);
}

static inline void call_nz(registers_t *cpu) {
 u16 next = fetch16(cpu);

 if (!cpu->F.Z) {
   push(cpu, cpu->PC);
   cpu->PC = next;
   TICK(cpu, 24);
 } else {
   TICK(cpu, 12);
 }
}

static inline void call_nc(registers_t *cpu) {
 u16 next = fetch16(cpu);

 if (!cpu->F.C) {
   push(cpu, cpu->PC);
   cpu->PC = next;
   TICK(cpu, 24);
 } else {
   TICK(cpu, 12);
 }
}

static inline void call_c(registers_t *cpu) {
 u16 next = fetch16(cpu);

 if (cpu->F.C) {
   push(cpu, cpu->PC);
   cpu->PC = next;
   TICK(cpu, 24);
 } else {
   TICK(cpu, 12);
 }
}

static inline void call_z(registers_t *cpu) {
 u16 next = fetch16(cpu);

 if (cpu->F.Z) {
   push(cpu, cpu->PC);
   cpu->PC = next;
   TICK(cpu, 24);
 } else {
   TICK(cpu, 12);
 }
}

static inline void call_u16(registers_t *cpu) {
   u16 next = fetch16(cpu);
   push(cpu, cpu->PC);
   cpu->PC = next;
   TICK(cpu, 24);
}

static inline void add_a_imm(registers_t *cpu) {
  u8 imm = fetch8(cpu);
  u8 a = read_reg8(cpu, REG_A);
  u16 result = a + imm;


  SET_Z(cpu, (u8)result);
  SET_N(cpu, 0);
  SET_H(cpu, ((a & 0x0F) + (imm & 0x0F)) > 0x0F);
  SET_C(cpu, (result > 0xFF));
  write_reg8(cpu, REG_A, (u8)result);
  TICK(cpu, 8);
}

static inline void sub_a_imm(registers_t *cpu) {
  u8 imm = fetch8(cpu);
  u8 a = read_reg8(cpu, REG_A);
  u16 result = a - imm;

  SET_Z(cpu, (u8)result);                       
  SET_N(cpu, 1);                                
  SET_H(cpu, (a & 0x0F) < (imm & 0x0F));        
  SET_C(cpu, a < imm); 

  write_reg8(cpu, REG_A, (u8)result);
  TICK(cpu, 8);
}

static inline void rst(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  u8 n = (opcode >> 3) & 7;

  u16 addr = n << 3;
  push(cpu, cpu->PC);
  cpu->PC = addr;

  TICK(cpu, 16);
}

void prefix(registers_t *cpu) {
  u8 opcode = fetch8(cpu);
  if (!cb_ops[opcode]) {
    printf("Non existent prefixed opcode\n");
  } else {
    cb_ops[opcode](cpu);
  }
}

static inline void reti(registers_t *cpu) {
  cpu->PC = pop(cpu);
  cpu->IME = 1;
  TICK(cpu, 16);
}

static inline void ldh_u8_a(registers_t *cpu) {
  u8 imm = fetch8(cpu);
  u16 addy = 0xFF00 + imm;
  write8(cpu, addy, read_reg8(cpu, REG_A));
  TICK(cpu, 12);
}

static inline void ldh_c_a(registers_t *cpu) {
  u16 addy = 0xFF00 + read_reg8(cpu, REG_C);
  write8(cpu, addy, read_reg8(cpu, REG_A));
  TICK(cpu, 8);
}

static inline void and_a_imm(registers_t *cpu) {
  u8 imm = fetch8(cpu);
  u8 a = read_reg8(cpu, REG_A);
  u8 result = a & imm;
  write_reg8(cpu, REG_A, result);

  SET_Z(cpu, result);
  SET_N(cpu, 0);
  SET_H(cpu, 1);
  SET_C(cpu, 0);

  TICK(cpu, 8);
}

static inline void add_sp_n8(registers_t *cpu) {
    int8_t imm = (int8_t)fetch8(cpu);
    u16 sp = cpu->SP;
    u16 result = sp + imm;

    cpu->F.Z = 0;
    cpu->F.N = 0;

    u8 lo = (u8)(sp & 0xFF);
    u8 n = (u8)imm;

    SET_H(cpu, ((lo & 0x0F) + (n & 0x0F)) > 0x0F);
    SET_C(cpu, (lo + n) > 0xFF);

    cpu->SP = result;
    TICK(cpu, 16);
}

static inline void jp_hl(registers_t *cpu) {
  cpu->PC = cpu->HL;
  TICK(cpu, 8);
}

static inline void ld_a16_a(registers_t *cpu) {
  u16 imm = fetch16(cpu);
  write8(cpu, imm, read_reg8(cpu, REG_A));
  TICK(cpu, 16);
}

static inline void xor_a_u8(registers_t *cpu) {
  u8 imm = fetch8(cpu);
  u8 a = read_reg8(cpu, REG_A);
  u8 result = a ^ imm;

  write_reg8(cpu, REG_A, result);

  SET_Z(cpu, result);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, 0);

  TICK(cpu, 8);
}

static inline void ldh_a_u8(registers_t *cpu) {
  u8 imm = fetch8(cpu);
  u16 addr = 0xFF00 + imm;
  write_reg8(cpu, REG_A, read8(cpu, addr));
  TICK(cpu, 12);
}

static inline void pop_af(registers_t *cpu) {
  u8 lsb = read8(cpu, cpu->SP++);   
  u8 msb = read8(cpu, cpu->SP++);  

  cpu->A = msb;

  cpu->F.Z = (lsb >> 7) & 1;
  cpu->F.N = (lsb >> 6) & 1;
  cpu->F.H = (lsb >> 5) & 1;
  cpu->F.C = (lsb >> 4) & 1;

  TICK(cpu, 12);
}

static inline void ldh_a_c(registers_t *cpu) {
  u16 addy = 0xFF00 + read_reg8(cpu, REG_C);
  write_reg8(cpu, REG_A, read8(cpu, addy));
  TICK(cpu, 8);
}

static inline void di(registers_t *cpu) {
  cpu->IME = false;
  TICK(cpu, 4);
}

static inline void or_a_u8(registers_t *cpu) {
  u8 imm = fetch8(cpu);
  u8 a = read_reg8(cpu, REG_A);
  u8 result = a | imm;
  
  write_reg8(cpu, REG_A, result);

  SET_Z(cpu, result);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, 0);

  TICK(cpu, 8);
}

static inline void push_af(registers_t *cpu) {
  u8 f = (cpu->F.Z << 7) |
         (cpu->F.N << 6) |
         (cpu->F.H << 5) |
         (cpu->F.C << 4);

  cpu->SP--;
  write8(cpu, cpu->SP, cpu->A);
  cpu->SP--;
  write8(cpu, cpu->SP, f);

  TICK(cpu, 16);
}

static inline void ld_hl_sp_e8(registers_t *cpu) {
  int8_t offset = (int8_t)fetch8(cpu);
  u16 sp = cpu->SP;
  u16 result = sp + offset;

  cpu->F.Z = 0;
  cpu->F.N = 0;

  u8 lo = (u8)(sp & 0xFF);
  u8 n = (u8)offset;
  SET_H(cpu, ((lo & 0x0F) + (n & 0x0F)) > 0x0F);
  SET_C(cpu, (lo + n) > 0xFF);

  cpu->HL = result;
  TICK(cpu, 12);
}

static inline void ld_sp_hl(registers_t *cpu) {
  cpu->SP = cpu->HL;
  TICK(cpu, 8);
}

static inline void ld_a_a16(registers_t *cpu) {
  u16 addr = fetch16(cpu);           
  u8 val = read8(cpu, addr);        
  write_reg8(cpu, REG_A, val);      
  TICK(cpu, 16);
}

static inline void ei(registers_t *cpu) {
  cpu->ime_pending = true;
  TICK(cpu, 4);
}

static inline void cp_a_u8(registers_t *cpu) {
  u8 imm = fetch8(cpu);
  u8 a = read_reg8(cpu, REG_A);
  u16 result = a - imm;

  SET_Z(cpu, (result & 0xFF));               
  SET_N(cpu, 1);                             
  SET_H(cpu, (a & 0x0F) < (imm & 0x0F));     
  SET_C(cpu, a < imm);                       

  TICK(cpu, 8);
}


// $CB PREFIX 
static inline void rlc_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;

  u8 val = read_reg8(cpu, reg);
  u8 carry = (val >> 7) & 1;
  val = (val << 1) | carry;

  SET_Z(cpu, val);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, carry);

  write_reg8(cpu, reg, val);
  TICK(cpu, (reg == REG_HLm) ? 16 : 8);
}

static inline void rrc_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;

  u8 val = read_reg8(cpu, reg);
  u8 carry = val & 1;
  val = (val >> 1) | (carry << 7);

  SET_Z(cpu, val);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, carry);

  write_reg8(cpu, reg, val);
  TICK(cpu, (reg == REG_HLm) ? 16 : 8);
}

static inline void rl_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;

  u8 val = read_reg8(cpu, reg);
  u8 old_c = cpu->F.C;
  u8 new_c = (val >> 7) & 1;
  val = (val << 1) | old_c;

  SET_Z(cpu, val);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, new_c);

  write_reg8(cpu, reg, val);
  TICK(cpu, (reg == REG_HLm) ? 16 : 8);
}

static inline void rr_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;

  u8 val = read_reg8(cpu, reg);
  u8 old_c = cpu->F.C;
  u8 new_c = val & 1;
  val = (val >> 1) | (old_c << 7);

  SET_Z(cpu, val);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, new_c);

  write_reg8(cpu, reg, val);
  TICK(cpu, (reg == REG_HLm) ? 16 : 8);
}

static inline void sla_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;

  u8 val = read_reg8(cpu, reg);
  u8 new_c = (val >> 7) & 1;
  val <<= 1;

  SET_Z(cpu, val);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, new_c);

  write_reg8(cpu, reg, val);
  TICK(cpu, (reg == REG_HLm) ? 16 : 8);
}

static inline void sra_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;

  u8 val = read_reg8(cpu, reg);
  u8 carry = val & 1;
  u8 msb = val & 0x80;
  val = (val >> 1) | msb;

  SET_Z(cpu, val);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, carry);

  write_reg8(cpu, reg, val);
  TICK(cpu, (reg == REG_HLm) ? 16 : 8);
}

static inline void swap_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;

  u8 val = read_reg8(cpu, reg);
  val = (val << 4) | (val >> 4);

  SET_Z(cpu, val);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, 0);

  write_reg8(cpu, reg, val);
  TICK(cpu, (reg == REG_HLm) ? 16 : 8);
}

static inline void srl_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;

  u8 val = read_reg8(cpu, reg);
  u8 carry = val & 1;
  val >>= 1;

  SET_Z(cpu, val);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
  SET_C(cpu, carry);

  write_reg8(cpu, reg, val);
  TICK(cpu, (reg == REG_HLm) ? 16 : 8);
}

static inline void bit_n_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int bit = (opcode >> 3) & 7;
  int reg = opcode & 7;

  u8 val = read_reg8(cpu, reg);
  bool zero = (val & (1 << bit));

  SET_Z(cpu, zero);
  SET_N(cpu, 0);
  SET_H(cpu, 1);

  TICK(cpu, (reg == REG_HLm) ? 12 : 8);
}

static inline void res_n_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int bit = (opcode >> 3) & 7;
  int reg = opcode & 7;

  u8 val = read_reg8(cpu, reg);
  val &= ~(1 << bit);
  write_reg8(cpu, reg, val);

  TICK(cpu, (reg == REG_HLm) ? 16 : 8);
}

static inline void set_n_r(registers_t *cpu) {
  u8 opcode = read8(cpu, cpu->PC - 1);
  int bit = (opcode >> 3) & 7;
  int reg = opcode & 7;

  u8 val = read_reg8(cpu, reg);
  val |= (1 << bit);
  write_reg8(cpu, reg, val);

  TICK(cpu, (reg == REG_HLm) ? 16 : 8);
}



void (*opcodes[256])(registers_t *cpu) = {
  nop, ld_rr_immediate, ld_bc_a, inc_rr, inc_r, dec_r, ld_r_immediate, rlca,
  ld_a16_sp, add_hl_rr, ld_a_bc, dec_rr, inc_r, dec_r, ld_r_immediate, rrca,
  stop, ld_rr_immediate, ld_de_a, inc_rr, inc_r, dec_r, ld_r_immediate, rla,
  jr_e, add_hl_rr, ld_a_de, dec_rr, inc_r, dec_r, ld_r_immediate, rra, 
  jr_nz, ld_rr_immediate, ld_hlp_a, inc_rr, inc_r, dec_r, ld_r_immediate, daa,
  jr_z, add_hl_rr, ld_a_hlp, dec_rr, inc_r, dec_r, ld_r_immediate, cpl,
  jr_nc, ld_rr_immediate, ld_hlm_a, inc_rr, inc_r, dec_r, ld_r_immediate, scf,
  jr_c, add_hl_rr, ld_a_hlm, dec_rr, inc_r, dec_r, ld_r_immediate, ccf,
  ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, 
  ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, 
  ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, 
  ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r,   ld_r_r, 
  ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, 
  ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, 
  ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, halt, ld_r_r, 
  ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, 
								  
  add_r_r, add_r_r, add_r_r, add_r_r, add_r_r, add_r_r, add_r_r, add_r_r,
  adc_r, adc_r, adc_r, adc_r, adc_r, adc_r, adc_r, adc_r,
  sub_r, sub_r, sub_r, sub_r, sub_r, sub_r, sub_r, sub_r, 
  sbc_r, sbc_r, sbc_r, sbc_r, sbc_r, sbc_r, sbc_r, sbc_r,
  and_r, and_r, and_r, and_r, and_r, and_r, and_r, and_r, 
  xor_r, xor_r, xor_r, xor_r, xor_r, xor_r, xor_r, xor_r, 
  or_r, or_r, or_r, or_r, or_r, or_r, or_r, or_r, 
  cp_r, cp_r, cp_r, cp_r, cp_r, cp_r, cp_r, cp_r, 

  ret_nz, pop_rr, jp_nz_a16, jp_a16, call_nz, push_rr ,add_a_imm, rst, 
  ret_z, ret, jp_z_a16, prefix, call_z, call_u16, adc_u8, rst, 
  ret_nc, pop_rr, jp_nc_a16, NULL, call_nc, push_rr, sub_a_imm, rst,
  ret_c, reti, jp_c_a16, NULL, call_c, NULL, sbc_a_u8, rst,
  ldh_u8_a, pop_rr, ldh_c_a, NULL, NULL, push_rr, and_a_imm, rst,
  add_sp_n8, jp_hl, ld_a16_a, NULL, NULL, NULL, xor_a_u8, rst,
  ldh_a_u8, pop_af, ldh_a_c, di, NULL, push_af, or_a_u8, rst,
  ld_hl_sp_e8, ld_sp_hl, ld_a_a16, ei, NULL, NULL, cp_a_u8, rst
};

void (*cb_ops[256])(registers_t *cpu) = {
  rlc_r, rlc_r, rlc_r, rlc_r, rlc_r, rlc_r, rlc_r, rlc_r, 
  rrc_r, rrc_r, rrc_r, rrc_r, rrc_r, rrc_r, rrc_r, rrc_r, 
  rl_r, rl_r, rl_r, rl_r, rl_r, rl_r, rl_r, rl_r, 
  rr_r, rr_r, rr_r, rr_r, rr_r, rr_r, rr_r, rr_r, 
  sla_r, sla_r, sla_r, sla_r, sla_r, sla_r, sla_r, sla_r, 
  sra_r, sra_r, sra_r, sra_r, sra_r, sra_r, sra_r, sra_r, 
  swap_r, swap_r, swap_r, swap_r, swap_r, swap_r, swap_r, swap_r, 
  srl_r, srl_r, srl_r, srl_r, srl_r, srl_r, srl_r, srl_r, 
  bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r,     
  bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r,     
  bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r,     
  bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r,     
  bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r,     
  bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r,     
  bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r,     
  bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r,     

  res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r,     
  res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r,     
  res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r,     
  res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r,     
  res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r,     
  res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r,     
  res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r,     
  res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r,     

  set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r,     
  set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r,     
  set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r,     
  set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r,     
  set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r,     
  set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r,     
  set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r,     
  set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r,     
};


void debug_state(registers_t *cpu, u8 opcode) {
    fprintf(stderr, "PC=%04X OP=%02X A=%02X F=%d%d%d%d BC=%04X DE=%04X HL=%04X SP=%04X CYC=%lu\n",
        cpu->PC-1, opcode,
        cpu->A,
        cpu->F.Z, cpu->F.N, cpu->F.H, cpu->F.C,
        cpu->BC, cpu->DE, cpu->HL, cpu->SP, cpu->cycle);
    fflush(stderr);
}

void cpu_go(registers_t *cpu) {
    u8 opcode = fetch8(cpu);
    trace_log(cpu, opcode);
    //debug_state(cpu, opcode);
    if (opcodes[opcode]) opcodes[opcode](cpu);
    else {
      exit(1);
    } 

    if (cpu->ime_pending) {
      cpu->IME = 1;
      cpu->ime_pending = false;
    }
}


void RESET_CPU(registers_t *cpu) {
    memset(cpu, 0, sizeof(registers_t));
    cpu->SP = 0xFFFE;
    cpu->PC = 0x0100;
}

void load_rom(registers_t *cpu, const char *path) {
  if (bus_load_rom(cpu->bus, path) != 0) {
    fprintf(stderr, "ROM has failed to load: %s\n", path);
    exit(1);
  }
  printf("Loaded ROM %s\n", path);
}

int service_interrupt(registers_t *cpu) {
  u8 req = (cpu->bus->IF & cpu->bus->IE) & 0x1F;
  if (!cpu->IME || !req) return 0;

  cpu->halt = false;
  cpu->IME = 0;

  int which = -1;
  if (req & 0x01) which = 0;
  else if (req & 0x02) which = 1;
  else if (req & 0x04) which = 2;
  else if (req & 0x08) which = 3;
  else if (req & 0x10) which = 4;

  if (which >= 0) {
    cpu->bus->IF &= ~(1 << which);

    push(cpu, cpu->PC);
    cpu->PC = (u16)(0x40 + (which * 8));
    TICK(cpu, 20);
    return 1;
  }
  return 0;
}

void halt_wake(registers_t *cpu) {
  if (!cpu->halt) return;
  if (cpu->bus->IF & cpu->bus->IE) {
    cpu->halt = false;
  }
}

void stop_wake(registers_t *cpu) {
  if (!cpu->stopped) {
    return;
  }
  if (cpu->bus->IF & cpu->bus->IE & JOYP_IF) {
    cpu->stopped =false;
  }
}

void helper(registers_t *cpu) {

  stop_wake(cpu);
  halt_wake(cpu);		

   if (service_interrupt(cpu)) return;

  if (cpu->stopped) {
    return;
  }

  if (cpu->halt) {
    TICK(cpu, 4);   
    return;
  }

  uint8_t opcode = fetch8(cpu);
  trace_log(cpu, opcode);
  if (opcodes[opcode]) opcodes[opcode](cpu);
  else { 
    //fprintf(stderr, "Cycle cap hit. Dumping trace:\n");
    //trace_dump_last(256);
    exit(1);
  }

  if (cpu->ime_pending) {
    cpu->IME = 1;
    cpu->ime_pending = false;
  }
}

