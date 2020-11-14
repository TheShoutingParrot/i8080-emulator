#ifdef _CPU_TEST

#include "cpu.h"
#include "util.h"
#include "memory.h"

void testPortOut(struct cpu8080 *cpu, uint8_t ports, uint8_t port) {
	if(cpu->readMemory(cpu->memory, cpu->programCounter) == 0) {
		cpu->signalBuffer = exitSignal;
	}
	else if(cpu->readMemory(cpu->memory, cpu->programCounter) == 1) {
		if(cpu->registers[rC] == 2)
			printf("%c", cpu->registers[rE]);
		if(cpu->registers[rC] == 9) {
			uint16_t i = 
				(cpu->registers[rD] << 8 | cpu->registers[rE] & 0x00FF);
			while(cpu->readMemory(cpu->memory, i) != '$')
				printf("%c", cpu->readMemory(cpu->memory, i++));
		}
	}

}

void runTest(const char *testPath) {
	struct cpu8080 cpu;

	cpu.memory = calloc(0xffff, sizeof(*(cpu.memory)));

	if(cpu.memory == NULL) {
		exit(1);
	}

	loadRom(cpu.memory, testPath, 0x100);
	
	cpu.memory[0x0000] = 0xD3;
        cpu.memory[0x0001] = 0x00;

        cpu.memory[0x0005] = 0xD3;
        cpu.memory[0x0006] = 0x01;
        cpu.memory[0x0007] = 0xC9;

	cpu.readMemory = testReadMemory;
	cpu.readMemoryWord = testReadMemoryWord;
	cpu.writeMemory = testWriteMemory;
	cpu.writeMemoryWord = testWriteMemoryWord;

	cpu.portOut = testPortOut;

	cpu.programCounter = 0x100;

	cpu.registers[rSTATUS] = (0 << 5) | (0 << 3) | (1 << 1);

	cpu.cycleCounter = 0;
	
	cpu.signalBuffer = noSignal;

	for(;;) {
		cpuExecuteInstruction(&cpu, NULL);

		if(cpu.signalBuffer == exitSignal) {
			printf("\ntest finished. cpu's final state:\n");
			printCpuState(cpu);
			puts("exiting loop...");
			break;
		}

#ifdef SINGLE_STEP 
#warning "single stepping is not recommended for debugging"
		fgetc(stdin);
#endif
	}

	free(cpu.memory);
}

int main(void) {
	//runTest("cpu_tests/8080EXM.COM");
	runTest("cpu_tests/CPUTEST.COM");
	runTest("cpu_tests/TST8080.COM");
	
	return EXIT_SUCCESS;
}

#endif /* #ifdef _CPU_TEST */
