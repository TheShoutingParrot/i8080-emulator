#include "memory.h" 
#include <SDL2/SDL.h>

uint8_t readMemory(uint16_t address) {
	return memory[address];
}

uint16_t readMemoryWord(uint16_t address) {
	return readMemory(address + 1) << 8 | readMemory(address);
}

void writeMemory(uint16_t address, uint8_t data) {
	memory[address] = data;
}

void writeMemoryWord(uint16_t address, uint16_t data) {
	writeMemory(address, data & 0xFF);
	writeMemory(address + 1, data >> 8);
}

void loadRom(const char *path, const uint16_t start) {
	FILE *romFile;

	size_t i, fileSize;

	romFile = fopen(path, "rb");

	fseek(romFile, 0, SEEK_END);
	fileSize = ftell(romFile);
	rewind(romFile);

	fread(&memory[start], fileSize, sizeof(uint8_t), romFile);
}
