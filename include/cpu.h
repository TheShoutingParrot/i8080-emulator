#ifndef _CPU_H
#define _CPU_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

enum _registers {
	rB, rC,
	rD, rE,
	rH, rL,
	rA, rSTATUS,	/* together form the PSW (Program Status Word) */
	totalR,		/* not a register just the n of total registers*/
};

enum _flags {
	carryF		= 0,
	parityF		= 2, 
	auxCarryF	= 4,
	zeroF		= 6,
	signF		= 7,
};

uint8_t		registers[totalR];
uint16_t	programCounter,
		stackPointer,
		cycleCounter;

void cpuExecuteInstruction(void);

#endif /* #ifndef _CPU_H */
