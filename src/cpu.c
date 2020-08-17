#include "cpu.h"
#include "util.h"
#include "memory.h"

static void cpuResetStatusRegister(void);
static void clearOrSetParityBit(uint8_t value);
static void cpuPushToStack(uint16_t data);
static uint16_t cpuPopFromStack(void);
static void cpuWriteWordToRegisterPair(uint8_t r1, uint8_t r2, uint16_t data);
static void cpuJumpToAddr(uint16_t addr);
static void cpuJumpIf(bool value, uint16_t addr);
static void cpuInstructionMVI(uint8_t r, uint8_t data);
static void cpuInstructionANI(uint8_t value);
static void cpuInstructionADI(uint8_t value);
static void cpuInstructionSUI(uint8_t value);
static void cpuInstructionCPI(uint8_t value);
static void cpuInstructionCALL(uint16_t addr);
static void cpuInstructionRET(void);
static void cpuInstructionXCHG(void);

static void cpuResetStatusRegister(void) {
	registers[rSTATUS] = 0x2;	
}

static void clearOrSetParityBit(uint8_t value) {
	uint8_t temp;

	temp = value ^ (value >> 4);
	temp ^= (temp >> 2);
	temp ^= (temp >> 1);

	clearOrSetBit(&registers[rSTATUS], parityF, (temp & 1) == 0);
}

static void cpuPushToStack(uint16_t data) {
	stackPointer -= 2;
	writeMemoryWord(stackPointer, data);
}

static uint16_t cpuPopFromStack(void) {
	uint16_t data;

	data = readMemoryWord(stackPointer);

	stackPointer += 2;

	return data;
}

static void cpuWriteWordToRegisterPair(uint8_t r1, uint8_t r2, uint16_t data) {
	registers[r1] = data >> 8;
	registers[r2] = data & 0x00FF;
}

static uint16_t cpuReadRegisterPair(uint8_t r1, uint8_t r2) {
	return registers[r1] << 8 | registers[r2];
}

static void cpuJumpToAddr(uint16_t addr) {
#ifdef _CPU_TEST
	if(addr == 0) {
		printf("\n\nthe program has jumped to address 0x0000...\nprinting cpu status...\n");

		printCpuState();

		printf("exiting...\n");

		exit(0);
	}
#endif /* #ifdef _CPU_TESTS */

	programCounter = addr;
}

static void cpuJumpIf(bool value, uint16_t addr) {
	if(value) {
		cpuJumpToAddr(addr);
	}
	else
		programCounter += 2;
}

static void cpuInstructionMVI(uint8_t r, uint8_t data) {
	registers[r] = data;
}

static void cpuInstructionMOV(uint8_t r1, uint8_t r2) {
	registers[r1] = registers[r2];
}

static void cpuInstructionMVItoM(uint8_t data) {
	writeMemory(cpuReadRegisterPair(rH, rL), data);
}

static void cpuInstructionMOVfromM(uint8_t r) {
	registers[r] = readMemory(cpuReadRegisterPair(rH, rL));
}

static void cpuInstructionANI(uint8_t value) {
	cpuResetStatusRegister();

	registers[rA] &= value;

	/* carry */
	clearBit(&registers[rSTATUS], 0);

	/* parity */
	clearOrSetParityBit(registers[rA]);

	/* auxiliary carry*/
	clearOrSetBit(&registers[rSTATUS], 4,
			isBitSet(registers[rSTATUS], 3) | isBitSet(registers[rA], 3));

	/* zero */
	clearOrSetBit(&registers[rSTATUS], 6, !registers[rA]);
	
	/* sign */
	clearOrSetBit(&registers[rSTATUS], 7, isBitSet(registers[rA], 7));
}

static void cpuInstructionORI(uint8_t value) {
	cpuResetStatusRegister();

	registers[rA] |= value;

	/* carry */
	clearBit(&registers[rSTATUS], carryF);

	/* parity */
	clearOrSetParityBit(registers[rA]);

	/* auxiliary carry*/
	clearOrSetBit(&registers[rSTATUS], auxCarryF,
			isBitSet(registers[rSTATUS], 3) | isBitSet(registers[rA], 3));

	/* zero */
	clearOrSetBit(&registers[rSTATUS], zeroF, !registers[rA]);
	
	/* sign */
	clearOrSetBit(&registers[rSTATUS], signF, isBitSet(registers[rA], 7));
}

static void cpuInstructionXRI(uint8_t value) {
	cpuResetStatusRegister();

	registers[rA] ^= value;

	/* carry */
	clearBit(&registers[rSTATUS], carryF);

	/* parity */
	clearOrSetParityBit(registers[rA]);

	/* auxiliary carry*/
	clearOrSetBit(&registers[rSTATUS], auxCarryF,
			isBitSet(registers[rSTATUS], 3) | isBitSet(registers[rA], 3));

	/* zero */
	clearOrSetBit(&registers[rSTATUS], zeroF, !registers[rA]);
	
	/* sign */
	clearOrSetBit(&registers[rSTATUS], signF, isBitSet(registers[rA], 7));
}

static void cpuAdd(uint8_t r, uint8_t value) {
	cpuResetStatusRegister();

	/* carry */
	clearOrSetBit(&registers[rSTATUS], 0, (((uint16_t)registers[r] + (uint16_t)value) > 0xff));

	/* auxiliary carry*/
	clearOrSetBit(&registers[rSTATUS], 4, ((registers[r] & 0x0F) + (value & 0x0F) > 0x0F));
	
	registers[r] += value;

	/* parity */
	clearOrSetParityBit(registers[r]);
	
	/* zero */
	clearOrSetBit(&registers[rSTATUS], 6, !registers[r]);

	/* sign */
	clearOrSetBit(&registers[rSTATUS], 7, isBitSet(registers[r], 7));
}

static void cpuInstructionADI(uint8_t value) {
	cpuAdd(rA, value);
}

static void cpuInstructionACI(uint8_t value) {
	bool carryBit;

	carryBit = isBitSet(registers[rSTATUS], carryF);

	cpuResetStatusRegister();

	/* carry */
	clearOrSetBit(&registers[rSTATUS], 0, (((uint16_t)registers[rA] + (uint16_t)value + (uint16_t)carryBit) > 0xff));

	/* auxiliary carry*/
	clearOrSetBit(&registers[rSTATUS], 4, (((registers[rA] & 0x0F) + (value & 0x0F) + ((uint8_t)carryBit)) > 0x0F));
	
	registers[rA] += (value + (uint8_t)carryBit);

	/* parity */
	clearOrSetParityBit(registers[rA]);
	
	/* zero */
	clearOrSetBit(&registers[rSTATUS], 6, !registers[rA]);

	/* sign */
	clearOrSetBit(&registers[rSTATUS], 7, isBitSet(registers[rA], 7));
}

static void cpuSubtract(uint8_t r, uint8_t value) {
	cpuResetStatusRegister();

	/* carry */
	clearOrSetBit(&registers[rSTATUS], 0, ((((int16_t)registers[r] - (int16_t)value)) < 0));

	/* auxiliary carry*/
	clearOrSetBit(&registers[rSTATUS], 4, (((registers[r] & 0x0F) - (value & 0x0F) > 0x0F)));
	
	registers[r] -= value;

	/* parity */
	clearOrSetParityBit(registers[r]);
	
	/* zero */
	clearOrSetBit(&registers[rSTATUS], 6, !registers[r]);

	/* sign */
	clearOrSetBit(&registers[rSTATUS], 7, isBitSet(registers[r], 7));
}

static void cpuInstructionSUI(uint8_t value) {
	cpuSubtract(rA, value);
}

static void cpuInstructionSBI(uint8_t value) {
	bool borrowBit;

	borrowBit = isBitSet(registers[rSTATUS], carryF);

	cpuResetStatusRegister();

	value += (int8_t)borrowBit;

	/* carry */
	clearOrSetBit(&registers[rSTATUS], 0, (((int16_t)registers[rA] - ((int16_t)value)) < 0));

	/* auxiliary carry*/
	clearOrSetBit(&registers[rSTATUS], 4, ((registers[rA] & 0x0F) - (value & 0x0F) > 0x0F));
	
	registers[rA] -= value;

	/* parity */
	clearOrSetParityBit(registers[rA]);
	
	/* zero */
	clearOrSetBit(&registers[rSTATUS], 6, !registers[rA]);

	/* sign */
	clearOrSetBit(&registers[rSTATUS], 7, isBitSet(registers[rA], 7));
}

static void cpuInstructionCPI(uint8_t value) {
	uint8_t temp;

	temp = registers[rA];

	cpuInstructionSUI(value);

	registers[rA] = temp;
}

static void cpuInstructionCALL(uint16_t addr) {
	cpuPushToStack(programCounter+2);
	cpuJumpToAddr(addr);

#ifdef _CPU_TEST
	if(programCounter == 5) {
		if(registers[rC] == 2)
			printf("%c", registers[rE]);
		if(registers[rC] == 9) {
			uint16_t i = (registers[rD] << 8 | registers[rE] & 0x00FF);
			while(readMemory(i) != '$')
				printf("%c", readMemory(i++));
		}
		
		cpuInstructionRET();
	}
#endif
}

static void cpuCallIf(bool value, uint16_t addr) {
	if(value) {
		cpuInstructionCALL(addr);
		cycleCounter += 17;
	}
	else {
		programCounter += 2;
		cycleCounter += 11;
	}
}

static void cpuInstructionRET(void) {
	programCounter = cpuPopFromStack(); 
}

static void cpuReturnIf(bool value) {
	if(value) {
		cpuInstructionRET();
		cycleCounter += 11;
	}
	else
		cycleCounter += 5;
}

static void cpuInstructionXCHG(void) {
	uint8_t r1, r2;

	r1 = registers[rD];
	r2 = registers[rE];

	registers[rD] = registers[rH];
	registers[rE] = registers[rL];
	
	registers[rH] = r1;
	registers[rL] = r2;
}

static void cpuInstructionINR(uint8_t r) {
	cpuAdd(r, 1);
}

static void cpuInstructionINX(uint8_t r1, uint8_t r2) {
	cpuWriteWordToRegisterPair(r1, r2, cpuReadRegisterPair(r1, r2) + 1);
}

static void cpuInstructionDCR(uint8_t r) {
	cpuSubtract(r, 1);
}

static void cpuInstructionDCX(uint8_t r1, uint8_t r2) {
	cpuWriteWordToRegisterPair(r1, r2, cpuReadRegisterPair(r1, r2) - 1);
}

static void cpuInstructionDAD(uint16_t data) {
	clearBit(&registers[rSTATUS], carryF);
	clearOrSetBit(&registers[rSTATUS], 0, (((uint32_t)cpuReadRegisterPair(rH, rL) + (uint32_t)data) > UINT16_MAX));

	cpuWriteWordToRegisterPair(rH, rL, cpuReadRegisterPair(rH, rL) + data);
}

void cpuExecuteInstruction(void) {
	uint8_t opcode,
		temp;

	opcode = readMemory(programCounter);

#ifdef _VERBOSE_DEBUG
	printf("pc: %04X opcode: %02X\n", programCounter, opcode);
#endif /* #ifdef _VERBOSE_DEBUG */

	programCounter++;

	switch(opcode) {
		/* NOP */
		case 0x00:
			cycleCounter += 4;

			break;

		/* LXI BC, d16 */
		case 0x01:
			cpuWriteWordToRegisterPair(rB, rC, readMemoryWord(programCounter));

			programCounter += 2;

			cycleCounter += 10;

			break;

		/* STAX BC */
		case 0x02:
			writeMemory(cpuReadRegisterPair(rB, rC), registers[rA]);

			cycleCounter += 7;

			break;

		/* INX BC */
		case 0x03:
			cpuInstructionINX(rB, rC);

			cycleCounter += 5;

			break;

		/* INR B */
		case 0x04:
			cpuInstructionINR(rB);

			cycleCounter += 5;

			break;

		/* DCR B */
		case 0x05:
			cpuInstructionDCR(rB);

			cycleCounter += 5;

			break;

		/* MVI B, d8 */
		case 0x06:
			cpuInstructionMVI(rB, readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* RLC */
		case 0x07:
			clearOrSetBit(&registers[rSTATUS], carryF, isBitSet(registers[rA], 7));

			temp = isBitSet(registers[rA], 7);

			registers[rA] <<= 1;

			clearOrSetBit(&registers[rA], 0, temp);

			cycleCounter += 4;

			break;

		/* DAD BC */
		case 0x09:
			cpuInstructionDAD(cpuReadRegisterPair(rB, rC));

			cycleCounter += 10;

			break;

		/* LDAX BC */
		case 0x0A:
			registers[rA] = readMemory(cpuReadRegisterPair(rB, rC));

			cycleCounter += 7;

			break;

		/* DCX BC */
		case 0x0B:
			cpuInstructionDCX(rB, rC);

			cycleCounter += 5;

			break;

		/* INR C */
		case 0x0C:
			cpuInstructionINR(rC);

			cycleCounter += 5;

			break;

		/* DCR C */
		case 0x0D:
			cpuInstructionDCR(rC);

			cycleCounter += 5;

			break;

		/* MVI C, d8 */
		case 0x0E:
			cpuInstructionMVI(rC, readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* RRC */
		case 0x0F:
			clearOrSetBit(&registers[rSTATUS], carryF, isBitSet(registers[rA], 0));

			temp = isBitSet(registers[rA], 0);

			registers[rA] >>= 1;

			clearOrSetBit(&registers[rA], 7, temp);

			cycleCounter += 4;

			break;

		/* LXI DE, d16 */
		case 0x11:
			cpuWriteWordToRegisterPair(rD, rE, readMemoryWord(programCounter));

			programCounter += 2;

			cycleCounter += 10;

			break;

		/* STAX DE */
		case 0x12:
			writeMemory(cpuReadRegisterPair(rD, rE), registers[rA]);

			cycleCounter += 7;

			break;

		/* INX DE */
		case 0x13:
			cpuInstructionINX(rD, rE);

			cycleCounter += 5;

			break;

		/* INR D */
		case 0x14:
			cpuInstructionINR(rD);

			cycleCounter += 5;

			break;

		/* DCR D */
		case 0x15:
			cpuInstructionDCR(rD);

			cycleCounter += 5;

			break;

		/* MVI D, d8 */
		case 0x16:
			cpuInstructionMVI(rD, readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* RAL */
		case 0x17:
			temp = isBitSet(registers[rSTATUS], carryF);

			clearOrSetBit(&registers[rSTATUS], carryF, isBitSet(registers[rA], 7));

			registers[rA] <<= 1;

			clearOrSetBit(&registers[rA], 0, temp);

			cycleCounter += 4;

			break;

		/* DAD DE */
		case 0x19:
			cpuInstructionDAD(cpuReadRegisterPair(rD, rE));

			cycleCounter += 10;

			break;

		/* LDAX DE */
		case 0x1A:
			registers[rA] = readMemory(cpuReadRegisterPair(rD, rE));

			cycleCounter += 7;

			break;

		/* DCX DE */
		case 0x1B:
			cpuInstructionDCX(rD, rE);

			cycleCounter += 5;

			break;

		/* INR E */
		case 0x1C:
			cpuInstructionINR(rE);

			cycleCounter += 5;

			break;

		/* DCR E */
		case 0x1D:
			cpuInstructionDCR(rE);

			cycleCounter += 5;

			break;

		/* MVI E, d8 */
		case 0x1E:
			cpuInstructionMVI(rE, readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* RAR */
		case 0x1F:
			temp = isBitSet(registers[rSTATUS], carryF);

			clearOrSetBit(&registers[rSTATUS], carryF, isBitSet(registers[rA], 0));

			registers[rA] >>= 1;

			clearOrSetBit(&registers[rA], 7, temp);

			cycleCounter += 4;

			break;

		/* LXI HL, d16 */
		case 0x21:
			cpuWriteWordToRegisterPair(rH, rL, readMemoryWord(programCounter));

			programCounter += 2;
			cycleCounter += 10;

			break;

		/* SHLD a16 */
		case 0x22:
			writeMemoryWord(readMemoryWord(programCounter), cpuReadRegisterPair(rH, rL));

			programCounter += 2;
			cycleCounter += 16;

			break;

		/* INX HL */
		case 0x23:
			cpuInstructionINX(rH, rL);

			cycleCounter += 5;

			break;

		/* INR H */
		case 0x24:
			cpuInstructionINR(rH);

			cycleCounter += 5;

			break;

		/* DCR H */
		case 0x25:
			cpuInstructionDCR(rH);

			cycleCounter += 5;

			break;

		/* MVI H, d8 */
		case 0x26:
			cpuInstructionMVI(rH, readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* DAA */
		case 0x27:
			if((registers[rA] & 0x0F) > 9 || 
					isBitSet(registers[rSTATUS], auxCarryF)) {

				clearOrSetBit(&registers[rSTATUS], auxCarryF, 
						((registers[rA] & 0x0F) + 6 > 0x0F));

				registers[rA] += 6;
			}

			if((registers[rA] >> 4) > 9 || 
					isBitSet(registers[rSTATUS], carryF)) {

				clearOrSetBit(&registers[rSTATUS], carryF,
						(((uint16_t)registers[rA] + (6 << 4)) > 0xff));

				registers[rA] += (6 << 4);
			}
			
			clearOrSetParityBit(registers[rA]);
	
			clearOrSetBit(&registers[rSTATUS], zeroF, !registers[rA]);

			clearOrSetBit(&registers[rSTATUS], signF, isBitSet(registers[rA], 7));

			cycleCounter += 4;

			break;

		/* DAD HL */
		case 0x29:
			cpuInstructionDAD(cpuReadRegisterPair(rH, rL));

			cycleCounter += 10;

			break;

		/* LHLD a16 */
		case 0x2A:
			cpuWriteWordToRegisterPair(rH, rL, readMemoryWord(readMemoryWord(programCounter)));

			programCounter += 2;
			cycleCounter += 16;

			break;

		/* DCX HL */
		case 0x2B:
			cpuInstructionDCX(rH, rL);

			cycleCounter += 5;

			break;

		/* INR L */
		case 0x2C:
			cpuInstructionINR(rL);

			cycleCounter += 5;

			break;

		/* DCR L */
		case 0x2D:
			cpuInstructionDCR(rL);

			cycleCounter += 5;

			break;

		/* MVI L, d8 */
		case 0x2E:
			cpuInstructionMVI(rL, readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* CMA */
		case 0x2F:
			registers[rA] = ~registers[rA];

			cycleCounter += 4;

			break;

		/* LXI SP, d16*/
		case 0x31:
			stackPointer = (readMemory(programCounter + 1) << 8) |
				readMemory(programCounter);

			programCounter += 2;
			cycleCounter += 10;

			break;

		/* STA a16 */
		case 0x32:
			writeMemory(readMemoryWord(programCounter), registers[rA]);

			programCounter += 2;
			cycleCounter += 13;

			break;

		/* INX SP */
		case 0x33:
			stackPointer++;

			cycleCounter += 5;

			break;

		/* INR M */
		case 0x34:
			writeMemory(cpuReadRegisterPair(rH, rL),
					readMemory(cpuReadRegisterPair(rH, rL)) + 1);

			cycleCounter += 10;

			break;

		/* DCR M */
		case 0x35:
			writeMemory(cpuReadRegisterPair(rH, rL),
					readMemory(cpuReadRegisterPair(rH, rL)) - 1);

			cycleCounter += 10;

			break;

		/* MOV M, d8 */
		case 0x36:
			writeMemory(cpuReadRegisterPair(rH, rL), readMemory(programCounter++));

			cycleCounter += 10;

			break;

		/* STC */
		case 0x37:
			setBit(&registers[rSTATUS], carryF);

			cycleCounter += 4;

			break;

		/* DAD SP */
		case 0x39:
			cpuInstructionDAD(stackPointer);

			cycleCounter += 10;

			break;

		/* LDA a16 */
		case 0x3A:
			registers[rA] = readMemory(readMemoryWord(programCounter));

			programCounter += 2;
			cycleCounter += 13;

			break;

		/* DCX SP */
		case 0x3B:
			stackPointer--;

			cycleCounter += 5;

			break;

		/* INR A */
		case 0x3C:
			cpuInstructionINR(rA);

			cycleCounter += 5;

			break;

		/* DCR A */
		case 0x3D:
			cpuInstructionDCR(rA);

			cycleCounter += 5;

			break;

		/* MVI A, d8 */
		case 0x3E:
			cpuInstructionMVI(rA, readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* CMC */
		case 0x3F:
			clearOrSetBit(&registers[rSTATUS], carryF, !isBitSet(registers[rSTATUS], carryF));

			break;

		/* MOV B, C */
		case 0x41:
			cpuInstructionMOV(rB, rC);

			cycleCounter += 5;

			break;

		/* MOV B, D */
		case 0x42:
			cpuInstructionMOV(rB, rD);

			cycleCounter += 5;

			break;
	
		/* MOV B, E */
		case 0x43:
			cpuInstructionMOV(rB, rE);

			cycleCounter += 5;

			break;

		/* MOV B, H */
		case 0x44:
			cpuInstructionMOV(rB, rH);

			cycleCounter += 5;

			break;

		/* MOV M, L */
		case 0x45:
			cpuInstructionMOV(rB, rL);

			cycleCounter += 5;

			break;

		/* MOV B, A */
		case 0x47:
			cpuInstructionMOV(rB, rA);

			cycleCounter += 5;

			break;

		/* MOV B, M */
		case 0x46:
			cpuInstructionMOVfromM(rB);

			cycleCounter += 7;

			break;

		/* MOV C, B */
		case 0x48:
			cpuInstructionMOV(rC, rB);

			cycleCounter += 5;

			break;

		/* MOV C, D */
		case 0x4A:
			cpuInstructionMOV(rC, rD);

			cycleCounter += 5;

			break;
		
		/* MOV C, E */
		case 0x4B:
			cpuInstructionMOV(rC, rE);

			cycleCounter += 5;

			break;

		/* MOV C, H */
		case 0x4C:
			cpuInstructionMOV(rC, rH);

			cycleCounter += 5;

			break;

		/* MOV C, L */
		case 0x4D:
			cpuInstructionMOV(rC, rL);

			cycleCounter += 5;

			break;

		/* MOV C, M */
		case 0x4E:
			cpuInstructionMOVfromM(rC);

			cycleCounter += 7;

			break;

		/* MOV C, A */
		case 0x4F:
			cpuInstructionMOV(rC, rA);

			cycleCounter += 5;

			break;

		/* MOV D, B */
		case 0x50:
			cpuInstructionMOV(rD, rB);

			cycleCounter += 5;

			break;

		/* MOV D, C */
		case 0x51:
			cpuInstructionMOV(rD, rC);

			cycleCounter += 5;

			break;

		/* MOV D, E */
		case 0x53:
			cpuInstructionMOV(rD, rE);

			cycleCounter += 5;

			break;

		/* MOV D, H */
		case 0x54:
			cpuInstructionMOV(rD, rH);

			cycleCounter += 5;

			break;

		/* MOV D, L */
		case 0x55:
			cpuInstructionMOV(rD, rL);

			cycleCounter += 5;

			break;

		/* MOV D, M */
		case 0x56:
			cpuInstructionMOVfromM(rD);

			cycleCounter += 7;

			break;

		/* MOV D, A */
		case 0x57:
			cpuInstructionMOV(rD, rA);

			cycleCounter += 5;

			break;

		/* MOV E, B */
		case 0x58:
			cpuInstructionMOV(rE, rB);

			cycleCounter += 5;

			break;

		/* MOV E, C */
		case 0x59:
			cpuInstructionMOV(rE, rC);

			cycleCounter += 5;

			break;

		/* MOV E, D */
		case 0x5A:
			cpuInstructionMOV(rE, rD);

			cycleCounter += 7;

			break;

		/* MOV E, H */
		case 0x5C:
			cpuInstructionMOV(rE, rH);

			cycleCounter += 5;

			break;

		/* MOV E, L */
		case 0x5D:
			cpuInstructionMOV(rE, rL);

			cycleCounter += 5;

			break;

		/* MOV E, M */
		case 0x5E:
			cpuInstructionMOVfromM(rE);

			cycleCounter += 7;

			break;

		/* MOV E, A */
		case 0x5F:
			cpuInstructionMOV(rE, rA);

			cycleCounter += 5;

			break;

		/* MOV H, B */
		case 0x60:
			cpuInstructionMOV(rH, rB);

			cycleCounter += 5;

			break;

		/* MOV H, C */
		case 0x61:
			cpuInstructionMOV(rH, rC);

			cycleCounter += 5;

			break;

		/* MOV H, D */
		case 0x62:
			cpuInstructionMOV(rH, rD);

			cycleCounter += 5;

			break;

		/* MOV H, E */
		case 0x63:
			cpuInstructionMOV(rH, rE);

			cycleCounter += 5;

			break;

		/* MOV H, L */
		case 0x65:
			cpuInstructionMOV(rH, rL);

			cycleCounter += 5;

			break;

		/* MOV H, M */
		case 0x66:
			cpuInstructionMOVfromM(rH);

			cycleCounter += 7;

			break;

		/* MOV H, A */
		case 0x67:
			cpuInstructionMOV(rH, rA);

			cycleCounter += 5;

			break;

		/* MOV L, B */
		case 0x68:
			cpuInstructionMOV(rL, rB);

			cycleCounter += 5;

			break;

		/* MOV L, C */
		case 0x69:
			cpuInstructionMOV(rL, rC);

			cycleCounter += 5;

			break;

		/* MOV L, D */
		case 0x6A:
			cpuInstructionMOV(rL, rD);

			cycleCounter += 5;

			break;

		/* MOV L, E */
		case 0x6B:
			cpuInstructionMOV(rL, rE);

			cycleCounter += 5;

			break;

		/* MOV L, H */
		case 0x6C:
			cpuInstructionMOV(rL, rH);

			cycleCounter += 5;

			break;

		/* MOV L, M */
		case 0x6E:
			cpuInstructionMOVfromM(rL);

			cycleCounter += 7;

			break;

		/* MOV L, A */
		case 0x6F:
			cpuInstructionMOV(rL, rA);

			cycleCounter += 5;

			break;

		/* MOV M, B */
		case 0x70:
			cpuInstructionMVItoM(registers[rB]);

			cycleCounter += 7;

			break;

		/* MOV M, C */
		case 0x71:
			cpuInstructionMVItoM(registers[rC]);

			cycleCounter += 7;

			break;

		/* MOV M, D */
		case 0x72:
			cpuInstructionMVItoM(registers[rD]);

			cycleCounter += 7;

			break;


		/* MOV M, E */
		case 0x73:
			cpuInstructionMVItoM(registers[rE]);

			cycleCounter += 7;

			break;

		/* MOV M, H */
		case 0x74:
			cpuInstructionMVItoM(registers[rH]);

			cycleCounter += 7;

			break;

		/* MOV M, L */
		case 0x75:
			cpuInstructionMVItoM(registers[rL]);

			cycleCounter += 7;

			break;

		/* MOV M, A */
		case 0x77:
			cpuInstructionMVItoM(registers[rA]);

			cycleCounter += 7;

			break;

		/* MOV A, B */
		case 0x78:
			cpuInstructionMOV(rA, rB);

			cycleCounter += 5;

			break;

		/* MOV A, C */
		case 0x79:
			cpuInstructionMOV(rA, rC);

			cycleCounter += 5;

			break;

		/* MOV A, D */
		case 0x7A:
			cpuInstructionMOV(rA, rD);

			cycleCounter += 5;

			break;

		/* MOV A, E */
		case 0x7B:
			cpuInstructionMOV(rA, rE);

			cycleCounter += 5;

			break;

		/* MOV A, H */
		case 0x7C:
			cpuInstructionMOV(rA, rH);

			cycleCounter += 5;

			break;

		/* MOV A, L */
		case 0x7D:
			cpuInstructionMOV(rA, rL);

			cycleCounter += 5;

			break;

		/* MOV A, M */
		case 0x7E:
			cpuInstructionMOVfromM(rA);

			cycleCounter += 7;

			break;

		/* ADD B */
		case 0x80:
			cpuInstructionADI(registers[rB]);

			cycleCounter += 4;

			break;

		/* ADD C */
		case 0x81:
			cpuInstructionADI(registers[rC]);

			cycleCounter += 4;

			break;

		/* ADD D */
		case 0x82:
			cpuInstructionADI(registers[rD]);

			cycleCounter += 4;

			break;

		/* ADD E */
		case 0x83:
			cpuInstructionADI(registers[rE]);

			cycleCounter += 4;

			break;

		/* ADD H */
		case 0x84:
			cpuInstructionADI(registers[rH]);

			cycleCounter += 4;

			break;

		/* ADD L */
		case 0x85:
			cpuInstructionADI(registers[rL]);

			cycleCounter += 4;

			break;

		/* ADD M */
		case 0x86:
			cpuInstructionADI(readMemory(cpuReadRegisterPair(rH, rL)));

			cycleCounter += 4;

			break;

		/* ADD A */
		case 0x87:
			cpuInstructionADI(registers[rA]);

			cycleCounter += 4;

			break;

		/* ADC B */
		case 0x88:
			cpuInstructionACI(registers[rB]);

			cycleCounter += 4;

			break;

		/* ADC C */
		case 0x89:
			cpuInstructionACI(registers[rC]);

			cycleCounter += 4;

			break;

		/* ADC D */
		case 0x8A:
			cpuInstructionACI(registers[rD]);

			cycleCounter += 4;

			break;

		/* ADC E */
		case 0x8B:
			cpuInstructionACI(registers[rE]);

			cycleCounter += 4;

			break;

		/* ADC H */
		case 0x8C:
			cpuInstructionACI(registers[rH]);

			cycleCounter += 4;

			break;

		/* ADC L */
		case 0x8D:
			cpuInstructionACI(registers[rL]);

			cycleCounter += 4;

			break;

		/* ADC M */
		case 0x8E:
			cpuInstructionACI(readMemory(cpuReadRegisterPair(rH, rL)));

			cycleCounter += 7;

			break;

		/* ADC C */
		case 0x8F:
			cpuInstructionACI(registers[rA]);

			cycleCounter += 4;

			break;

		/* SUB B */
		case 0x90:
			cpuInstructionSUI(registers[rB]);

			cycleCounter += 4;

			break;

		/* SUB C */
		case 0x91:
			cpuInstructionSUI(registers[rC]);

			cycleCounter += 4;

			break;

		/* SUB D */
		case 0x92:
			cpuInstructionSUI(registers[rD]);

			cycleCounter += 4;

			break;

		/* SUB E */
		case 0x93:
			cpuInstructionSUI(registers[rE]);

			cycleCounter += 4;

			break;

		/* SUB H */
		case 0x94:
			cpuInstructionSUI(registers[rH]);

			cycleCounter += 4;

			break;

		/* SUB L */
		case 0x95:
			cpuInstructionSUI(registers[rL]);

			cycleCounter += 4;

			break;

		/* SUB M */
		case 0x96:
			cpuInstructionSUI(readMemory(cpuReadRegisterPair(rH, rL)));

			cycleCounter += 4;

			break;

		/* SUB A */
		case 0x97:
			cpuInstructionSUI(registers[rA]);

			cycleCounter += 4;

			break;

		/* SBB B */
		case 0x98:
			cpuInstructionSBI(registers[rB]);

			cycleCounter += 4;

			break;

		/* SBB C */
		case 0x99:
			cpuInstructionSBI(registers[rC]);

			cycleCounter += 4;

			break;

		/* SBB D */
		case 0x9A:
			cpuInstructionSBI(registers[rD]);

			cycleCounter += 4;

			break;

		/* SBB E */
		case 0x9B:
			cpuInstructionSBI(registers[rE]);

			cycleCounter += 4;

			break;

		/* SBB H */
		case 0x9C:
			cpuInstructionSBI(registers[rH]);

			cycleCounter += 4;

			break;

		/* SBB L */
		case 0x9D:
			cpuInstructionSBI(registers[rL]);

			cycleCounter += 4;

			break;

		/* SBB M */
		case 0x9E:
			cpuInstructionSBI(readMemory(cpuReadRegisterPair(rH, rL)));

			cycleCounter += 4;

			break;

		/* SBB A */
		case 0x9F:
			cpuInstructionSBI(registers[rA]);

			cycleCounter += 4;

			break;

		/* ANA B */
		case 0xA0:
			cpuInstructionANI(registers[rB]);

			cycleCounter += 4;
				
			break;

		/* ANA B */
		case 0xA1:
			cpuInstructionANI(registers[rC]);

			cycleCounter += 4;
				
			break;

		/* ANA D */
		case 0xA2:
			cpuInstructionANI(registers[rD]);

			cycleCounter += 4;
				
			break;

		/* ANA E */
		case 0xA3:
			cpuInstructionANI(registers[rE]);

			cycleCounter += 4;
				
			break;

		/* ANA H */
		case 0xA4:
			cpuInstructionANI(registers[rH]);

			cycleCounter += 4;
				
			break;

		/* ANA L */
		case 0xA5:
			cpuInstructionANI(registers[rL]);

			cycleCounter += 4;
				
			break;

		/* ANA M */
		case 0xA6:
			cpuInstructionANI(readMemory(cpuReadRegisterPair(rH, rL)));

			cycleCounter += 4;
				
			break;

		/* ANA H */
		case 0xA7:
			cpuInstructionANI(registers[rH]);

			cycleCounter += 4;
				
			break;

		/* XRA B */
		case 0xA8:
			cpuInstructionXRI(registers[rB]);

			cycleCounter += 4;

			break;

		/* XRA C */
		case 0xA9:
			cpuInstructionXRI(registers[rC]);

			cycleCounter += 4;

			break;

		/* XRA D */
		case 0xAA:
			cpuInstructionXRI(registers[rD]);

			cycleCounter += 4;

			break;

		/* XRA E */
		case 0xAB:
			cpuInstructionXRI(registers[rE]);

			cycleCounter += 4;

			break;

		/* XRA H */
		case 0xAC:
			cpuInstructionXRI(registers[rH]);

			cycleCounter += 4;

			break;

		/* XRA L */
		case 0xAD:
			cpuInstructionXRI(registers[rL]);

			cycleCounter += 4;

			break;

		/* XRA M */
		case 0xAE:
			cpuInstructionXRI(readMemory(cpuReadRegisterPair(rH, rL)));

			cycleCounter += 4;

			break;

		/* XRA A */
		case 0xAF:
			cpuInstructionXRI(registers[rA]);

			cycleCounter += 4;

			break;

		/* ORA B */
		case 0xB0:
			cpuInstructionORI(registers[rB]);

			cycleCounter += 4;

			break;

		/* ORA C */
		case 0xB1:
			cpuInstructionORI(registers[rC]);

			cycleCounter += 4;

			break;

		/* ORA D */
		case 0xB2:
			cpuInstructionORI(registers[rD]);

			cycleCounter += 4;

			break;

		/* ORA E */
		case 0xB3:
			cpuInstructionORI(registers[rE]);

			cycleCounter += 4;

			break;

		/* ORA H */
		case 0xB4:
			cpuInstructionORI(registers[rH]);

			cycleCounter += 4;

			break;

		/* ORA L */
		case 0xB5:
			cpuInstructionORI(registers[rL]);

			cycleCounter += 4;

			break;

		/* ORA M */
		case 0xB6:
			cpuInstructionORI(readMemory(cpuReadRegisterPair(rH, rL)));

			cycleCounter += 4;

			break;

		/* ORA A */
		case 0xB7:
			cpuInstructionORI(registers[rA]);

			cycleCounter += 4;

			break;

		/* CMP B  */
		case 0xB8:
			cpuInstructionCPI(registers[rB]);

			cycleCounter += 4;

			break;

		/* CMP C  */
		case 0xB9:
			cpuInstructionCPI(registers[rC]);

			cycleCounter += 4;

			break;

		/* CMP D  */
		case 0xBA:
			cpuInstructionCPI(registers[rD]);

			cycleCounter += 4;

			break;

		/* CMP E  */
		case 0xBB:
			cpuInstructionCPI(registers[rE]);

			cycleCounter += 4;

			break;

		/* CMP H  */
		case 0xBC:
			cpuInstructionCPI(registers[rH]);

			cycleCounter += 4;

			break;

		/* CMP L  */
		case 0xBD:
			cpuInstructionCPI(registers[rL]);

			cycleCounter += 4;

			break;

		/* CMP M */ 
		case 0xBE:
			cpuInstructionCPI(readMemory(cpuReadRegisterPair(rH, rL)));

			cycleCounter += 4;

			break;

		/* CMP A  */
		case 0xBF:
			cpuInstructionCPI(registers[rA]);

			cycleCounter += 4;

			break;

		/* RNZ */
		case 0xC0:
			cpuReturnIf(!isBitSet(registers[rSTATUS], zeroF));

			break;

		/* POP BC*/
		case 0xC1:
			cpuWriteWordToRegisterPair(rB, rC, cpuPopFromStack());

			cycleCounter += 10;

			break;


		/* JNZ a16 */
		case 0xC2:
			cpuJumpIf(!isBitSet(registers[rSTATUS], 6), 
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			cycleCounter += 10;

			break;

		/* JMP a16 */
		case 0xC3:
			cpuJumpToAddr(readMemory(programCounter + 1) << 8 |
					readMemory(programCounter));

			cycleCounter += 10;

			break;

		/* CNZ a16 */
		case 0xC4:
			cpuCallIf(!isBitSet(registers[rSTATUS], zeroF),
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			break;

		/* PUSH BC */
		case 0xC5:
			cpuPushToStack(registers[rB] << 8 | registers[rC]);

			cycleCounter += 11;

			break;

		/* ADI d8*/
		case 0xC6:
			cpuInstructionADI(readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* RST 0 */
		case 0xC7:
			cpuInstructionCALL(0x0000);

			break;

		/* RZ */
		case 0xC8:
			cpuReturnIf(isBitSet(registers[rSTATUS], zeroF));

			break;

		/* RET */
		case 0xC9:
			cpuInstructionRET();

			cycleCounter += 10;

			break;

		/* JZ a16 */
		case 0xCA:
			cpuJumpIf(isBitSet(registers[rSTATUS], 6), 
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			cycleCounter += 10;

			break;

		/* CZ a16 */
		case 0xCC:
			cpuCallIf(isBitSet(registers[rSTATUS], zeroF),
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			break;


		/* CALL a16 */
		case 0xCD:
			cpuInstructionCALL(readMemory(programCounter + 1) << 8 |
						readMemory(programCounter));

			cycleCounter += 17;

			break;

		/* ACI d8 */
		case 0xCE:
			cpuInstructionACI(readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* RNC */
		case 0xD0:
			cpuReturnIf(!isBitSet(registers[rSTATUS], carryF));

			break;

		/* POP DE*/
		case 0xD1:
			cpuWriteWordToRegisterPair(rD, rE, cpuPopFromStack());

			cycleCounter += 10;

			break;

		/* JNC a16 */
		case 0xD2:
			cpuJumpIf(!isBitSet(registers[rSTATUS], 0), 
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			cycleCounter += 10;

			break;

		/* CNC a16 */
		case 0xD4:
			cpuCallIf(!isBitSet(registers[rSTATUS], carryF),
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			break;

		/* PUSH DE */
		case 0xD5:
			cpuPushToStack(registers[rD] << 8 | registers[rE]);

			cycleCounter += 11;

			break;

		/* SUI d8 */
		case 0xD6:
			cpuInstructionSUI(readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* RC */
		case 0xD8:
			cpuReturnIf(isBitSet(registers[rSTATUS], carryF));

			break;
			
		/* JC a16 */
		case 0xDA:
			cpuJumpIf(isBitSet(registers[rSTATUS], 0), 
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			cycleCounter += 10;

			break;

		/* CC a16 */
		case 0xDC:
			cpuCallIf(isBitSet(registers[rSTATUS], carryF),
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			break;

		/* SBI d8 */
		case 0xDE:
			cpuInstructionSBI(readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* RPO */
		case 0xE0:
			cpuReturnIf(!isBitSet(registers[rSTATUS], parityF));

			break;

		/* POP HL*/
		case 0xE1:
			cpuWriteWordToRegisterPair(rH, rL, cpuPopFromStack());

			cycleCounter += 10;

			break;

		/* JPO a16 */
		case 0xE2:
			cpuJumpIf(!isBitSet(registers[rSTATUS], 2), 
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			cycleCounter += 10;

			break;

		/* XTHL */
		case 0xE3:
			temp = registers[rH];

			registers[rH] = readMemory(stackPointer + 1);
			writeMemory(stackPointer + 1, temp);

			temp = registers[rL];

			registers[rL] = readMemory(stackPointer);
			writeMemory(stackPointer, temp);

			cycleCounter += 17;

			break;

		/* CPO a16 */
		case 0xE4:
			cpuCallIf(!isBitSet(registers[rSTATUS], parityF),
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			break;

		/* PUSH HL */
		case 0xE5:
			cpuPushToStack(registers[rH] << 8 | registers[rL]);

			cycleCounter += 13;

			break;

		/* ANI d8 */
		case 0xE6:
			cpuInstructionANI(readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* RPE */
		case 0xE8:
			cpuReturnIf(isBitSet(registers[rSTATUS], parityF));

			break;

		/* PCHL */
		case 0xE9:
			programCounter = cpuReadRegisterPair(rH, rL);

			cycleCounter += 5;

			break;

		/* JPE a16 */
		case 0xEA:
			cpuJumpIf(isBitSet(registers[rSTATUS], 2), 
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			cycleCounter += 10;

			break;

		/* XCHG */
		case 0xEB:
			cpuInstructionXCHG();

			cycleCounter += 4;

			break;

		/* CPE a16 */
		case 0xEC:
			cpuCallIf(isBitSet(registers[rSTATUS], parityF),
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			break;

		/* XRI d8 */
		case 0xEE:
			cpuInstructionXRI(readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* RP */
		case 0xF0:
			cpuReturnIf(!isBitSet(registers[rSTATUS], signF));

			break;

		/* POP PSW */
		case 0xF1:
			cpuWriteWordToRegisterPair(rA, rSTATUS, cpuPopFromStack());

			cycleCounter += 10;

			break;

		/* JP a16 */
		case 0xF2:
			cpuJumpIf(!isBitSet(registers[rSTATUS], 7), 
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			cycleCounter += 10;

			break;

		/* DI */
		case 0xF3:
			/* This instruction doesn't do anything in the cpu tests */

			cycleCounter += 4;

			break;
	
		/* CP a16 */
		case 0xF4:
			cpuCallIf(!isBitSet(registers[rSTATUS], signF),
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			break;

		/* PUSH PSW */
		case 0xF5:
			cpuPushToStack(registers[rA] << 8 | registers[rSTATUS]);

			cycleCounter += 13;

			break;

		/* ORI d8 */
		case 0xF6:
			cpuInstructionORI(readMemory(programCounter++));

			cycleCounter += 7;

			break;

		/* RP */
		case 0xF8:
			cpuReturnIf(isBitSet(registers[rSTATUS], signF));

			break;

		/* SPHL */
		case 0xF9:
			stackPointer = cpuReadRegisterPair(rH, rL);

			break;

		/* JM a16 */
		case 0xFA:
			cpuJumpIf(isBitSet(registers[rSTATUS], 7), 
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			cycleCounter += 10;

			break;

		/* EI */
		case 0xFB:
			/* This instruction doesn't do anything in the cpu tests */

			cycleCounter += 4;

			break;

		/* CM a16 */
		case 0xFC:
			cpuCallIf(isBitSet(registers[rSTATUS], signF),
					readMemory(programCounter + 1) << 8 | readMemory(programCounter));

			break;

		/* CPI d8  */
		case 0xFE:
			cpuInstructionCPI(readMemory(programCounter++));

			cycleCounter += 7;

			break;

		default:
			puts("unrecognized opcode");

			printf("opcode: %X\n", opcode);

			printCpuState();

			exit(1);
	}

#ifdef _VERBOSE_DEBUG
	printCpuState();
#endif /* #ifdef _VERBOSE_DEBUG */
}
