#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>
#include <stdbool.h>

bool isBitSet(uint8_t number, uint8_t bit);
void setBit(uint8_t *number, uint8_t bit);
void clearBit(uint8_t *number, uint8_t bit);
void clearOrSetBit(uint8_t *number, uint8_t bit, bool value);
char *byteToBitString(uint8_t byte);

#endif /* #ifndef _UTIL_H */
