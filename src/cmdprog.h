// cmdprog.h - a command to edit and compile an assembler program
#ifndef __CMDPROG_H__
#define __CMDPROG_H__


// The context is expected to implement
#include <stdint.h>
extern const uint16_t mem_size;
extern uint8_t mem_read(uint16_t addr);
extern void    mem_write(uint16_t addr, uint8_t data);


// This module implements a command
void cmdprog_register(void);


#endif
