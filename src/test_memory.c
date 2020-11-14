#ifdef _CPU_TEST

#include "memory.h" 

uint8_t testReadMemory(uint8_t *memory, uint16_t address) {
	return memory[address];
}

uint16_t testReadMemoryWord(uint8_t *memory, uint16_t address) {
	return testReadMemory(memory, address + 1) << 8 | testReadMemory(memory, address);
}

void testWriteMemory(uint8_t *memory, uint16_t address, uint8_t data) {
	memory[address] = data;
}

void testWriteMemoryWord(uint8_t *memory, uint16_t address, uint16_t data) {
	testWriteMemory(memory, address, data & 0xFF);
	testWriteMemory(memory, address + 1, data >> 8);
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

#endif /* #ifdef _CPU_TEST */
