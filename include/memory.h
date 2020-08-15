#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint8_t		memory[0xffff];

uint8_t		readMemory(uint16_t address);
uint16_t	readMemoryWord(uint16_t address);
void		writeMemory(uint16_t address, uint8_t data);
void		writeMemoryWord(uint16_t address, uint16_t data);
void		loadRom(const char *path, const uint16_t start);

#endif /* #ifndef _MEMORY_H */
