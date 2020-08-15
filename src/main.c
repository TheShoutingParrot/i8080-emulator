#include "cpu.h"
#include "util.h"
#include "memory.h"

void printCpuState(void) {
	printf("registers: bc %02X%02X de %02X%02X hl %02X%02X psw: %02X%02X\n"
		"\tflags: c: %d p: %d ac: %d z: %d s: %d\nsp: %X\ncycles: %d\n", 
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

int main(void) {
	loadRom("cpu_tests/TST8080.COM", 0x100);

	programCounter = 0x100;

	registers[rSTATUS] = (0 << 5) | (0 << 3) | (1 << 1);

	printf("beginning cpu tests...\nprinting current cpu state...\n");
	printCpuState();
	putchar('\n');

	for(;;) {
		cpuExecuteInstruction();
	}
	
	return EXIT_FAILURE; /* program shouldn't get here */
}
