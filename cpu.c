#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cpu.h"
#include "test.h"
//#ifndef TESTING
//#define TESTING
//#endif


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
//static inline u8 read8(registers_t *cpu, u16 addy) {
//  return cpu->mem[addy];
//}
//static inline void write8(registers_t *cpu, u16 addy, u8 val) {
//  cpu->mem[addy] = val;
//}

u8 read8(registers_t *cpu, u16 addr) {
    if (addr < 0x8000) return cpu->mem[addr];
    else if (addr >= 0xC000 && addr < 0xE000) return cpu->mem[addr];
    else if (addr >= 0xE000 && addr < 0xFE00) return cpu->mem[addr - 0x2000];
    else if (addr == 0xFF02) return cpu->mem[addr] | 0x80; // serial always ready
    else if (addr >= 0xFF80 && addr <= 0xFFFE) return cpu->mem[addr];
    return 0x00;
}

void write8(registers_t *cpu, u16 addr, u8 val) {
    if (addr < 0x8000) return; // ROM is read-only

    else if (addr >= 0xC000 && addr < 0xE000) cpu->mem[addr] = val;
    else if (addr >= 0xE000 && addr < 0xFE00) cpu->mem[addr - 0x2000] = val;
    else if (addr == 0xFF01) cpu->mem[addr] = val; // serial data
    else if (addr == 0xFF02) {
        cpu->mem[addr] = val;
        if (val == 0x81) { // "transfer start"
            putchar(cpu->mem[0xFF01]);
            fflush(stdout);
            cpu->mem[0xFF02] = 0x01; // mark transfer complete
        }
    }
    else if (addr >= 0xFF80 && addr <= 0xFFFE) cpu->mem[addr] = val;
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

static inline void halt(registers_t *cpu) {
  cpu->halt = true;
  TICK(cpu, 4);
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

    SET_Z(cpu, 0);
    SET_N(cpu, 0);
    SET_H(cpu, ((sp ^ imm ^ result) & 0x10) != 0);
    SET_C(cpu, ((sp ^ imm ^ result) & 0x100) != 0);

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
  SET_Z(cpu, 0);

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
  SET_Z(cpu, 0);

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

  SET_Z(cpu, 0);
  SET_N(cpu, 0);
  SET_H(cpu, ((sp & 0x0F) + (offset & 0x0F)) > 0x0F);
  SET_C(cpu, ((sp & 0xFF) + (offset & 0xFF)) > 0xFF);

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
  cpu->IME = true;
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
  bool zero = !(val & (1 << bit));

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

//void cpu_go(registers_t *cpu) {
//    u8 opcode = fetch8(cpu);
//
//    if (opcodes[opcode]) {
//        opcodes[opcode](cpu);
//    } else {
//        printf("Unimplemented opcode: 0x%02X\n", opcode);
//    }
//}


void debug_state(registers_t *cpu, u8 opcode) {
    printf("PC=%04X OP=%02X A=%02X F=%d%d%d%d BC=%04X DE=%04X HL=%04X SP=%04X CYC=%lu\n",
        cpu->PC-1, opcode,
        cpu->A,
        cpu->F.Z, cpu->F.N, cpu->F.H, cpu->F.C,
        cpu->BC, cpu->DE, cpu->HL, cpu->SP, cpu->cycle);
}

void cpu_go(registers_t *cpu) {
    u8 opcode = fetch8(cpu);
    debug_state(cpu, opcode);
    if (opcodes[opcode]) opcodes[opcode](cpu);
    else printf("Unimplemented opcode: 0x%02X\n", opcode);
}


void RESET_CPU(registers_t *cpu) {
    memset(cpu, 0, sizeof(registers_t));
    cpu->SP = 0xFFFE;
    cpu->PC = 0x0100;
}
void load_rom(registers_t *cpu, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("ROM open failed"); exit(1); }

    size_t bytes = fread(cpu->mem, 1, 0x8000, f);
    fclose(f);

    printf("Loaded %zu bytes from %s\n", bytes, path);
}



int main(void) {
#ifdef TESTING
    run_tests();
#endif

    registers_t cpu = {0};
    RESET_CPU(&cpu);
    load_rom(&cpu, "cpu_instrs.gb");
    for (int i = 0x100; i < 0x110; i++)
    printf("%02X ", cpu.mem[i]);
puts("");

    printf("ROM first bytes: %02X %02X %02X %02X %02X\n",
       cpu.mem[0x0000], cpu.mem[0x0100], cpu.mem[0x0101], cpu.mem[0x0102], cpu.mem[0x0150]);

    printf("Starting PC=%04X (byte there=%02X)\n", cpu.PC, cpu.mem[cpu.PC]);
    for (int i = 0; i < 1000; i++) {
        cpu_go(&cpu);
	printf("Executed opcode %02X at PC=%04X\n", cpu.mem[cpu.PC - 1], cpu.PC - 1);
    }

    printf("CPU stopped after %lu cycles\n", cpu.cycle);
    return 0;
}

