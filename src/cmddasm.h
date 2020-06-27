// cmddasm.h - implements a command for 6502 inline disassembler
#ifndef __CMDDASM_H__
#define __CMDDASM_H__


// The context is expected to implement
#include <stdint.h>
extern uint8_t mem_read(uint16_t addr);
extern void    mem_write(uint16_t addr, uint8_t data);


// Next address to show; the default for the dasm command
extern uint16_t cmddasm_addr; 
// This module implements a command
void cmddasm_register(void);



#endif