#include "cpu.h"
#include "util.h"
#include "memory.h"

static void cpuResetStatusRegister(struct cpu8080 *cpu);
static void clearOrSetParityBit(struct cpu8080 *cpu, uint8_t value);
static void cpuPushToStack(struct cpu8080 *cpu, uint16_t data);
static uint16_t cpuPopFromStack(struct cpu8080 *cpu);
static void cpuWriteWordToRegisterPair(struct cpu8080 *cpu, uint8_t r1, uint8_t r2, uint16_t data);
static void cpuJumpToAddr(struct cpu8080 *cpu, uint16_t addr);
static void cpuJumpIf(struct cpu8080 *cpu, bool value, uint16_t addr);
static void cpuInstructionMVI(struct cpu8080 *cpu, uint8_t r, uint8_t data);
static void cpuInstructionANI(struct cpu8080 *cpu, uint8_t value);
static void cpuInstructionADI(struct cpu8080 *cpu, uint8_t value);
static void cpuInstructionSUI(struct cpu8080 *cpu, uint8_t value);
static void cpuInstructionCPI(struct cpu8080 *cpu, uint8_t value);
static void cpuInstructionCALL(struct cpu8080 *cpu, uint16_t addr);
static void cpuInstructionRET(struct cpu8080 *cpu);
static void cpuInstructionXCHG(struct cpu8080 *cpu);

void printCpuState(struct cpu8080 cpu) {
        printf("PC: %04X, AF: %04X, BC: %04X, DE: %04X, HL: %04X, SP: %04X, CYC: %lu",
                cpu.programCounter, cpu.registers[rA] << 8 | cpu.registers[rSTATUS],
                cpu.registers[rB] << 8 | cpu.registers[rC], cpu.registers[rD] << 8 | cpu.registers[rE],
                cpu.registers[rH] << 8 | cpu.registers[rL],
                cpu.stackPointer, cpu.cycleCounter);

        printf("\t(%02X %02X %02X %02X)\n", cpu.readMemory(cpu.memory, cpu.programCounter), cpu.readMemory(cpu.memory, cpu.programCounter + 1),
                cpu.readMemory(cpu.memory, cpu.programCounter + 2), cpu.readMemory(cpu.memory, cpu.programCounter + 3));
}

static void cpuResetStatusRegister(struct cpu8080 *cpu) {
	cpu->registers[rSTATUS] = 0x2;	
}

static void clearOrSetParityBit(struct cpu8080 *cpu, uint8_t value) {
	uint8_t temp;

	temp = value ^ (value >> 4);
	temp ^= (temp >> 2);
	temp ^= (temp >> 1);

	clearOrSetBit(&cpu->registers[rSTATUS], parityF, (temp & 1) == 0);
}

static void cpuPushToStack(struct cpu8080 *cpu, uint16_t data) {
	cpu->stackPointer -= 2;
	cpu->writeMemoryWord(cpu->memory, cpu->stackPointer, data);
}

static uint16_t cpuPopFromStack(struct cpu8080 *cpu) {
	uint16_t data;

	data = cpu->readMemoryWord(cpu->memory, cpu->stackPointer);

	cpu->stackPointer += 2;

	return data;
}

static void cpuWriteWordToRegisterPair(struct cpu8080 *cpu, uint8_t r1, uint8_t r2, uint16_t data) {
	cpu->registers[r1] = data >> 8;
	cpu->registers[r2] = data & 0x00FF;
}

static uint16_t cpuReadRegisterPair(struct cpu8080 *cpu, uint8_t r1, uint8_t r2) {
	return cpu->registers[r1] << 8 | cpu->registers[r2];
}

static void cpuJumpToAddr(struct cpu8080 *cpu, uint16_t addr) {
	cpu->programCounter = addr;
}

static void cpuJumpIf(struct cpu8080 *cpu, bool value, uint16_t addr) {
	if(value) {
		cpuJumpToAddr(cpu, addr);
	}
	else
		cpu->programCounter += 2;
}

static void cpuInstructionMVI(struct cpu8080 *cpu, uint8_t r, uint8_t data) {
	cpu->registers[r] = data;
}

static void cpuInstructionMOV(struct cpu8080 *cpu, uint8_t r1, uint8_t r2) {
	cpu->registers[r1] = cpu->registers[r2];
}

static void cpuInstructionMVItoM(struct cpu8080 *cpu, uint8_t data) {
	cpu->writeMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL), data);
}

static void cpuInstructionMOVfromM(struct cpu8080 *cpu, uint8_t r) {
	cpu->registers[r] = cpu->readMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL));
}

static void cpuInstructionANI(struct cpu8080 *cpu, uint8_t value) {
	cpuResetStatusRegister(cpu);

	/* auxiliary carry*/
	clearOrSetBit(&cpu->registers[rSTATUS], 4,
			isBitSet(value, 3) | isBitSet(cpu->registers[rA], 3));

	cpu->registers[rA] &= value;

	/* carry */
	clearBit(&cpu->registers[rSTATUS], 0);

	/* parity */
	clearOrSetParityBit(cpu, cpu->registers[rA]);

	/* zero */
	clearOrSetBit(&cpu->registers[rSTATUS], 6, !cpu->registers[rA]);
	
	/* sign */
	clearOrSetBit(&cpu->registers[rSTATUS], 7, isBitSet(cpu->registers[rA], 7));
}

static void cpuInstructionORI(struct cpu8080 *cpu, uint8_t value) {
	cpuResetStatusRegister(cpu);

	cpu->registers[rA] |= value;

	/* carry */
	clearBit(&cpu->registers[rSTATUS], carryF);

	/* parity */
	clearOrSetParityBit(cpu, cpu->registers[rA]);

	/* auxiliary carry*/
	clearOrSetBit(&cpu->registers[rSTATUS], auxCarryF, 0);

	/* zero */
	clearOrSetBit(&cpu->registers[rSTATUS], zeroF, !cpu->registers[rA]);
	
	/* sign */
	clearOrSetBit(&cpu->registers[rSTATUS], signF, isBitSet(cpu->registers[rA], 7));
}

static void cpuInstructionXRI(struct cpu8080 *cpu, uint8_t value) {
	cpuResetStatusRegister(cpu);

	cpu->registers[rA] ^= value;

	/* carry */
	clearBit(&cpu->registers[rSTATUS], carryF);

	/* parity */
	clearOrSetParityBit(cpu, cpu->registers[rA]);

	/* auxiliary carry*/
	clearBit(&cpu->registers[rSTATUS], auxCarryF);

	/* zero */
	clearOrSetBit(&cpu->registers[rSTATUS], zeroF, !cpu->registers[rA]);
	
	/* sign */
	clearOrSetBit(&cpu->registers[rSTATUS], signF, isBitSet(cpu->registers[rA], 7));
}

static void cpuAdd(struct cpu8080 *cpu, uint8_t r, uint8_t value) {
	cpuResetStatusRegister(cpu);

	/* carry */
	clearOrSetBit(&cpu->registers[rSTATUS], 0, (((uint16_t)cpu->registers[r] + (uint16_t)value) > 0xff));

	/* auxiliary carry*/
	clearOrSetBit(&cpu->registers[rSTATUS], 4, ((cpu->registers[r] & 0x0F) + (value & 0x0F) > 0x0F));
	
	cpu->registers[r] += value;

	/* parity */
	clearOrSetParityBit(cpu, cpu->registers[r]);
	
	/* zero */
	clearOrSetBit(&cpu->registers[rSTATUS], 6, !cpu->registers[r]);

	/* sign */
	clearOrSetBit(&cpu->registers[rSTATUS], 7, isBitSet(cpu->registers[r], 7));
}

static void cpuInstructionADI(struct cpu8080 *cpu, uint8_t value) {
	cpuAdd(cpu, rA, value);
}

static void cpuInstructionACI(struct cpu8080 *cpu, uint8_t value) {
	bool carryBit;

	carryBit = isBitSet(cpu->registers[rSTATUS], carryF);

	cpuResetStatusRegister(cpu);

	/* carry */
	clearOrSetBit(&cpu->registers[rSTATUS], 0, (((uint16_t)cpu->registers[rA] + (uint16_t)value + (uint16_t)carryBit) > 0xff));

	/* auxiliary carry*/
	clearOrSetBit(&cpu->registers[rSTATUS], 4, (((cpu->registers[rA] & 0x0F) + (value & 0x0F) + ((uint8_t)carryBit)) > 0x0F));
	
	cpu->registers[rA] += (value + (uint8_t)carryBit);

	/* parity */
	clearOrSetParityBit(cpu, cpu->registers[rA]);
	
	/* zero */
	clearOrSetBit(&cpu->registers[rSTATUS], 6, !cpu->registers[rA]);

	/* sign */
	clearOrSetBit(&cpu->registers[rSTATUS], 7, isBitSet(cpu->registers[rA], 7));
}

static void cpuSubtract(struct cpu8080 *cpu, uint8_t r, uint8_t value) {
	/* carry */
	clearOrSetBit(&cpu->registers[rSTATUS], 0, (((int16_t)cpu->registers[rA] - ((int16_t)value)) < 0));

	/* auxiliary carry*/
	clearOrSetBit(&cpu->registers[rSTATUS], 4, ((cpu->registers[rA]&0x0f) + ((~value)&0x0f)) >= 0x0f);

	cpu->registers[r] -= value;

	/* parity */
	clearOrSetParityBit(cpu, cpu->registers[r]);
	
	/* zero */
	clearOrSetBit(&cpu->registers[rSTATUS], 6, !cpu->registers[r]);

	/* sign */
	clearOrSetBit(&cpu->registers[rSTATUS], 7, isBitSet(cpu->registers[r], 7));
}

static void cpuInstructionSUI(struct cpu8080 *cpu, uint8_t value) {
	cpuSubtract(cpu, rA, value);
}

static void cpuInstructionSBI(struct cpu8080 *cpu, uint8_t value) {
	bool borrowBit;

	borrowBit = isBitSet(cpu->registers[rSTATUS], carryF);

	value += (uint8_t)borrowBit;

	/* carry */
	clearOrSetBit(&cpu->registers[rSTATUS], 0, (((uint16_t)cpu->registers[rA] - ((uint16_t)value)) < 0));

	/* auxiliary carry*/
	clearOrSetBit(&cpu->registers[rSTATUS], 4, (((cpu->registers[rA] & 0x0F) > (value & 0x0F))));
	
	cpu->registers[rA] -= value;

	/* parity */
	clearOrSetParityBit(cpu, cpu->registers[rA]);
	
	/* zero */
	clearOrSetBit(&cpu->registers[rSTATUS], 6, !cpu->registers[rA]);

	/* sign */
	clearOrSetBit(&cpu->registers[rSTATUS], 7, isBitSet(cpu->registers[rA], 7));
}

static void cpuInstructionCPI(struct cpu8080 *cpu, uint8_t value) {
	uint8_t temp;

	temp = cpu->registers[rA];

	cpuSubtract(cpu, rA, value);

	cpu->registers[rA] = temp;
}

static void cpuInstructionCALL(struct cpu8080 *cpu, uint16_t addr) {
	cpuPushToStack(cpu, cpu->programCounter+2);
	cpuJumpToAddr(cpu, addr);

}

static void cpuCallIf(struct cpu8080 *cpu, bool value, uint16_t addr) {
	if(value) {
		cpuInstructionCALL(cpu, addr);
		cpu->cycleCounter += 17;
	}
	else {
		cpu->programCounter += 2;
		cpu->cycleCounter += 11;
	}
}

static void cpuInstructionRET(struct cpu8080 *cpu) {
	cpu->programCounter = cpuPopFromStack(cpu); 
}

static void cpuReturnIf(struct cpu8080 *cpu, bool value) {
	if(value) {
		cpuInstructionRET(cpu);
		cpu->cycleCounter += 11;
	}
	else
		cpu->cycleCounter += 5;
}

static void cpuInstructionXCHG(struct cpu8080 *cpu) {
	uint8_t r1, r2;

	r1 = cpu->registers[rD];
	r2 = cpu->registers[rE];

	cpu->registers[rD] = cpu->registers[rH];
	cpu->registers[rE] = cpu->registers[rL];
	
	cpu->registers[rH] = r1;
	cpu->registers[rL] = r2;
}

static void cpuInstructionINR(struct cpu8080 *cpu, uint8_t r) {
	/* auxiliary carry*/
	clearOrSetBit(&cpu->registers[rSTATUS], 4, ((unsigned)((cpu->registers[r] & 0x0F) + 1)) > 0x0F);
	
	cpu->registers[r]++;

	/* parity */
	clearOrSetParityBit(cpu, cpu->registers[r]);
	
	/* zero */
	clearOrSetBit(&cpu->registers[rSTATUS], zeroF, !cpu->registers[r]);

	/* sign */
	clearOrSetBit(&cpu->registers[rSTATUS], signF, isBitSet(cpu->registers[r], 7));
}

static void cpuInstructionINX(struct cpu8080 *cpu, uint8_t r1, uint8_t r2) {
	cpuWriteWordToRegisterPair(cpu, r1, r2, cpuReadRegisterPair(cpu, r1, r2) + 1);
}

static void cpuInstructionDCR(struct cpu8080 *cpu, uint8_t r) {
	/* auxiliary carry*/
	clearOrSetBit(&cpu->registers[rSTATUS], 4, (cpu->registers[r] & 0x0F) != 0);
	
	cpu->registers[r]--;

	/* parity */
	clearOrSetParityBit(cpu, cpu->registers[r]);
	
	/* zero */
	clearOrSetBit(&cpu->registers[rSTATUS], 6, !cpu->registers[r]);

	/* sign */
	clearOrSetBit(&cpu->registers[rSTATUS], 7, isBitSet(cpu->registers[r], 7));
}

static void cpuInstructionDCX(struct cpu8080 *cpu, uint8_t r1, uint8_t r2) {
	cpuWriteWordToRegisterPair(cpu, r1, r2, cpuReadRegisterPair(cpu, r1, r2) - 1);
}

static void cpuInstructionDAD(struct cpu8080 *cpu, uint16_t data) {
	clearBit(&cpu->registers[rSTATUS], carryF);
	clearOrSetBit(&cpu->registers[rSTATUS], 0, (((uint32_t)cpuReadRegisterPair(cpu, rH, rL) + (uint32_t)data) > UINT16_MAX));

	cpuWriteWordToRegisterPair(cpu, rH, rL, cpuReadRegisterPair(cpu, rH, rL) + data);
}

void cpuExecuteInstruction(struct cpu8080 *cpu) {
	uint8_t opcode,
		temp;

	opcode = cpu->readMemory(cpu->memory, cpu->programCounter);

#ifdef DEBUG 
        printCpuState(*cpu);
#endif

	cpu->programCounter++;

	switch(opcode) {
		/* NOP */
		case 0x00:
			cpu->cycleCounter += 4;

			break;

		/* LXI BC, d16 */
		case 0x01:
			cpuWriteWordToRegisterPair(cpu, rB, rC, cpu->readMemoryWord(cpu->memory, cpu->programCounter));

			cpu->programCounter += 2;

			cpu->cycleCounter += 10;

			break;

		/* STAX BC */
		case 0x02:
			cpu->writeMemory(cpu->memory, cpuReadRegisterPair(cpu, rB, rC), cpu->registers[rA]);

			cpu->cycleCounter += 7;

			break;

		/* INX BC */
		case 0x03:
			cpuInstructionINX(cpu, rB, rC);

			cpu->cycleCounter += 5;

			break;

		/* INR B */
		case 0x04:
			cpuInstructionINR(cpu, rB);

			cpu->cycleCounter += 5;

			break;

		/* DCR B */
		case 0x05:
			cpuInstructionDCR(cpu, rB);

			cpu->cycleCounter += 5;

			break;

		/* MVI B, d8 */
		case 0x06:
			cpuInstructionMVI(cpu, rB, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* RLC */
		case 0x07:
			clearOrSetBit(&cpu->registers[rSTATUS], carryF, isBitSet(cpu->registers[rA], 7));

			temp = isBitSet(cpu->registers[rA], 7);

			cpu->registers[rA] <<= 1;

			clearOrSetBit(&cpu->registers[rA], 0, temp);

			cpu->cycleCounter += 4;

			break;

		/* ILLEGAL/UNDOCUMENTED OPCODE - NOP */
		case 0x08:
			cpu->cycleCounter += 4;

			break;

		/* DAD BC */
		case 0x09:
			cpuInstructionDAD(cpu, cpuReadRegisterPair(cpu, rB, rC));

			cpu->cycleCounter += 10;

			break;

		/* LDAX BC */
		case 0x0A:
			cpu->registers[rA] = cpu->readMemory(cpu->memory, cpuReadRegisterPair(cpu, rB, rC));

			cpu->cycleCounter += 7;

			break;

		/* DCX BC */
		case 0x0B:
			cpuInstructionDCX(cpu, rB, rC);

			cpu->cycleCounter += 5;

			break;

		/* INR C */
		case 0x0C:
			cpuInstructionINR(cpu, rC);

			cpu->cycleCounter += 5;

			break;

		/* DCR C */
		case 0x0D:
			cpuInstructionDCR(cpu, rC);

			cpu->cycleCounter += 5;

			break;

		/* MVI C, d8 */
		case 0x0E:
			cpuInstructionMVI(cpu, rC, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* RRC */
		case 0x0F:
			clearOrSetBit(&cpu->registers[rSTATUS], carryF, isBitSet(cpu->registers[rA], 0));

			temp = isBitSet(cpu->registers[rA], 0);

			cpu->registers[rA] >>= 1;

			clearOrSetBit(&cpu->registers[rA], 7, temp);

			cpu->cycleCounter += 4;

			break;

		/* ILLEGAL/UNDOCUMENTED OPCODE - NOP */
		case 0x10:
			cpu->cycleCounter += 4;

			break;

		/* LXI DE, d16 */
		case 0x11:
			cpuWriteWordToRegisterPair(cpu, rD, rE, cpu->readMemoryWord(cpu->memory, cpu->programCounter));

			cpu->programCounter += 2;

			cpu->cycleCounter += 10;

			break;

		/* STAX DE */
		case 0x12:
			cpu->writeMemory(cpu->memory, cpuReadRegisterPair(cpu, rD, rE), cpu->registers[rA]);

			cpu->cycleCounter += 7;

			break;

		/* INX DE */
		case 0x13:
			cpuInstructionINX(cpu, rD, rE);

			cpu->cycleCounter += 5;

			break;

		/* INR D */
		case 0x14:
			cpuInstructionINR(cpu, rD);

			cpu->cycleCounter += 5;

			break;

		/* DCR D */
		case 0x15:
			cpuInstructionDCR(cpu, rD);

			cpu->cycleCounter += 5;

			break;

		/* MVI D, d8 */
		case 0x16:
			cpuInstructionMVI(cpu, rD, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* RAL */
		case 0x17:
			temp = isBitSet(cpu->registers[rSTATUS], carryF);

			clearOrSetBit(&cpu->registers[rSTATUS], carryF, isBitSet(cpu->registers[rA], 7));

			cpu->registers[rA] <<= 1;

			clearOrSetBit(&cpu->registers[rA], 0, temp);

			cpu->cycleCounter += 4;

			break;

		/* DAD DE */
		case 0x19:
			cpuInstructionDAD(cpu, cpuReadRegisterPair(cpu, rD, rE));

			cpu->cycleCounter += 10;

			break;

		/* LDAX DE */
		case 0x1A:
			cpu->registers[rA] = cpu->readMemory(cpu->memory, cpuReadRegisterPair(cpu, rD, rE));

			cpu->cycleCounter += 7;

			break;

		/* DCX DE */
		case 0x1B:
			cpuInstructionDCX(cpu, rD, rE);

			cpu->cycleCounter += 5;

			break;

		/* INR E */
		case 0x1C:
			cpuInstructionINR(cpu, rE);

			cpu->cycleCounter += 5;

			break;

		/* DCR E */
		case 0x1D:
			cpuInstructionDCR(cpu, rE);

			cpu->cycleCounter += 5;

			break;

		/* MVI E, d8 */
		case 0x1E:
			cpuInstructionMVI(cpu, rE, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* RAR */
		case 0x1F:
			temp = isBitSet(cpu->registers[rSTATUS], carryF);

			clearOrSetBit(&cpu->registers[rSTATUS], carryF, isBitSet(cpu->registers[rA], 0));

			cpu->registers[rA] >>= 1;

			clearOrSetBit(&cpu->registers[rA], 7, temp);

			cpu->cycleCounter += 4;

			break;

		/* LXI HL, d16 */
		case 0x21:
			cpuWriteWordToRegisterPair(cpu, rH, rL, cpu->readMemoryWord(cpu->memory, cpu->programCounter));

			cpu->programCounter += 2;
			cpu->cycleCounter += 10;

			break;

		/* SHLD a16 */
		case 0x22:
			cpu->writeMemoryWord(cpu->memory, cpu->readMemoryWord(cpu->memory, cpu->programCounter), cpuReadRegisterPair(cpu, rH, rL));

			cpu->programCounter += 2;
			cpu->cycleCounter += 16;

			break;

		/* INX HL */
		case 0x23:
			cpuInstructionINX(cpu, rH, rL);

			cpu->cycleCounter += 5;

			break;

		/* INR H */
		case 0x24:
			cpuInstructionINR(cpu, rH);

			cpu->cycleCounter += 5;

			break;

		/* DCR H */
		case 0x25:
			cpuInstructionDCR(cpu, rH);

			cpu->cycleCounter += 5;

			break;

		/* MVI H, d8 */
		case 0x26:
			cpuInstructionMVI(cpu, rH, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* DAA */
		case 0x27:
			if((cpu->registers[rA] & 0x0F) > 9 || 
					isBitSet(cpu->registers[rSTATUS], auxCarryF)) {

				clearOrSetBit(&cpu->registers[rSTATUS], auxCarryF, 
						((cpu->registers[rA] & 0x0F) + 6 > 0x0F));

				cpu->registers[rA] += 6;
			}

			if((cpu->registers[rA] >> 4) > 9 || 
					isBitSet(cpu->registers[rSTATUS], carryF)) {

				clearOrSetBit(&cpu->registers[rSTATUS], carryF,
						(((uint16_t)cpu->registers[rA] + (6 << 4)) > 0xff));

				cpu->registers[rA] += (6 << 4);
			}
			
			clearOrSetParityBit(cpu, cpu->registers[rA]);
	
			clearOrSetBit(&cpu->registers[rSTATUS], zeroF, !cpu->registers[rA]);

			clearOrSetBit(&cpu->registers[rSTATUS], signF, isBitSet(cpu->registers[rA], 7));

			cpu->cycleCounter += 4;

			break;

		/* DAD HL */
		case 0x29:
			cpuInstructionDAD(cpu, cpuReadRegisterPair(cpu, rH, rL));

			cpu->cycleCounter += 10;

			break;

		/* LHLD a16 */
		case 0x2A:
			cpuWriteWordToRegisterPair(cpu, rH, rL, cpu->readMemoryWord(cpu->memory, cpu->readMemoryWord(cpu->memory, cpu->programCounter)));

			cpu->programCounter += 2;
			cpu->cycleCounter += 16;

			break;

		/* DCX HL */
		case 0x2B:
			cpuInstructionDCX(cpu, rH, rL);

			cpu->cycleCounter += 5;

			break;

		/* INR L */
		case 0x2C:
			cpuInstructionINR(cpu, rL);

			cpu->cycleCounter += 5;

			break;

		/* DCR L */
		case 0x2D:
			cpuInstructionDCR(cpu, rL);

			cpu->cycleCounter += 5;

			break;

		/* MVI L, d8 */
		case 0x2E:
			cpuInstructionMVI(cpu, rL, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* CMA */
		case 0x2F:
			cpu->registers[rA] = ~cpu->registers[rA];

			cpu->cycleCounter += 4;

			break;

		/* LXI SP, d16*/
		case 0x31:
			cpu->stackPointer = (cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8) |
				cpu->readMemory(cpu->memory, cpu->programCounter);

			cpu->programCounter += 2;
			cpu->cycleCounter += 10;

			break;

		/* STA a16 */
		case 0x32:
			cpu->writeMemory(cpu->memory, cpu->readMemoryWord(cpu->memory, cpu->programCounter), cpu->registers[rA]);

			cpu->programCounter += 2;
			cpu->cycleCounter += 13;

			break;

		/* INX SP */
		case 0x33:
			cpu->stackPointer++;

			cpu->cycleCounter += 5;

			break;

		/* INR M */
		case 0x34:
			temp = cpu->readMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL));

			/* auxiliary carry*/
			clearOrSetBit(&cpu->registers[rSTATUS], 4, ((unsigned)(temp & 0x0F) + 1) > 0x0F);

			cpu->writeMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL), ++temp);

			/* parity */
			clearOrSetParityBit(cpu, temp);
	
			/* zero */
			clearOrSetBit(&cpu->registers[rSTATUS], zeroF, !temp);

			/* sign */
			clearOrSetBit(&cpu->registers[rSTATUS], signF, isBitSet(temp, 7));

			cpu->cycleCounter += 10;

			break;

		/* DCR M */
		case 0x35:
			temp = cpu->readMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL));
			
			/* auxiliary carry*/
			clearOrSetBit(&cpu->registers[rSTATUS], 4, (temp & 0x0F) != 0);

			cpu->writeMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL), --temp);

			/* parity */
			clearOrSetParityBit(cpu, temp);
	
			/* zero */
			clearOrSetBit(&cpu->registers[rSTATUS], 6, !temp);

			/* sign */
			clearOrSetBit(&cpu->registers[rSTATUS], 7, isBitSet(temp, 7));


			cpu->cycleCounter += 10;

			break;

		/* MOV M, d8 */
		case 0x36:
			cpu->writeMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL), cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 10;

			break;

		/* STC */
		case 0x37:
			setBit(&cpu->registers[rSTATUS], carryF);

			cpu->cycleCounter += 4;

			break;

		/* DAD SP */
		case 0x39:
			cpuInstructionDAD(cpu, cpu->stackPointer);

			cpu->cycleCounter += 10;

			break;

		/* LDA a16 */
		case 0x3A:
			cpu->registers[rA] = cpu->readMemory(cpu->memory, cpu->readMemoryWord(cpu->memory, cpu->programCounter));

			cpu->programCounter += 2;
			cpu->cycleCounter += 13;

			break;

		/* DCX SP */
		case 0x3B:
			cpu->stackPointer--;

			cpu->cycleCounter += 5;

			break;

		/* INR A */
		case 0x3C:
			cpuInstructionINR(cpu, rA);

			cpu->cycleCounter += 5;

			break;

		/* DCR A */
		case 0x3D:
			cpuInstructionDCR(cpu, rA);

			cpu->cycleCounter += 5;

			break;

		/* MVI A, d8 */
		case 0x3E:
			cpuInstructionMVI(cpu, rA, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* CMC */
		case 0x3F:
			clearOrSetBit(&cpu->registers[rSTATUS], carryF, !isBitSet(cpu->registers[rSTATUS], carryF));

			cpu->cycleCounter += 4;

			break;

		/* MOV B, B */
		case 0x40:
			cpu->cycleCounter += 5;

			break;

		/* MOV B, C */
		case 0x41:
			cpuInstructionMOV(cpu, rB, rC);

			cpu->cycleCounter += 5;

			break;

		/* MOV B, D */
		case 0x42:
			cpuInstructionMOV(cpu, rB, rD);

			cpu->cycleCounter += 5;

			break;
	
		/* MOV B, E */
		case 0x43:
			cpuInstructionMOV(cpu, rB, rE);

			cpu->cycleCounter += 5;

			break;

		/* MOV B, H */
		case 0x44:
			cpuInstructionMOV(cpu, rB, rH);

			cpu->cycleCounter += 5;

			break;

		/* MOV M, L */
		case 0x45:
			cpuInstructionMOV(cpu, rB, rL);

			cpu->cycleCounter += 5;

			break;

		/* MOV B, A */
		case 0x47:
			cpuInstructionMOV(cpu, rB, rA);

			cpu->cycleCounter += 5;

			break;

		/* MOV B, M */
		case 0x46:
			cpuInstructionMOVfromM(cpu, rB);

			cpu->cycleCounter += 7;

			break;

		/* MOV C, B */
		case 0x48:
			cpuInstructionMOV(cpu, rC, rB);

			cpu->cycleCounter += 5;

			break;

		/* MOV C, C */
		case 0x49:
			cpu->cycleCounter += 5;

			break;

		/* MOV C, D */
		case 0x4A:
			cpuInstructionMOV(cpu, rC, rD);

			cpu->cycleCounter += 5;

			break;
		
		/* MOV C, E */
		case 0x4B:
			cpuInstructionMOV(cpu, rC, rE);

			cpu->cycleCounter += 5;

			break;

		/* MOV C, H */
		case 0x4C:
			cpuInstructionMOV(cpu, rC, rH);

			cpu->cycleCounter += 5;

			break;

		/* MOV C, L */
		case 0x4D:
			cpuInstructionMOV(cpu, rC, rL);

			cpu->cycleCounter += 5;

			break;

		/* MOV C, M */
		case 0x4E:
			cpuInstructionMOVfromM(cpu, rC);

			cpu->cycleCounter += 7;

			break;

		/* MOV C, A */
		case 0x4F:
			cpuInstructionMOV(cpu, rC, rA);

			cpu->cycleCounter += 5;

			break;

		/* MOV D, B */
		case 0x50:
			cpuInstructionMOV(cpu, rD, rB);

			cpu->cycleCounter += 5;

			break;

		/* MOV D, C */
		case 0x51:
			cpuInstructionMOV(cpu, rD, rC);

			cpu->cycleCounter += 5;

			break;

		/* MOV D, D */
		case 0x52:
			cpu->cycleCounter += 5;

			break;

		/* MOV D, E */
		case 0x53:
			cpuInstructionMOV(cpu, rD, rE);

			cpu->cycleCounter += 5;

			break;

		/* MOV D, H */
		case 0x54:
			cpuInstructionMOV(cpu, rD, rH);

			cpu->cycleCounter += 5;

			break;

		/* MOV D, L */
		case 0x55:
			cpuInstructionMOV(cpu, rD, rL);

			cpu->cycleCounter += 5;

			break;

		/* MOV D, M */
		case 0x56:
			cpuInstructionMOVfromM(cpu, rD);

			cpu->cycleCounter += 7;

			break;

		/* MOV D, A */
		case 0x57:
			cpuInstructionMOV(cpu, rD, rA);

			cpu->cycleCounter += 5;

			break;

		/* MOV E, B */
		case 0x58:
			cpuInstructionMOV(cpu, rE, rB);

			cpu->cycleCounter += 5;

			break;

		/* MOV E, C */
		case 0x59:
			cpuInstructionMOV(cpu, rE, rC);

			cpu->cycleCounter += 5;

			break;

		/* MOV E, D */
		case 0x5A:
			cpuInstructionMOV(cpu, rE, rD);

			cpu->cycleCounter += 5;

			break;

		/* MOV E, E */
		case 0x5B:
			cpu->cycleCounter += 5;

			break;

		/* MOV E, H */
		case 0x5C:
			cpuInstructionMOV(cpu, rE, rH);

			cpu->cycleCounter += 5;

			break;

		/* MOV E, L */
		case 0x5D:
			cpuInstructionMOV(cpu, rE, rL);

			cpu->cycleCounter += 5;

			break;

		/* MOV E, M */
		case 0x5E:
			cpuInstructionMOVfromM(cpu, rE);

			cpu->cycleCounter += 7;

			break;

		/* MOV E, A */
		case 0x5F:
			cpuInstructionMOV(cpu, rE, rA);

			cpu->cycleCounter += 5;

			break;

		/* MOV H, B */
		case 0x60:
			cpuInstructionMOV(cpu, rH, rB);

			cpu->cycleCounter += 5;

			break;

		/* MOV H, C */
		case 0x61:
			cpuInstructionMOV(cpu, rH, rC);

			cpu->cycleCounter += 5;

			break;

		/* MOV H, D */
		case 0x62:
			cpuInstructionMOV(cpu, rH, rD);

			cpu->cycleCounter += 5;

			break;

		/* MOV H, E */
		case 0x63:
			cpuInstructionMOV(cpu, rH, rE);

			cpu->cycleCounter += 5;

			break;

		/* MOV H, H */
		case 0x64:
			cpu->cycleCounter += 5;

			break;

		/* MOV H, L */
		case 0x65:
			cpuInstructionMOV(cpu, rH, rL);

			cpu->cycleCounter += 5;

			break;

		/* MOV H, M */
		case 0x66:
			cpuInstructionMOVfromM(cpu, rH);

			cpu->cycleCounter += 7;

			break;

		/* MOV H, A */
		case 0x67:
			cpuInstructionMOV(cpu, rH, rA);

			cpu->cycleCounter += 5;

			break;

		/* MOV L, B */
		case 0x68:
			cpuInstructionMOV(cpu, rL, rB);

			cpu->cycleCounter += 5;

			break;

		/* MOV L, C */
		case 0x69:
			cpuInstructionMOV(cpu, rL, rC);

			cpu->cycleCounter += 5;

			break;

		/* MOV L, D */
		case 0x6A:
			cpuInstructionMOV(cpu, rL, rD);

			cpu->cycleCounter += 5;

			break;

		/* MOV L, E */
		case 0x6B:
			cpuInstructionMOV(cpu, rL, rE);

			cpu->cycleCounter += 5;

			break;

		/* MOV L, H */
		case 0x6C:
			cpuInstructionMOV(cpu, rL, rH);

			cpu->cycleCounter += 5;

			break;

		/* MOV L, L */
		case 0x6D:
			cpu->cycleCounter += 5;

			break;

		/* MOV L, M */
		case 0x6E:
			cpuInstructionMOVfromM(cpu, rL);

			cpu->cycleCounter += 7;

			break;

		/* MOV L, A */
		case 0x6F:
			cpuInstructionMOV(cpu, rL, rA);

			cpu->cycleCounter += 5;

			break;

		/* MOV M, B */
		case 0x70:
			cpuInstructionMVItoM(cpu, cpu->registers[rB]);

			cpu->cycleCounter += 7;

			break;

		/* MOV M, C */
		case 0x71:
			cpuInstructionMVItoM(cpu, cpu->registers[rC]);

			cpu->cycleCounter += 7;

			break;

		/* MOV M, D */
		case 0x72:
			cpuInstructionMVItoM(cpu, cpu->registers[rD]);

			cpu->cycleCounter += 7;

			break;


		/* MOV M, E */
		case 0x73:
			cpuInstructionMVItoM(cpu, cpu->registers[rE]);

			cpu->cycleCounter += 7;

			break;

		/* MOV M, H */
		case 0x74:
			cpuInstructionMVItoM(cpu, cpu->registers[rH]);

			cpu->cycleCounter += 7;

			break;

		/* MOV M, L */
		case 0x75:
			cpuInstructionMVItoM(cpu, cpu->registers[rL]);

			cpu->cycleCounter += 7;

			break;

		/* MOV M, A */
		case 0x77:
			cpuInstructionMVItoM(cpu, cpu->registers[rA]);

			cpu->cycleCounter += 7;

			break;

		/* MOV A, B */
		case 0x78:
			cpuInstructionMOV(cpu, rA, rB);

			cpu->cycleCounter += 5;

			break;

		/* MOV A, C */
		case 0x79:
			cpuInstructionMOV(cpu, rA, rC);

			cpu->cycleCounter += 5;

			break;

		/* MOV A, D */
		case 0x7A:
			cpuInstructionMOV(cpu, rA, rD);

			cpu->cycleCounter += 5;

			break;

		/* MOV A, E */
		case 0x7B:
			cpuInstructionMOV(cpu, rA, rE);

			cpu->cycleCounter += 5;

			break;

		/* MOV A, H */
		case 0x7C:
			cpuInstructionMOV(cpu, rA, rH);

			cpu->cycleCounter += 5;

			break;

		/* MOV A, L */
		case 0x7D:
			cpuInstructionMOV(cpu, rA, rL);

			cpu->cycleCounter += 5;

			break;

		/* MOV A, M */
		case 0x7E:
			cpuInstructionMOVfromM(cpu, rA);

			cpu->cycleCounter += 7;

			break;

		/* MOV A, A */
		case 0x7F:
			cpu->cycleCounter += 5;

			break;

		/* ADD B */
		case 0x80:
			cpuInstructionADI(cpu, cpu->registers[rB]);

			cpu->cycleCounter += 4;

			break;

		/* ADD C */
		case 0x81:
			cpuInstructionADI(cpu, cpu->registers[rC]);

			cpu->cycleCounter += 4;

			break;

		/* ADD D */
		case 0x82:
			cpuInstructionADI(cpu, cpu->registers[rD]);

			cpu->cycleCounter += 4;

			break;

		/* ADD E */
		case 0x83:
			cpuInstructionADI(cpu, cpu->registers[rE]);

			cpu->cycleCounter += 4;

			break;

		/* ADD H */
		case 0x84:
			cpuInstructionADI(cpu, cpu->registers[rH]);

			cpu->cycleCounter += 4;

			break;

		/* ADD L */
		case 0x85:
			cpuInstructionADI(cpu, cpu->registers[rL]);

			cpu->cycleCounter += 4;

			break;

		/* ADD M */
		case 0x86:
			cpuInstructionADI(cpu, cpu->readMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL)));

			cpu->cycleCounter += 7;

			break;

		/* ADD A */
		case 0x87:
			cpuInstructionADI(cpu, cpu->registers[rA]);

			cpu->cycleCounter += 4;

			break;

		/* ADC B */
		case 0x88:
			cpuInstructionACI(cpu, cpu->registers[rB]);

			cpu->cycleCounter += 4;

			break;

		/* ADC C */
		case 0x89:
			cpuInstructionACI(cpu, cpu->registers[rC]);

			cpu->cycleCounter += 4;

			break;

		/* ADC D */
		case 0x8A:
			cpuInstructionACI(cpu, cpu->registers[rD]);

			cpu->cycleCounter += 4;

			break;

		/* ADC E */
		case 0x8B:
			cpuInstructionACI(cpu, cpu->registers[rE]);

			cpu->cycleCounter += 4;

			break;

		/* ADC H */
		case 0x8C:
			cpuInstructionACI(cpu, cpu->registers[rH]);

			cpu->cycleCounter += 4;

			break;

		/* ADC L */
		case 0x8D:
			cpuInstructionACI(cpu, cpu->registers[rL]);

			cpu->cycleCounter += 4;

			break;

		/* ADC M */
		case 0x8E:
			cpuInstructionACI(cpu, cpu->readMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL)));

			cpu->cycleCounter += 7;

			break;

		/* ADC C */
		case 0x8F:
			cpuInstructionACI(cpu, cpu->registers[rA]);

			cpu->cycleCounter += 4;

			break;

		/* SUB B */
		case 0x90:
			cpuInstructionSUI(cpu, cpu->registers[rB]);

			cpu->cycleCounter += 4;

			break;

		/* SUB C */
		case 0x91:
			cpuInstructionSUI(cpu, cpu->registers[rC]);

			cpu->cycleCounter += 4;

			break;

		/* SUB D */
		case 0x92:
			cpuInstructionSUI(cpu, cpu->registers[rD]);

			cpu->cycleCounter += 4;

			break;

		/* SUB E */
		case 0x93:
			cpuInstructionSUI(cpu, cpu->registers[rE]);

			cpu->cycleCounter += 4;

			break;

		/* SUB H */
		case 0x94:
			cpuInstructionSUI(cpu, cpu->registers[rH]);

			cpu->cycleCounter += 4;

			break;

		/* SUB L */
		case 0x95:
			cpuInstructionSUI(cpu, cpu->registers[rL]);

			cpu->cycleCounter += 4;

			break;

		/* SUB M */
		case 0x96:
			cpuInstructionSUI(cpu, cpu->readMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL)));

			cpu->cycleCounter += 7;

			break;

		/* SUB A */
		case 0x97:
			cpuInstructionSUI(cpu, cpu->registers[rA]);

			cpu->cycleCounter += 4;

			break;

		/* SBB B */
		case 0x98:
			cpuInstructionSBI(cpu, cpu->registers[rB]);

			cpu->cycleCounter += 4;

			break;

		/* SBB C */
		case 0x99:
			cpuInstructionSBI(cpu, cpu->registers[rC]);

			cpu->cycleCounter += 4;

			break;

		/* SBB D */
		case 0x9A:
			cpuInstructionSBI(cpu, cpu->registers[rD]);

			cpu->cycleCounter += 4;

			break;

		/* SBB E */
		case 0x9B:
			cpuInstructionSBI(cpu, cpu->registers[rE]);

			cpu->cycleCounter += 4;

			break;

		/* SBB H */
		case 0x9C:
			cpuInstructionSBI(cpu, cpu->registers[rH]);

			cpu->cycleCounter += 4;

			break;

		/* SBB L */
		case 0x9D:
			cpuInstructionSBI(cpu, cpu->registers[rL]);

			cpu->cycleCounter += 4;

			break;

		/* SBB M */
		case 0x9E:
			cpuInstructionSBI(cpu, cpu->readMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL)));

			cpu->cycleCounter += 7;

			break;

		/* SBB A */
		case 0x9F:
			cpuInstructionSBI(cpu, cpu->registers[rA]);

			cpu->cycleCounter += 4;

			break;

		/* ANA B */
		case 0xA0:
			cpuInstructionANI(cpu, cpu->registers[rB]);

			cpu->cycleCounter += 4;
				
			break;

		/* ANA B */
		case 0xA1:
			cpuInstructionANI(cpu, cpu->registers[rC]);

			cpu->cycleCounter += 4;
				
			break;

		/* ANA D */
		case 0xA2:
			cpuInstructionANI(cpu, cpu->registers[rD]);

			cpu->cycleCounter += 4;
				
			break;

		/* ANA E */
		case 0xA3:
			cpuInstructionANI(cpu, cpu->registers[rE]);

			cpu->cycleCounter += 4;
				
			break;

		/* ANA H */
		case 0xA4:
			cpuInstructionANI(cpu, cpu->registers[rH]);

			cpu->cycleCounter += 4;
				
			break;

		/* ANA L */
		case 0xA5:
			cpuInstructionANI(cpu, cpu->registers[rL]);

			cpu->cycleCounter += 4;
				
			break;

		/* ANA M */
		case 0xA6:
			cpuInstructionANI(cpu, cpu->readMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL)));

			cpu->cycleCounter += 7;
				
			break;

		/* ANA A */
		case 0xA7:
			cpuInstructionANI(cpu, cpu->registers[rA]);

			cpu->cycleCounter += 4;
				
			break;

		/* XRA B */
		case 0xA8:
			cpuInstructionXRI(cpu, cpu->registers[rB]);

			cpu->cycleCounter += 4;

			break;

		/* XRA C */
		case 0xA9:
			cpuInstructionXRI(cpu, cpu->registers[rC]);

			cpu->cycleCounter += 4;

			break;

		/* XRA D */
		case 0xAA:
			cpuInstructionXRI(cpu, cpu->registers[rD]);

			cpu->cycleCounter += 4;

			break;

		/* XRA E */
		case 0xAB:
			cpuInstructionXRI(cpu, cpu->registers[rE]);

			cpu->cycleCounter += 4;

			break;

		/* XRA H */
		case 0xAC:
			cpuInstructionXRI(cpu, cpu->registers[rH]);

			cpu->cycleCounter += 4;

			break;

		/* XRA L */
		case 0xAD:
			cpuInstructionXRI(cpu, cpu->registers[rL]);

			cpu->cycleCounter += 4;

			break;

		/* XRA M */
		case 0xAE:
			cpuInstructionXRI(cpu, cpu->readMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL)));

			cpu->cycleCounter += 7;

			break;

		/* XRA A */
		case 0xAF:
			cpuInstructionXRI(cpu, cpu->registers[rA]);

			cpu->cycleCounter += 4;

			break;

		/* ORA B */
		case 0xB0:
			cpuInstructionORI(cpu, cpu->registers[rB]);

			cpu->cycleCounter += 4;

			break;

		/* ORA C */
		case 0xB1:
			cpuInstructionORI(cpu, cpu->registers[rC]);

			cpu->cycleCounter += 4;

			break;

		/* ORA D */
		case 0xB2:
			cpuInstructionORI(cpu, cpu->registers[rD]);

			cpu->cycleCounter += 4;

			break;

		/* ORA E */
		case 0xB3:
			cpuInstructionORI(cpu, cpu->registers[rE]);

			cpu->cycleCounter += 4;

			break;

		/* ORA H */
		case 0xB4:
			cpuInstructionORI(cpu, cpu->registers[rH]);

			cpu->cycleCounter += 4;

			break;

		/* ORA L */
		case 0xB5:
			cpuInstructionORI(cpu, cpu->registers[rL]);

			cpu->cycleCounter += 4;

			break;

		/* ORA M */
		case 0xB6:
			cpuInstructionORI(cpu, cpu->readMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL)));

			cpu->cycleCounter += 7;

			break;

		/* ORA A */
		case 0xB7:
			cpuInstructionORI(cpu, cpu->registers[rA]);

			cpu->cycleCounter += 4;

			break;

		/* CMP B  */
		case 0xB8:
			cpuInstructionCPI(cpu, cpu->registers[rB]);

			cpu->cycleCounter += 4;

			break;

		/* CMP C  */
		case 0xB9:
			cpuInstructionCPI(cpu, cpu->registers[rC]);

			cpu->cycleCounter += 4;

			break;

		/* CMP D  */
		case 0xBA:
			cpuInstructionCPI(cpu, cpu->registers[rD]);

			cpu->cycleCounter += 4;

			break;

		/* CMP E  */
		case 0xBB:
			cpuInstructionCPI(cpu, cpu->registers[rE]);

			cpu->cycleCounter += 4;

			break;

		/* CMP H  */
		case 0xBC:
			cpuInstructionCPI(cpu, cpu->registers[rH]);

			cpu->cycleCounter += 4;

			break;

		/* CMP L  */
		case 0xBD:
			cpuInstructionCPI(cpu, cpu->registers[rL]);

			cpu->cycleCounter += 4;

			break;

		/* CMP M */ 
		case 0xBE:
			cpuInstructionCPI(cpu, cpu->readMemory(cpu->memory, cpuReadRegisterPair(cpu, rH, rL)));

			cpu->cycleCounter += 7;

			break;

		/* CMP A  */
		case 0xBF:
			cpuInstructionCPI(cpu, cpu->registers[rA]);

			cpu->cycleCounter += 4;

			break;

		/* RNZ */
		case 0xC0:
			cpuReturnIf(cpu, !isBitSet(cpu->registers[rSTATUS], zeroF));

			break;

		/* POP BC*/
		case 0xC1:
			cpuWriteWordToRegisterPair(cpu, rB, rC, cpuPopFromStack(cpu));

			cpu->cycleCounter += 10;

			break;


		/* JNZ a16 */
		case 0xC2:
			cpuJumpIf(cpu, !isBitSet(cpu->registers[rSTATUS], 6), 
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			cpu->cycleCounter += 10;

			break;

		/* JMP a16 */
		case 0xC3:
			cpuJumpToAddr(cpu, cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 |
					cpu->readMemory(cpu->memory, cpu->programCounter));

			cpu->cycleCounter += 10;

			break;

		/* CNZ a16 */
		case 0xC4:
			cpuCallIf(cpu, !isBitSet(cpu->registers[rSTATUS], zeroF),
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			break;

		/* PUSH BC */
		case 0xC5:
			cpuPushToStack(cpu, cpu->registers[rB] << 8 | cpu->registers[rC]);

			cpu->cycleCounter += 11;

			break;

		/* ADI d8*/
		case 0xC6:
			cpuInstructionADI(cpu, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* RST 0 */
		case 0xC7:
			cpuInstructionCALL(cpu, 0x0000);

			break;

		/* RZ */
		case 0xC8:
			cpuReturnIf(cpu, isBitSet(cpu->registers[rSTATUS], zeroF));

			break;

		/* RET */
		case 0xC9:
			cpuInstructionRET(cpu);

			cpu->cycleCounter += 10;

			break;

		/* JZ a16 */
		case 0xCA:
			cpuJumpIf(cpu, isBitSet(cpu->registers[rSTATUS], 6), 
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			cpu->cycleCounter += 10;

			break;

		/* CZ a16 */
		case 0xCC:
			cpuCallIf(cpu, isBitSet(cpu->registers[rSTATUS], zeroF),
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			break;


		/* CALL a16 */
		case 0xCD:
			cpuInstructionCALL(cpu, cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 |
						cpu->readMemory(cpu->memory, cpu->programCounter));

			cpu->cycleCounter += 17;

			break;

		/* ACI d8 */
		case 0xCE:
			cpuInstructionACI(cpu, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* RNC */
		case 0xD0:
			cpuReturnIf(cpu, !isBitSet(cpu->registers[rSTATUS], carryF));

			break;

		/* POP DE*/
		case 0xD1:
			cpuWriteWordToRegisterPair(cpu, rD, rE, cpuPopFromStack(cpu));

			cpu->cycleCounter += 10;

			break;

		/* JNC a16 */
		case 0xD2:
			cpuJumpIf(cpu, !isBitSet(cpu->registers[rSTATUS], 0), 
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			cpu->cycleCounter += 10;

			break;

		/* OUT d8 */
		case 0xD3:
			cpu->portOut(cpu, cpu->readMemory(cpu->memory, cpu->programCounter));

			cpu->programCounter++;
			cpu->cycleCounter += 10;

			break;

		/* CNC a16 */
		case 0xD4:
			cpuCallIf(cpu, !isBitSet(cpu->registers[rSTATUS], carryF),
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			break;

		/* PUSH DE */
		case 0xD5:
			cpuPushToStack(cpu, cpu->registers[rD] << 8 | cpu->registers[rE]);

			cpu->cycleCounter += 11;

			break;

		/* SUI d8 */
		case 0xD6:
			cpuInstructionSUI(cpu, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* RC */
		case 0xD8:
			cpuReturnIf(cpu, isBitSet(cpu->registers[rSTATUS], carryF));

			break;
			
		/* JC a16 */
		case 0xDA:
			cpuJumpIf(cpu, isBitSet(cpu->registers[rSTATUS], 0), 
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			cpu->cycleCounter += 10;

			break;

		/* IN d8 */
		case 0xDB:
			cpu->registers[rA] = cpu->portIn(cpu, cpu->readMemory(cpu->memory, cpu->programCounter));
			cpu->cycleCounter += 10;

			break;

		/* CC a16 */
		case 0xDC:
			cpuCallIf(cpu, isBitSet(cpu->registers[rSTATUS], carryF),
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			break;

		/* SBI d8 */
		case 0xDE:
			cpuInstructionSBI(cpu, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* RPO */
		case 0xE0:
			cpuReturnIf(cpu, !isBitSet(cpu->registers[rSTATUS], parityF));

			break;

		/* POP HL*/
		case 0xE1:
			cpuWriteWordToRegisterPair(cpu, rH, rL, cpuPopFromStack(cpu));

			cpu->cycleCounter += 10;

			break;

		/* JPO a16 */
		case 0xE2:
			cpuJumpIf(cpu, !isBitSet(cpu->registers[rSTATUS], 2), 
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			cpu->cycleCounter += 10;

			break;

		/* XTHL */
		case 0xE3:
			temp = cpu->registers[rH];

			cpu->registers[rH] = cpu->readMemory(cpu->memory, cpu->stackPointer + 1);
			cpu->writeMemory(cpu->memory, cpu->stackPointer + 1, temp);

			temp = cpu->registers[rL];

			cpu->registers[rL] = cpu->readMemory(cpu->memory, cpu->stackPointer);
			cpu->writeMemory(cpu->memory, cpu->stackPointer, temp);

			cpu->cycleCounter += 18;

			break;

		/* CPO a16 */
		case 0xE4:
			cpuCallIf(cpu, !isBitSet(cpu->registers[rSTATUS], parityF),
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			break;

		/* PUSH HL */
		case 0xE5:
			cpuPushToStack(cpu, cpu->registers[rH] << 8 | cpu->registers[rL]);

			cpu->cycleCounter += 11;

			break;

		/* ANI d8 */
		case 0xE6:
			cpuInstructionANI(cpu, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* RPE */
		case 0xE8:
			cpuReturnIf(cpu, isBitSet(cpu->registers[rSTATUS], parityF));

			break;

		/* PCHL */
		case 0xE9:
			cpu->programCounter = cpuReadRegisterPair(cpu, rH, rL);

			cpu->cycleCounter += 5;

			break;

		/* JPE a16 */
		case 0xEA:
			cpuJumpIf(cpu, isBitSet(cpu->registers[rSTATUS], 2), 
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			cpu->cycleCounter += 10;

			break;

		/* XCHG */
		case 0xEB:
			cpuInstructionXCHG(cpu);

			cpu->cycleCounter += 4;

			break;

		/* CPE a16 */
		case 0xEC:
			cpuCallIf(cpu, isBitSet(cpu->registers[rSTATUS], parityF),
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			break;

		/* XRI d8 */
		case 0xEE:
			cpuInstructionXRI(cpu, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* RP */
		case 0xF0:
			cpuReturnIf(cpu, !isBitSet(cpu->registers[rSTATUS], signF));

			break;

		/* POP PSW */
		case 0xF1:
			cpuWriteWordToRegisterPair(cpu, rA, rSTATUS, (cpuPopFromStack(cpu) & 0xFFD7) | 0x0002);

			cpu->cycleCounter += 10;

			break;

		/* JP a16 */
		case 0xF2:
			cpuJumpIf(cpu, !isBitSet(cpu->registers[rSTATUS], 7), 
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			cpu->cycleCounter += 10;

			break;

		/* DI */
		case 0xF3:
			/* This instruction doesn't do anything in the cpu tests */

			cpu->cycleCounter += 4;

			break;
	
		/* CP a16 */
		case 0xF4:
			cpuCallIf(cpu, !isBitSet(cpu->registers[rSTATUS], signF),
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			break;

		/* PUSH PSW */
		case 0xF5:
			cpuPushToStack(cpu, cpu->registers[rA] << 8
					| cpu->registers[rSTATUS] & 0xD7);

			cpu->cycleCounter += 11;

			break;

		/* ORI d8 */
		case 0xF6:
			cpuInstructionORI(cpu, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		/* RP */
		case 0xF8:
			cpuReturnIf(cpu, isBitSet(cpu->registers[rSTATUS], signF));

			break;

		/* SPHL */
		case 0xF9:
			cpu->stackPointer = cpuReadRegisterPair(cpu, rH, rL);

			cpu->cycleCounter += 5;

			break;

		/* JM a16 */
		case 0xFA:
			cpuJumpIf(cpu, isBitSet(cpu->registers[rSTATUS], 7), 
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			cpu->cycleCounter += 10;

			break;

		/* EI */
		case 0xFB:
			/* This instruction doesn't do anything in the cpu tests */

			cpu->cycleCounter += 4;

			break;

		/* CM a16 */
		case 0xFC:
			cpuCallIf(cpu, isBitSet(cpu->registers[rSTATUS], signF),
					cpu->readMemory(cpu->memory, cpu->programCounter + 1) << 8 | cpu->readMemory(cpu->memory, cpu->programCounter));

			break;

		/* CPI d8  */
		case 0xFE:
			cpuInstructionCPI(cpu, cpu->readMemory(cpu->memory, cpu->programCounter++));

			cpu->cycleCounter += 7;

			break;

		default:
			puts("unrecognized opcode");

			printf("opcode: %X at %d\n", opcode, cpu->programCounter);

			exit(1);
	}
}
