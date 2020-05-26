// cmddasm.cpp - implements a 6502 disassembler


#include <Arduino.h>
#include <stdint.h>
#include "isa.h"
#include "cmd.h"
#include "mem.h"


// DISASSEMBLER ====================================================================


// Next address to disassemble
uint16_t cmddasm_addr;


// Disassembles 'num' instructions from memory, starting at 'addr'.
// Prints all values to Serial.
void cmddasm_dasm( uint16_t addr, uint16_t num ) {
  while( num>0 ) {
    // Get all data for the instruction
    uint8_t opcode= mem[ addr%MEM_SIZE ];
    uint8_t iix= isa_opcode_iix(opcode);
    uint8_t aix= isa_opcode_aix(opcode);
    uint8_t bytes= isa_addrmode_bytes(aix);
    uint8_t op1= mem[ (addr+1)%MEM_SIZE ];
    uint8_t op2= mem[ (addr+2)%MEM_SIZE ];
    // Prepare format
    char opsB[7];
    char opsT[5];
    if(      bytes==1 ) { snprintf_P(opsB,sizeof opsB,PSTR("     "));             *opsT=0;                          }
    else if( bytes==2 ) { snprintf_P(opsB,sizeof opsB,PSTR("%02x   "),op1);       snprintf_P(opsT,sizeof opsT,PSTR("%02x"),op1);         }
    else if( bytes==3 ) { snprintf_P(opsB,sizeof opsB,PSTR("%02x %02x"),op1,op2); snprintf_P(opsT,sizeof opsT,PSTR("%02x%02x"),op2,op1); }
    else { Serial.println(F("ERROR: dasm: this should not happen (bytes mismatch)")); return; }
    // Print bytes
    char buf[20]; // 4+1+2+1+5+1+1
    snprintf_P(buf,sizeof buf,PSTR("%04x %02x %s "),addr,opcode,opsB); Serial.print(buf);
    // Print text
    if( iix>0 ) {
      Serial.print(f(isa_instruction_iname(iix))); 
      isa_unparse(buf,isa_addrmode_syntax(aix),opsT); 
      Serial.print(buf);
      if( aix==ISA_AIX_REL ) {
        uint16_t target= addr+bytes+(int8_t)op1;
        // int otherpage= (addr>>8) != (target>>8);
        snprintf_P(buf,sizeof buf,PSTR(" (%04x)"), target); Serial.print(buf);
      }
    } else {
      Serial.print(F("---")); 
    }
    Serial.println(); 
    num--; addr+= bytes;
  }
  cmddasm_addr= addr;
}

#define CMDDASM_NUM 8 // also in help
// The handler for the "dasm" command
void cmddasm_main( int argc, char * argv[] ) {
  // dasm [ <addr> [ <num> ] ]
  if( argc==1 ) { cmddasm_dasm(cmddasm_addr,CMDDASM_NUM); return; }
  // Parse addr
  uint16_t addr;
  if( argv[1][0]=='-' && argv[1][1]=='\0' ) 
    addr= cmddasm_addr;
  else 
    if( !cmd_parse(argv[1],&addr) ) { Serial.println(F("ERROR: dasm: expected hex <addr>")); return; }
  if( argc==2 ) { cmddasm_dasm(addr,CMDDASM_NUM); return; }
  // Parse num
  uint16_t num;
  if( !cmd_parse(argv[2],&num) ) { Serial.println(F("ERROR: dasm: expected hex <num>")); return; }
  if( argc==3 ) { cmddasm_dasm(addr,num); return; }
  Serial.println(F("ERROR: dasm: too many arguments"));
}

const char cmddasm_longhelp[] PROGMEM = 
  "SYNTAX: dasm [ <addr> [ <num> ] ]\n"
  "- disassembles <num> instructions from memory, starting at location <addr>\n"
  "- when <num> is absent, it defaults to 8\n"
  "- when <addr> is absent or '-', it defaults to \"previous\" address\n"
  "- <addr> and <num> are 0000..FFFF\n"
;
  
 
// Note cmd_register needs all strings to be PROGMEM strings. For the short string we do that inline with PSTR.
void cmddasm_register(void) {
  cmd_register(cmddasm_main, PSTR("dasm"), PSTR("disassemble program in memory"), cmddasm_longhelp);
}


// ASSEMBLER ====================================================================


// Next address to assemble
uint16_t cmdasm_addr= 0x0200; // page 0 is zero-page, page 1 is stack


void cmdasm_stream( int argc, char * argv[] ) {
  char buf[20];
  if( argc==0 ) { // no arguments toggles streaming mode
    if( cmd_get_streamfunc()==0 ) cmd_set_streamfunc(cmdasm_stream); else cmd_set_streamfunc(0);
  }  else if( argc==1 && argv[0][0]=='-'&& argv[0][1]==0 ) {
    // Recall that `cmddasm_addr` was set for first instruction, search for last
    uint16_t addr= cmddasm_addr;
    if( addr==cmdasm_addr ) { Serial.println(F("ERROR: asm: can not undo")); return; }
    while(1) {
      uint8_t opcode= mem[ addr%MEM_SIZE ];
      uint8_t aix= isa_opcode_aix(opcode);
      uint8_t bytes= isa_addrmode_bytes(aix);
      if( addr+bytes == cmdasm_addr ) break;
      addr+= bytes;
    }
    cmdasm_addr= addr;
  } else {
    // Get Mnenemonic 
    int iix= isa_instruction_find(argv[0]);
    if( iix==0 ) { Serial.println(F("ERROR: asm: unknown mnenemonic")); return; }
    // Parse operand syntax to find addressing mode
    if( argc==1 ) buf[0]='\0'; else strncpy(buf, argv[1], sizeof buf );
    int aix= isa_parse(buf);
    if( aix==0 ) { Serial.println(F("ERROR: asm: syntax error in operand")); return; }
    // We now have instruction type and addressing mode, does the combo map to an opcode?
    bool rel_as_abs= isa_instruction_opcodes(iix,ISA_AIX_REL)!=ISA_OPCODE_INVALID && aix==ISA_AIX_ABS;
    if( rel_as_abs ) aix=ISA_AIX_REL; // We accept ABS notation for REL-only instructions
    uint8_t opcode= isa_instruction_opcodes(iix,aix);
    if( opcode==ISA_OPCODE_INVALID ) { Serial.println(F("ERROR: asm: instruction does not have that addressing mode")); return; }
    // Check operand size
    uint16_t op;
    int bytes= isa_addrmode_bytes(aix);
    if( bytes>1 ) {
      if( !cmd_parse(buf,&op) ) { Serial.println(F("ERROR: asm: operand must be <hex>")); return; }
      if( rel_as_abs ) { op= op-(cmdasm_addr+2); if( 0x7f<op && op<0xff80 ) { Serial.println(F("ERROR: asm: REL address exceeds range")); return; } op&=0xFF; }
      if( !rel_as_abs && bytes==2 && op>0xff ) { Serial.println(F("ERROR: asm: operand must be 00..ff")); return; }
    }
    // Check
    if( argc>2 ) { Serial.println(F("ERROR: asm: text after operand")); return; }
    // Now assemble
    if( bytes>=1 ) { mem[cmdasm_addr++%MEM_SIZE]=opcode; }
    if( bytes>=2 ) { mem[cmdasm_addr++%MEM_SIZE]=op&0xFF; }
    if( bytes>=3 ) { mem[cmdasm_addr++%MEM_SIZE]=(op>>8)&0xFF; }
    // Print hints
    #define PAGE(a) (((a)>>8)&0xff)
    if( aix==ISA_AIX_ABS && op<0x100 ) Serial.println(F("INFO: asm: suggest ZPG instead of ABS (try - for undo)")); 
    if( aix==ISA_AIX_ABX && op<0x100 ) Serial.println(F("INFO: asm: suggest ZPX instead of ABX (try - for undo)")); 
    if( aix==ISA_AIX_ABY && op<0x100 ) Serial.println(F("INFO: asm: suggest ZPY instead of ABY (try - for undo)")); 
    if( aix==ISA_AIX_REL && PAGE(cmdasm_addr+(int8_t)op)!=PAGE(cmdasm_addr) ) Serial.println(F("INFO: asm: branch to other page (1 cycle extra) (try - for undo)")); 
  }
  // Set the streaming prompt (will only be shown in streaming mode)
  snprintf_P(buf,sizeof buf, PSTR("w%04x> "),cmdasm_addr); cmd_set_streamprompt(buf);
}


// The handler for the "asm" command
void cmdasm_main( int argc, char * argv[] ) {
  argc--; argv++; // remove 'asm'
  uint16_t addr;
  if( argc==0 ) { 
    addr= cmdasm_addr;
  } else {
    if( cmd_parse(argv[0],&addr) ) { 
      // no valid address, this could be a mnenemonic
      argc--; argv++; // remove '<addr>'
    } else {
      addr= cmdasm_addr;
    }
  }
  extern uint16_t cmdread_addr;
  cmdasm_addr= addr; // set own pointer
  cmddasm_addr= cmdasm_addr; // set dasm pointer (so that a 'dasm' command shows what has just been written)
  cmdread_addr= cmdasm_addr; // set read pointer (so that a 'read' command shows what has just been written)
  cmdasm_stream(argc,argv); // note that argc/argv have been moved
}


const char cmdasm_longhelp[] PROGMEM = 
  "SYNTAX: asm [ <addr> ] [ <inst> ]\n"
  "- assembles instruction <inst>, and write it to memory location <addr>\n"
  "- if <inst> is absent, starts streaming mode, one instruction per line\n"
  "- streaming mode ends with an empty line\n"
  "- if <addr> is absent, continues with previous address\n"
  "NOTE:\n"
  "- <inst> is <mnenemonic> <operand>\n"
  "- <mnenemonic> is one of the 3 letter opcode abbreviations\n"
  "- <operand> syntax determines addressing mode\n"
  "- in streaming mode '-' undoes previous instruction\n"
  "- <addr> is 0000..FFFF\n"
;
  
 
// Note cmd_register needs all strings to be PROGMEM strings. For the short string we do that inline with PSTR.
void cmdasm_register(void) {
  cmd_register(cmdasm_main, PSTR("asm"), PSTR("assemble program to memory"), cmdasm_longhelp);
}
