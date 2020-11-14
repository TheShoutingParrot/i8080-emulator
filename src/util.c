#include "util.h"
#include <stdio.h>

bool isBitSet(uint8_t number, uint8_t bit) {
        return number & (1 << bit);
}

void setBit(uint8_t *number, uint8_t bit) {
        *number |= (1 << bit);
}

void clearBit(uint8_t *number, uint8_t bit) {
        *number &= ~(1 << bit);
}

void clearOrSetBit(uint8_t *number, uint8_t bit, bool value) {
        if(value)
                setBit(number, bit);
        else
                clearBit(number, bit);
}


char *byteToBitString(uint8_t byte) {
        static char str[8];
        int i, j;

        for(i = 7, j = 0; i > -1, j < 8; i--, j++) {
                str[i] = isBitSet(byte, j) ? '1' : '0';
        }

        return str;
}

void loadRom(uint8_t *memory, const char *path, const uint16_t start) {
        FILE *romFile;

        size_t i, fileSize;

        romFile = fopen(path, "rb");

        fseek(romFile, 0, SEEK_END);
        fileSize = ftell(romFile);
        rewind(romFile);

        fread(memory + start, fileSize, sizeof(uint8_t), romFile);
}
