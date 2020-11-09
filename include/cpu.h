#ifndef _CPU_H
#define _CPU_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "memory.h"

enum _registers {
        rB, rC,
        rD, rE,
        rH, rL,
        rA, rSTATUS,    /* together form the PSW (Program Status Word) */
        totalR,         /* not a register just the n of total registers*/
};

enum _flags {
        carryF          = 0,
        parityF         = 2, 
        auxCarryF       = 4,
        zeroF           = 6,
        signF           = 7,
};

enum _Signals {
        noSignal,
        exitSignal,
};

struct cpu8080 {
        /* memory and i/o functions */
        uint8_t         (*readMemory)(struct memory, uint16_t);
        uint16_t        (*readMemoryWord)(struct memory, uint16_t);
        void            (*writeMemory)(struct memory, uint16_t, uint8_t);
        void            (*writeMemoryWord)(struct memory, uint16_t, uint16_t);

        void            (*portOut)(struct cpu8080 *, uint8_t);
        uint8_t         (*portIn)(struct cpu8080 *, uint8_t);

        struct memory   memory;

        uint8_t         registers[totalR];
        uint16_t        programCounter,
                        stackPointer;

        size_t          cycleCounter;

        uint8_t         signalBuffer;
};

void cpuExecuteInstruction(struct cpu8080 *cpu);
void printCpuState(struct cpu8080 cpu);

#endif /* #ifndef _CPU_H */
