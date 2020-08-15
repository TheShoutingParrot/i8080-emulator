#include "util.h"

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
