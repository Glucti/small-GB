#include <stdio.h>
#include "cpu.h"
#include "test.h"
#ifndef TESTING
#define TESTING
#endif


#define TICK(cpu, n)  ((cpu)->cycle += (n))
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


void (*cb_ops[256])(registers_t *cpu);

// helpers
static inline u8 read8(registers_t *cpu, u16 addy) {
  return cpu->mem[addy];
}
static inline void write8(registers_t *cpu, u16 addy, u8 val) {
  cpu->mem[addy] = val;
}
static inline u16 read16(registers_t *cpu, u16 addy) {
  u8 low = read8(cpu, addy);
  u8 high = read8(cpu, addy + 1);

  return (high << 8) | low;
}
static inline void write16(registers_t *cpu, u16 addy, u16 val) {
  write8(cpu, addy, val & 0xFF);
  write8(cpu, addy + 1, val >> 8);
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
  SET_H(cpu, ((old & 0x0F) - 1) > 0x0F);

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

  printf("LD_r_r: opcode=%02X x=%d y=%d HL=%04X E=%02X mem[HL]=%02X\n",
         opcode, x, y, cpu->HL, cpu->E, cpu->mem[cpu->HL]);
  
  u8 src = read_reg8(cpu, y);
  write_reg8(cpu, x, src);
  TICK(cpu, (x == REG_HLm || y == REG_HLm) ? 8 : 4);
}

static inline void halt() {
  // todo 
  printf("one");
}


// rotates
static inline void rlca(registers_t *cpu) {
  int reg = read_reg8(cpu, REG_A);
  u8 msb = (reg >> 7) & 1;
  reg = (reg << 1) | msb;
  SET_Z(cpu, 1);
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
  SET_Z(cpu, 1);
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
  SET_Z(cpu, 1);
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
  SET_Z(cpu, 1);
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


static inline void sbc_r(registers_t *cpu) { 
  u8 opcode = read8(cpu, cpu->PC - 1); 
  int reg = opcode & 7; 
  u8 a = read_reg8(cpu, REG_A); 
  u8 r = read_reg8(cpu, reg); 
  u16 result = a - r - cpu->F.C; 

  SET_Z(cpu, (result & 0xFF)); 
  SET_N(cpu, 1); 
  SET_H(cpu, ((a & 0x0F) < (r & 0x0F))); 
  SET_C(cpu, (a < r)); 

  write_reg8(cpu, REG_A, (u8)result); 

  TICK(cpu, (reg == REG_HLm) ? 8 : 4); 
}

static inline void adc_r(registers_t *cpu) {

  u8 opcode = read8(cpu, cpu->PC - 1);
  int reg = opcode & 7;
  u8 a = read_reg8(cpu, REG_A);
  u8 r = read_reg8(cpu, reg);
  u16 result = a + r + cpu->F.C;

  SET_Z(cpu, (result & 0xFF));
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

static inline void daa(registers_t *cpu) { 
  uint8_t offset = 0;
  uint8_t a = (uint8_t)read_reg8(cpu, REG_A);
  bool carry = false;

  if (!cpu->F.N) {
    if (cpu->F.H || (a & 0x0F) > 9)  offset |= 0x06;
    if (cpu->F.C || (a & 0xFF) > 99) { offset |= 0x60; carry=true; }
    a += offset;
  } else {
    if (cpu->F.H) offset |= 0x06;
    if (cpu->F.C) offset |= 0x60;
    a -= offset;
  }

  write_reg8(cpu, REG_A, (u8)a);
  SET_Z(cpu, a);
  SET_H(cpu, 0);
  SET_C(cpu, (carry == 1 ? 1 : 0));

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
  write_reg8(cpu, REG_A, result);
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
  u16 next = read16(cpu, cpu->PC + 1);
  
  if (!cpu->F.Z) {
    cpu->PC = next;
    TICK(cpu, 16);
  } else {
    cpu->PC += 3; // skip
    TICK(cpu, 12);
  }
}

static inline void jp_nc_a16(registers_t *cpu) {
  u16 next = read16(cpu, cpu->PC + 1);
  
  if (!cpu->F.C) {
    cpu->PC = next;
    TICK(cpu, 16);
  } else {
    cpu->PC += 3; // skip
    TICK(cpu, 12);
  }
}

static inline void jp_c_a16(registers_t *cpu) {
  u16 next = read16(cpu, cpu->PC + 1);
  
  if (cpu->F.C) {
    cpu->PC = next;
    TICK(cpu, 16);
  } else {
    cpu->PC += 3; // skip
    TICK(cpu, 12);
  }
}

static inline void jp_z_a16(registers_t *cpu) {
  u16 next = read16(cpu, cpu->PC + 1);
  
  if (cpu->F.Z) {
    cpu->PC = next;
    TICK(cpu, 16);
  } else {
    cpu->PC += 3; // skip
    TICK(cpu, 12);
  }
}

static inline void jp_a16(registers_t *cpu) {
  u16 next = read16(cpu, cpu->PC + 1);
  cpu->PC = next;
  TICK(cpu, 16);
}

static inline void ccf(registers_t *cpu) {
  SET_C(cpu, !cpu->F.C);
  SET_N(cpu, 0);
  SET_H(cpu, 0);
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
 u16 next = read16(cpu, cpu->PC + 1);

 if (!cpu->F.Z) {
   push(cpu, cpu->PC+3);
   cpu->PC = next;
   TICK(cpu, 24);
 } else {
   cpu->PC += 3;
   TICK(cpu, 12);
 }
}

static inline void call_nc(registers_t *cpu) {
 u16 next = read16(cpu, cpu->PC + 1);

 if (!cpu->F.C) {
   push(cpu, cpu->PC+3);
   cpu->PC = next;
   TICK(cpu, 24);
 } else {
   cpu->PC += 3;
   TICK(cpu, 12);
 }
}

static inline void call_c(registers_t *cpu) {
 u16 next = read16(cpu, cpu->PC + 1);

 if (cpu->F.C) {
   push(cpu, cpu->PC+3);
   cpu->PC = next;
   TICK(cpu, 24);
 } else {
   cpu->PC += 3;
   TICK(cpu, 12);
 }
}

static inline void call_z(registers_t *cpu) {
 u16 next = read16(cpu, cpu->PC + 1);

 if (cpu->F.Z) {
   push(cpu, cpu->PC+3);
   cpu->PC = next;
   TICK(cpu, 24);
 } else {
   cpu->PC += 3;
   TICK(cpu, 12);
 }
}

static inline void call_u16(registers_t *cpu) {
   u16 next = read16(cpu, cpu->PC + 1);
   push(cpu, cpu->PC+3);
   cpu->PC = next;
   TICK(cpu, 24);
}

static inline void add_a_imm(registers_t *cpu) {
  u8 imm = fetch8(cpu);
  u8 a = read_reg8(cpu, REG_A);
  u16 result = a + imm;


  SET_Z(cpu, result);
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
  push(cpu, cpu->PC + 1);
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


void (*opcodes[256])(registers_t *cpu) = {
  nop, ld_rr_immediate, ld_bc_a, inc_rr, inc_r, dec_r, ld_r_immediate, rlca,
  ld_a16_sp, add_hl_rr, ld_a_bc, dec_rr, inc_r, dec_r, ld_r_immediate, rrca,
  stop, ld_rr_immediate, ld_de_a, inc_rr, inc_r, dec_r, ld_r_immediate, rla,
  jr_e, add_hl_rr, ld_a_de, dec_rr, inc_r, dec_r, ld_r_immediate, rra, 
  jr_nz, ld_rr_immediate, ld_hlp_a, inc_rr, inc_r, dec_r, ld_r_immediate, daa,
  jr_z, ld_rr_immediate, ld_a_hlp, dec_rr, inc_r, dec_r, ld_r_immediate, cpl,
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
  ret_c, reti, jp_c_a16, NULL, call_c, NULL, 
};

void (*cb_ops[256])(registers_t *cpu) = {
};

void cpu_go(registers_t *cpu) {
    u8 opcode = fetch8(cpu);

    if (opcodes[opcode]) {
        opcodes[opcode](cpu);
    } else {
        printf("Unimplemented opcode: 0x%02X\n", opcode);
    }
}


int main(void) {

#ifdef TESTING
  run_tests();
#endif

  printf("working");

    return 0;
}

