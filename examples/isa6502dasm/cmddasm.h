// cmddasm.h - implements a 6502 disassembler


#include <stdint.h>


// The context is expected to implements
extern uint8_t mem_read(uint16_t addr);
extern void    mem_write(uint16_t addr, uint8_t data);


// This module implements commands
void cmddasm_register(void);
void cmdasm_register(void);
