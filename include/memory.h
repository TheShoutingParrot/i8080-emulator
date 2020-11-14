#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint8_t		testReadMemory(uint8_t *memory, uint16_t address);
uint16_t	testReadMemoryWord(uint8_t *memory, uint16_t address);
void		testWriteMemory(uint8_t *memory, uint16_t address, uint8_t data);
void		testWriteMemoryWord(uint8_t *memory, uint16_t address, uint16_t data);

#endif /* #ifndef _MEMORY_H */
