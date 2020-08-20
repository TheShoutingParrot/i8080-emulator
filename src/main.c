#include "cpu.h"
#include "util.h"
#include "memory.h"

void printCpuState(void) {
	printf("registers: bc %02X%02X de %02X%02X hl %02X%02X psw: %02X%02X\n"
		"\tflags: c: %d p: %d ac: %d z: %d s: %d\nsp: %X\ncycles: %d\n\n", 
		registers[rB], registers[rC], 
		registers[rD], registers[rE],
		registers[rH], registers[rL], 
		registers[rA], registers[rSTATUS], 
		isBitSet(registers[rSTATUS], carryF),
		isBitSet(registers[rSTATUS], parityF), 
		isBitSet(registers[rSTATUS], auxCarryF),
		isBitSet(registers[rSTATUS], zeroF),
		isBitSet(registers[rSTATUS], signF), 
		stackPointer,
		cycleCounter);
}

void printSuperzazuStyleState(void) {
        printf("PC: %04X, AF: %04X, BC: %04X, DE: %04X, HL: %04X, SP: %04X, CYC: %lu",
                programCounter, registers[rA] << 8 | registers[rSTATUS],
                registers[rB] << 8 | registers[rC], registers[rD] << 8 | registers[rE],
                registers[rH] << 8 | registers[rL],
                stackPointer, cycleCounter);

        printf("\t(%02X %02X %02X %02X)\n", readMemory(programCounter), readMemory(programCounter + 1),
                readMemory(programCounter + 2), readMemory(programCounter + 3));
}

int main(void) {
	loadRom("cpu_tests/CPUTEST.COM", 0x100);

	programCounter = 0x100;

	registers[rSTATUS] = (0 << 5) | (0 << 3) | (1 << 1);

	for(;;) {
		cpuExecuteInstruction();

		/* fgetc(stdin); */
	}
	
	return EXIT_FAILURE; /* program shouldn't get here */
}
