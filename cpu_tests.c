#include <assert.h> 
#include <stdio.h> 
#include <string.h>
#include "cpu.h"

#define RUN_TEST(name, fn) do {		\
  printf("Running %s ....", name);	\
  fn();					\
  printf("\tPassed\n");			\
} while(0)

#define PRINT_CPU(cpu) do { \
    printf("PC=%04X A=%02X B=%02X C=%02X BC=%04X D=%02X E=%02X DE=%04X\nSP=%04X HL=%04X Z=%d N=%d H=%d C=%d cycles=%lu\n", \
        (cpu).PC, (cpu).A, (cpu).B, (cpu).C, (cpu).BC, (cpu).D, (cpu).E, \
	(cpu).DE, (cpu).SP, (cpu).HL, \
        (cpu).F.Z, (cpu).F.N, (cpu).F.H, (cpu).F.C, (cpu).cycle); \
} while(0)

#define SET_CPU() \
  registers_t cpu; \
  reset_cpu(&cpu)	\

static inline void reset_cpu(registers_t *cpu) {
  memset(cpu, 0, sizeof(*cpu));
}


// ld BC u16 0x01
void test_ld_BC_imm(void) {
  registers_t cpu;
  reset_cpu(&cpu);

  cpu.PC = 0x0000;
  cpu.mem[0x0000] = 0x01;
  cpu.mem[0x0001] = 0xCD;
  cpu.mem[0x0002] = 0xAB;

  cpu_go(&cpu);

  assert(cpu.BC == 0xABCD);
  assert(cpu.PC == 0x0003);
  assert(cpu.cycle == 12);
}

// ld (BC) A 0x02
void test_ld_bc_a(void) {
  registers_t cpu; 
  reset_cpu(&cpu);

  cpu.PC = 0x0000;
  cpu.A = 0xAB;
  cpu.BC = 0x1234;
  cpu.mem[0x0000] = 0x02;

  cpu_go(&cpu);

  assert(cpu.mem[0x1234] == 0xAB);
  assert(cpu.BC == 0x1234);
  assert(cpu.PC == 0x0001);
  assert(cpu.cycle == 8);
}

// inc BC 0x03
void test_inc_BC(void) {
  registers_t cpu; 
  reset_cpu(&cpu);

  cpu.PC = 0x0000;
  cpu.BC = 0x0002;

  cpu.mem[0x0000] = 0x03;

  cpu_go(&cpu);

  assert(cpu.PC == 0x0001);
  assert(cpu.BC == 0x0003);
  assert(cpu.cycle == 8);
}

// inc B 0x04
void test_inc_B(void) {
  registers_t cpu;
  reset_cpu(&cpu);

  cpu.PC = 0x0000;
  cpu.B = 0x02;

  cpu.mem[0x0000] = 0x04;

  cpu_go(&cpu);
  assert(cpu.PC == 0x0001);
  assert(cpu.B == 0x03);
  assert(cpu.cycle == 4);
}

// dec B 0x05
void test_dec_B(void) { 
  registers_t cpu; 
  reset_cpu(&cpu);

  cpu.PC = 0x0000;
  cpu.B = 0x02;
  cpu.mem[0x0000] = 0x05;

  cpu_go(&cpu);
  assert(cpu.PC == 0x0001);
  assert(cpu.B == 0x01);
  assert(cpu.cycle == 4);
}

// ld B u8 0x06
void test_ld_B_imm(void) {
  registers_t cpu;
  reset_cpu(&cpu);

  cpu.PC = 0x0000;
  cpu.B = 0x12;

  cpu.mem[0x0000] = 0x06;
  cpu.mem[0x0001] = 0xAB;
  cpu_go(&cpu);

  assert(cpu.B == 0xAB);
  assert(cpu.PC == 0x0002);
  assert(cpu.cycle == 8);
}

// RLCA 0x07
void test_RLCA(void) {
  registers_t cpu; 
  reset_cpu(&cpu);

  cpu.PC = 0x0000;
  cpu.C = 0;
  cpu.A = 0xAB;

  cpu.mem[0x0000] = 0x07;
  cpu_go(&cpu);

  assert(cpu.A == 0x57);
  assert(cpu.F.C == 1);
  assert(cpu.cycle == 4);
}

// ld u16 SP 0x08
void test_ld_nn_SP(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.SP = 0x1234;

  cpu.mem[0x0000] = 0x08;
  cpu.mem[0x0001] = 0x01;
  cpu.mem[0x0002] = 0x02;
  cpu_go(&cpu);

  assert(cpu.mem[0x0201] == 0x34);
  assert(cpu.mem[0x0202] == 0x12);
  assert(cpu.cycle == 20);
}

// ld ADD HL BC 0x09
void test_add_hl_bc(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.HL = 0xFFFF;
  cpu.BC = 0xFFFF;

  cpu.mem[0x000] = 0x09;
  cpu_go(&cpu);

  assert(cpu.HL == 0xFFFE);
  assert(cpu.F.C == 1);
  assert(cpu.cycle == 8);
}

// ld A (BC) 0x0A
void test_ld_a_bc(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.BC = 0x1234;
  cpu.mem[0x1234] = 0x45;

  cpu.mem[0x0000] = 0x0A;
  cpu_go(&cpu);

  assert(cpu.A == 0x45);
  assert(cpu.cycle == 8);
  assert(cpu.PC == 0x0001);
}

// DEC BC 0x0B
void test_dec_bc(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.BC = 0x1234;
  cpu.mem[0x0000] = 0x0B;
  cpu_go(&cpu);

  assert(cpu.BC == 0x1233);
  assert(cpu.PC == 0x0001);
  assert(cpu.cycle == 8);
}

// INC C 0x0C 
void test_inc_c(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.C = 0x12;

  cpu.mem[0x0000] = 0x0C;
  cpu_go(&cpu);

  assert(cpu.C == 0x13);
  assert(cpu.cycle == 4);
}

// DEC C 0x0D

void test_dec_c(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.C = 0x01;

  cpu.mem[0x0000] = 0x0D;
  cpu_go(&cpu);

  assert(cpu.C == 0x00);
  assert(cpu.cycle == 4);
}

//ld C u8 0x0E
void test_ld_c_imm(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.B = 0x0F;

  cpu.mem[0x0000] = 0x0E;
  cpu.mem[0x0001] = 0x56;
  cpu_go(&cpu);

  assert(cpu.C == 0x56);
  assert(cpu.cycle == 8);
}

//RRCA 0x0F

void test_rrca(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.A = 0xAB; // d5 when rotated
  cpu.mem[0x0000] = 0x0F;
  cpu_go(&cpu);

  assert(cpu.A == 0xD5);
  assert(cpu.F.C == 1);
  assert(cpu.cycle == 4);
}

// STOP 0x10
void stop(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.mem[0x0000] = 0x10;
  cpu_go(&cpu);

  assert(cpu.stopped == true);
}

// ld DE u8 0x11
void test_ld_DE_imm(void) {
  SET_CPU();

  cpu.PC = 0x0000;
  cpu.mem[0x0000] = 0x11;
  cpu.mem[0x0001] = 0xCD;
  cpu.mem[0x0002] = 0xAB;

  cpu_go(&cpu);

  assert(cpu.DE == 0xABCD);
  assert(cpu.PC == 0x0003);
  assert(cpu.cycle == 12);
}

// LD (DE) A 0x12
void test_ld_de_a(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.DE = 0x1234;
  cpu.mem[0x1234] = 0x00;
  cpu.A = 0xAB;

  cpu.mem[0x0000] = 0x12;
  cpu_go(&cpu); 
  assert(cpu.mem[0x1234] == 0xAB);
  assert(cpu.cycle == 8);
}

// INC DE 0x13
void test_inc_de(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.DE = 0x0012;

  cpu.mem[0x0000] = 0x013;
  cpu_go(&cpu);

  assert(cpu.DE == 0x0013);
  assert(cpu.cycle == 8);
}

void test_all_incdec_r(registers_t *cpu, u8 opcode, void *reg, bool isInc, bool is8bit) {
  reset_cpu(cpu);
  cpu->PC = 0x0000;

  if (is8bit) {
    *(u8 *)reg = 0x02;
  } else {
    *(u16 *)reg = 0x0002;
  }

  cpu->mem[0x0000] = opcode;
  cpu_go(cpu);

  uint32_t exp = isInc ? 0x0003 : 0x0001;
  uint32_t res = is8bit ? *(u8 *)reg : *(u16 *)reg; 

  assert(exp == res);
  assert(cpu->cycle == (is8bit ? 4 : 8));
}

void test_inc_generic(void) {
  registers_t cpu;
  test_all_incdec_r(&cpu, 0x14, &cpu.D, 1, 1);
  test_all_incdec_r(&cpu, 0x24, &cpu.H, 1, 1);
  test_all_incdec_r(&cpu, 0x1C, &cpu.E, 1, 1);
  test_all_incdec_r(&cpu, 0x2C, &cpu.L, 1, 1);
  test_all_incdec_r(&cpu, 0x3C, &cpu.A, 1, 1);
}

void test_dec_generic(void) {
  registers_t cpu;
  test_all_incdec_r(&cpu, 0x15, &cpu.D, 0, 1);
  test_all_incdec_r(&cpu, 0x25, &cpu.H, 0, 1);
  test_all_incdec_r(&cpu, 0x1D, &cpu.E, 0, 1);
  test_all_incdec_r(&cpu, 0x2D, &cpu.L, 0, 1);
  test_all_incdec_r(&cpu, 0x3D, &cpu.A, 0, 1);
}

void test_incdec_rr(void) {
  registers_t cpu;
  //inc RR
  test_all_incdec_r(&cpu, 0x03, &cpu.BC, 1, 0);
  test_all_incdec_r(&cpu, 0x13, &cpu.DE, 1, 0);
  test_all_incdec_r(&cpu, 0x23, &cpu.HL, 1, 0);
  test_all_incdec_r(&cpu, 0x33, &cpu.SP, 1, 0);

  //dec RR
  test_all_incdec_r(&cpu, 0x0B, &cpu.BC, 0, 0);
  test_all_incdec_r(&cpu, 0x1B, &cpu.DE, 0, 0);
  test_all_incdec_r(&cpu, 0x2B, &cpu.HL, 0, 0);
  test_all_incdec_r(&cpu, 0x3B, &cpu.SP, 0, 0);
}

// LD (HL+) A 0x22
void test_ld_hlp_a(void) {
  SET_CPU(); 
  cpu.PC = 0x0000;
  cpu.HL = 0x1234;
  cpu.mem[0x1234] = 0x01;
  cpu.A = 0x11;

  cpu.mem[0x0000] = 0x22;
  cpu_go(&cpu);

  assert(cpu.mem[0x1234] == 0x11);
  assert(cpu.HL == 0x1235);
  assert(cpu.cycle == 8);
}

void test_all_ld_rr_imm(registers_t *cpu, u8 opcode, u16 *reg) {
  reset_cpu(cpu);
  cpu->PC = 0x0000;
  cpu->mem[0x0001] = 0xCD;
  cpu->mem[0x0002] = 0xAB;

  cpu->mem[0x0000] = opcode;
  cpu_go(cpu);

  assert(*reg == 0xABCD);
  assert(cpu->cycle == 12);
}

void test_ld_rr_imm_generic(void) {
  registers_t cpu;

  test_all_ld_rr_imm(&cpu, 0x01, &cpu.BC);
  test_all_ld_rr_imm(&cpu, 0x11, &cpu.DE);
  test_all_ld_rr_imm(&cpu, 0x21, &cpu.HL);
  test_all_ld_rr_imm(&cpu, 0x31, &cpu.SP);
}

void test_ld_r_imm(registers_t *cpu, u8 opcode, u8 *reg) {
  reset_cpu(cpu);
  cpu->PC = 0x0000;
  cpu->mem[0x0001] = 0xAB;

  cpu->mem[0x0000] = opcode;
  cpu_go(cpu);

  assert(*reg == 0xAB);
  assert(cpu->cycle == 8);
}

void test_ld_r_imm_generic(void) {
  registers_t cpu;
  test_ld_r_imm(&cpu, 0x06, &cpu.B);
  test_ld_r_imm(&cpu, 0x16, &cpu.D);
  test_ld_r_imm(&cpu, 0x26, &cpu.H);
  test_ld_r_imm(&cpu, 0x0E, &cpu.C);
  test_ld_r_imm(&cpu, 0x1E, &cpu.E);
  test_ld_r_imm(&cpu, 0x2E, &cpu.L);
  test_ld_r_imm(&cpu, 0x3E, &cpu.A);
}

void test_all_add_r_r(registers_t *cpu, u8 opcode, u8 *reg) {
  reset_cpu(cpu);
  cpu->PC = 0x0000;
  cpu->A = 0x02;
  *reg = 0x02;
  cpu->mem[0x0000] = opcode;
  cpu_go(cpu);

  assert(cpu->A == 0x04);
  assert(cpu->cycle == 4);
  assert(cpu->F.C == 0);
  assert(cpu->F.N == 0);
  assert(cpu->F.H == 0);
  assert(cpu->F.Z == 0);
}

void test_all_add_generic(void) {
  registers_t cpu;

  test_all_add_r_r(&cpu, 0x80, &cpu.B);
  test_all_add_r_r(&cpu, 0x81, &cpu.C);
  test_all_add_r_r(&cpu, 0x82, &cpu.D);
  test_all_add_r_r(&cpu, 0x83, &cpu.E);
  test_all_add_r_r(&cpu, 0x84, &cpu.H);
  test_all_add_r_r(&cpu, 0x85, &cpu.L);
}


void test_add_a_a(void) {
  SET_CPU();

  cpu.PC = 0x0000;
  cpu.A = 0x02;
  cpu.mem[0x0000] = 0x87;
  cpu_go(&cpu);

  assert(cpu.A == 0x04);
  assert(cpu.cycle == 4);
}

void test_add_a_hl(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.HL = 0x1234;
  cpu.mem[0x1234] = 0x12;
  cpu.mem[0x0000] = 0x86;
  cpu_go(&cpu);

  assert(cpu.A == 0x12);
  assert(cpu.cycle == 8);
}

void test_adc_b(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.B = 0x12;
  cpu.F.C = 1;

  cpu.mem[0x0000] = 0x88;
  cpu_go(&cpu);
  
  assert(cpu.A == 0x13);
  assert(cpu.B == 0x12);
  assert(cpu.cycle == 4);
}

// ld B d8
void ld_b_d8(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.mem[0x0001] = 0xAB;
  cpu.mem[0x0000] = 0x06;
  cpu_go(&cpu);

  assert(cpu.B == 0xAB);
  assert(cpu.cycle == 8);
}

void ld_d_d8(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.mem[0x0001] = 0xAB;
  cpu.mem[0x0000] = 0x16;
  cpu_go(&cpu);

  assert(cpu.D == 0xAB);
  assert(cpu.cycle == 8);
}

void ld_h_d8(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.mem[0x0001] = 0xAB;
  cpu.mem[0x0000] = 0x26;
  cpu_go(&cpu);

  assert(cpu.H == 0xAB);
  assert(cpu.cycle == 8);
}

void ld_hl_d8(void) {
  SET_CPU();
  cpu.HL = 0x1234;
  cpu.PC = 0x0000;
  cpu.mem[0x0001] = 0xAB;
  cpu.mem[0x0000] = 0x36;
  cpu_go(&cpu);

  assert(cpu.mem[0x1234] == 0xAB);
  assert(cpu.cycle == 12);
}

void ld_bc_a(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.A = 0x0A;
  cpu.BC = 0x1234;
  cpu.mem[0x1234] = 0x33;

  cpu.mem[0x0000] = 0x02;
  cpu_go(&cpu);

  assert(cpu.mem[0x1234] == 0x0A);
  assert(cpu.cycle == 8);
}

void ld_a_bc(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.BC = 0x1234;
  cpu.mem[0x1234] = 0x0F;
  cpu.mem[0x0000] = 0x0A;
  cpu_go(&cpu);

  assert(cpu.A == 0x0F);
  assert(cpu.cycle == 8);
}

void ld_de_a(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.A = 0x0A;
  cpu.DE = 0x1234;
  cpu.mem[0x1234] = 0x33;

  cpu.mem[0x0000] = 0x12;
  cpu_go(&cpu);

  assert(cpu.mem[0x1234] == 0x0A);
  assert(cpu.cycle == 8);
}

void rla(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.F.C = 1;
  cpu.A = 0x0A;
  cpu.mem[0x0000] = 0x17;
  cpu_go(&cpu);
  
  assert(cpu.A == 0x15);
  assert(cpu.C == 0);
  assert(cpu.cycle == 4);
}

void jr_e(void) {
  SET_CPU();
  cpu.PC = 0x0012;
  cpu.mem[0x0012] = 0x18;
  cpu.mem[0x0013] = (u8)-2;
  cpu_go(&cpu);

  assert(cpu.PC == 0x0012);
}

void load_de_a(void) {
  SET_CPU(); 
  cpu.PC = 0x0000;
  cpu.DE = 0x1234;
  cpu.mem[0x1234] = 0xAB;
  cpu.mem[0x0000] = 0x12;
  cpu_go(&cpu);
  
  assert(cpu.A == 0xAB);
  assert(cpu.cycle == 8);
}

void rra(void) {
  SET_CPU(); 
  reset_cpu(&cpu);
  cpu.PC = 0x0000;
  cpu.A = 0xAA;
  cpu.F.C = 1;
  cpu.mem[0x0000] = 0x1F;
  cpu_go(&cpu);
  
  assert(cpu.A == 0xD5);
  assert(cpu.F.C == 0);
}

void jr_nz(void) {
  SET_CPU();
  cpu.PC = 0x0012;
  cpu.mem[0x0012] = 0x20;
  cpu.mem[0x0013] = 0x04;
  cpu_go(&cpu);

  assert(cpu.PC == 0x0018);
}
void ld_hlp_a(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.HL = 0x1234;
  cpu.mem[0x1234] = 0x33;
  cpu.A = 0x12;
  cpu.mem[0x0000] = 0x22;
  cpu_go(&cpu);
  
  assert(cpu.mem[0x1234] == 0x12);
  assert(cpu.HL == 0x1235);
}

void daa(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.F.H = 0;
  cpu.F.N = 1;
  cpu.F.C = 1;
  cpu.A = 0xE4;

  cpu.mem[0x0000] = 0x27;
  cpu_go(&cpu);

  assert(cpu.A == 0x84);
}

void jr_z(void) {
  SET_CPU();
  cpu.PC = 0x0012;
  cpu.F.Z = 1;
  cpu.mem[0x0012] = 0x28;
  cpu.mem[0x0013] = 0x04;
  cpu_go(&cpu);

  assert(cpu.PC == 0x0018);
}

void ld_a_hlp(void) {
  SET_CPU();
  cpu.PC = 0x0000;
  cpu.HL = 0x1234;
  cpu.mem[0x1234] = 0xAB;
  cpu.mem[0x0000] = 0x2A;
  cpu_go(&cpu);

  PRINT_CPU(cpu);
  assert(cpu.A == 0xAB);
  assert(cpu.HL == 0x1235);
}

void cpl(void) {
  SET_CPU(); 
  cpu.PC = 0x0000;
  cpu.A = 0xAA;
  cpu.mem[0x0000] = 0x2F;
  cpu_go(&cpu);

  PRINT_CPU(cpu);
  assert(cpu.A == 0x55);
  assert(cpu.F.N == 1);
  assert(cpu.F.H == 1);
}

void run_tests() {
  RUN_TEST("LD A (BC)", ld_a_bc);
  RUN_TEST("LD (BC) A", ld_bc_a);
  RUN_TEST("LD (DE) A", ld_de_a);
  RUN_TEST("RLA", rla);
  RUN_TEST("JR e8", jr_e);
  RUN_TEST("LD (DE) A", ld_de_a);
  RUN_TEST("RRCA", test_rrca);
  RUN_TEST("RLCA", test_RLCA);
  RUN_TEST("RRA", rra);
  RUN_TEST("JR NZ", jr_nz);
  RUN_TEST("LD (HL++) A", ld_hlp_a);
  RUN_TEST("DAA", daa);
  RUN_TEST("JR Z", jr_z);
  RUN_TEST("LD A (HL+)", ld_a_hlp);
  RUN_TEST("CPL", cpl);
  printf("\nAll tests passed\n");
}
