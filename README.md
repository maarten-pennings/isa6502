# isa6502
Arduino library with tables describing 6502 instructions

## Introduction

This library contains several table that store the 6502 instruction set.
See for example [this excellent work](https://www.masswerk.at/6502/6502_instruction_set.html).
The following tables are available:

- addressing mode  
  example: ABS (absolute), ZPG (zero page)
- instruction  
  example: LDA (load accumulator), JMP (jump)
- opcode  
  A9 (LDA.IMM)
  
## PROGMEM

In order to save RAM, the tables are stored in flash.
On AVR, that is known as PROGMEM.
Note that the AVRs have a Harvard architecture.
This means that every address occurs twice, once as DATAMEM (RAM), once as PROGMEM (flash).
This means that if a pointer like `char * p` has the value 0x1000, it could address location 0x1000 in RAM 
or 0x1000 in flash. Technically the AVR has two different instructions for that, and the C compiler must generate those.
Unqualified \*s in C are always RAM locations. To read from flash we need a modifier like `pgm_read_byte(p)`.

## Examples

There is a simple [example](isa6502/isa6502.ino) that prints all tables.

(end of doc)
