// cmdwrite.h - command to write to memory
#ifndef __CMDWRITE_H__
#define __CMDWRITE_H__


// The context is expected to implement
#include <stdint.h>
extern uint8_t mem_read(uint16_t addr);
extern void    mem_write(uint16_t addr, uint8_t data);


// This module implements a command
void cmdwrite_register(void);


#endif
