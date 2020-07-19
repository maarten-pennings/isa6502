// isa.h - 6502 instruction set architecture
// This file is generated by isa6502.py V7 on 2020-07-19 13:10:50
#ifndef __ISA_H__
#define __ISA_H__


#include "stdint.h"


#define ISA_VERSION "7.1.0"


// WARNING
// The data structure describing the addressing modes, instructions, and opcodes are stored in PROGMEM.
// This means you need special (code memory) instructions to read them.
// The access methods in this module take care of this for a part.
// However, when the returned value is a string (a pointer to characters), the characters are also in PROGMEM.
// Use these pointers with the f() macro, the %S print format (capital S!), or the xxx_P() functions. 
//   Serial.print( f(isa_addrmode_aname(4)) );
//   sprintf(buf, "out[%S]",isa_addrmode_aname(4))
//   if( strcmp_P("ABS",isa_addrmode_aname(4))==0 ) {}


// Recall that F(xxx) puts literal xxx in PROGMEM _and_ makes it printable.
// It uses PSTR() to map it to progmem, and f() to make it printable. Basically F(x) is f(PSTR(x))
// For some reason, Arduino defines PSTR() and F(), but not f().
// The macro f(xxx) assumes xxx is already a pointer to chars in PROGMEM' it only makes it printable.
#ifndef f
#define f(s) ((__FlashStringHelper *)(s))
#endif


 // AddrMode ===========================================================
 // Objects containing an address mode definitions - what does ABS mean
 // The definitions are available through indexes.

// The enumeration of all addressing modes
#define ISA_AIX_0Ea    0 // ??err?? // trap uninitialized variables
#define ISA_AIX_ABS    1 // OPC HHLL
#define ISA_AIX_ABX    2 // OPC HHLL,X
#define ISA_AIX_ABY    3 // OPC HHLL,Y
#define ISA_AIX_ACC    4 // OPC A
#define ISA_AIX_IMM    5 // OPC #NN
#define ISA_AIX_IMP    6 // OPC
#define ISA_AIX_IND    7 // OPC (HHLL)
#define ISA_AIX_REL    8 // OPC +NN
#define ISA_AIX_ZIY    9 // OPC (LL),Y
#define ISA_AIX_ZPG   10 // OPC *LL
#define ISA_AIX_ZPX   11 // OPC *LL,X
#define ISA_AIX_ZPY   12 // OPC *LL,Y
#define ISA_AIX_ZXI   13 // OPC (LL,X)
#define ISA_AIX_FIRST  1 // for iteration: first
#define ISA_AIX_LAST  14 // for iteration: (one after) last

/*PROGMEM*/ const char * isa_addrmode_aname ( int aix );              // A three letter name for the addressing mode (e.g. ABS)
            uint8_t      isa_addrmode_bytes ( int aix );              // Number of bytes this addressing mode takes (opcode plus 0, 1, or 2 bytes for operand)
/*PROGMEM*/ const char * isa_addrmode_desc  ( int aix );              // Human readable description of the addressing mode
/*PROGMEM*/ const char * isa_addrmode_syntax( int aix );              // Notation in assembly language
            int          isa_addrmode_find  (const char * aname);     // Returns aix for `aname` or 0 if not found
            int          isa_snprint_aname  (char * str, int size, int minlen, int aix); // Prints the addressing mode name (eg ABS) associated with `aix` to `str`.
            int          isa_snprint_op     (char * str, int size, int aix, const char * op); // Prints the operand-string (the operand `op` formated according to addressing mode `aix`) to `str`.
            int          isa_parse          (char * ops);             // ops is an operand string. Returns the addressing mode that maps the syntax of ops, and isolates the operand in ops


// Instruction ========================================================
// Objects containing an instruction definitions - what does LDA mean (and which addressing mode variants does it have)
// The definitions are available through indexes.

// The enumeration of all instructions
#define ISA_IIX_0Ei    0  // trap uninitialized variables
#define ISA_IIX_ADC    1 
#define ISA_IIX_AND    2 
#define ISA_IIX_ASL    3 
#define ISA_IIX_BCC    4 
#define ISA_IIX_BCS    5 
#define ISA_IIX_BEQ    6 
#define ISA_IIX_BIT    7 
#define ISA_IIX_BMI    8 
#define ISA_IIX_BNE    9 
#define ISA_IIX_BPL   10 
#define ISA_IIX_BRK   11 
#define ISA_IIX_BVC   12 
#define ISA_IIX_BVS   13 
#define ISA_IIX_CLC   14 
#define ISA_IIX_CLD   15 
#define ISA_IIX_CLI   16 
#define ISA_IIX_CLV   17 
#define ISA_IIX_CMP   18 
#define ISA_IIX_CPX   19 
#define ISA_IIX_CPY   20 
#define ISA_IIX_DEC   21 
#define ISA_IIX_DEX   22 
#define ISA_IIX_DEY   23 
#define ISA_IIX_EOR   24 
#define ISA_IIX_INC   25 
#define ISA_IIX_INX   26 
#define ISA_IIX_INY   27 
#define ISA_IIX_JMP   28 
#define ISA_IIX_JSR   29 
#define ISA_IIX_LDA   30 
#define ISA_IIX_LDX   31 
#define ISA_IIX_LDY   32 
#define ISA_IIX_LSR   33 
#define ISA_IIX_NOP   34 
#define ISA_IIX_ORA   35 
#define ISA_IIX_PHA   36 
#define ISA_IIX_PHP   37 
#define ISA_IIX_PLA   38 
#define ISA_IIX_PLP   39 
#define ISA_IIX_ROL   40 
#define ISA_IIX_ROR   41 
#define ISA_IIX_RTI   42 
#define ISA_IIX_RTS   43 
#define ISA_IIX_SBC   44 
#define ISA_IIX_SEC   45 
#define ISA_IIX_SED   46 
#define ISA_IIX_SEI   47 
#define ISA_IIX_STA   48 
#define ISA_IIX_STX   49 
#define ISA_IIX_STY   50 
#define ISA_IIX_TAX   51 
#define ISA_IIX_TAY   52 
#define ISA_IIX_TSX   53 
#define ISA_IIX_TXA   54 
#define ISA_IIX_TXS   55 
#define ISA_IIX_TYA   56 
#define ISA_IIX_FIRST  1 // for iteration: first
#define ISA_IIX_LAST  57 // for iteration: (one after) last

// Opcode 0xBB is not in use in the 6502. We use it in instruction.opcodes to signal the addrmode does not exist for that instruction
#define ISA_OPCODE_INVALID 0xBB

/*PROGMEM*/ const char * isa_instruction_iname  ( int iix );          // The instruction name (e.g. LDA)
/*PROGMEM*/ const char * isa_instruction_desc   ( int iix );          // The description of the instruction
/*PROGMEM*/ const char * isa_instruction_help   ( int iix );          // The detailed description of the instruction
/*PROGMEM*/ const char * isa_instruction_flags  ( int iix );          // The (program status) flags the instruction updates
            uint8_t      isa_instruction_opcodes( int iix, int aix ); // For each addrmode, the actual opcode
            int          isa_snprint_iname      (char * str, int size, int minlen, int iix); // Prints the instruction name (eg LDA) associated with `iix` to `str`.
            int          isa_instruction_find   (const char * iname); // Returns iix for `iname` or 0 if not found


// Opcode =============================================================
// Objects containing an opcode definitions - how many cycles does LDA.ABS need
// The definitions are available through indexes - the actual 6502 opcodes.
// Opcode that are not in use have the fields 0

uint8_t isa_opcode_iix    ( uint8_t opcode ); // Index into isa_instruction_xxx[]
uint8_t isa_opcode_aix    ( uint8_t opcode ); // Index into isa_addrmode_xxx[]
uint8_t isa_opcode_cycles ( uint8_t opcode ); // The (minimal) number of cycles to execute this instruction variant (0..)
uint8_t isa_opcode_xcycles( uint8_t opcode ); // The worst case additional number of cycles to execute this instruction variant (0..)


#endif
