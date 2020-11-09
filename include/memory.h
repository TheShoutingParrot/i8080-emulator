#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

struct memory {
	uint8_t		*memory;
};

uint8_t		testReadMemory(struct memory memory, uint16_t address);
uint16_t	testReadMemoryWord(struct memory memory, uint16_t address);
void		testWriteMemory(struct memory memory, uint16_t address, uint8_t data);
void		testWriteMemoryWord(struct memory memory, uint16_t address, uint16_t data);
void		loadRom(uint8_t *memory, const char *path, const uint16_t start);

#endif /* #ifndef _MEMORY_H */
