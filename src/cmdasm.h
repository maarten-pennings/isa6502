// cmdasm.h - implements a command for 6502 inline assembler 
#ifndef __CMDASM_H__
#define __CMDASM_H__


// The context is expected to implement
#include <stdint.h>
extern uint8_t mem_read(uint16_t addr);
extern void    mem_write(uint16_t addr, uint8_t data);


// This module implements a command
void cmdasm_register(void);


#endif