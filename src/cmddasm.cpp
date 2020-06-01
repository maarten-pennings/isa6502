// cmddasm.cpp - implements a command for 6502 inline disassembler


#include <Arduino.h>
#include "isa.h"
#include "cmd.h"
#include "cmddasm.h"


// Next address to disassemble
uint16_t cmddasm_addr;  // Not extern, set by write/dasm


// Disassembles 'num' instructions from memory, starting at 'addr'.
// Prints all values to Serial.
static void cmddasm_dasm( uint16_t addr, uint16_t num ) {
  while( num>0 ) {
    // Get all data for the instruction
    uint8_t opcode= mem_read(addr);
    uint8_t iix= isa_opcode_iix(opcode);
    uint8_t aix= isa_opcode_aix(opcode);
    uint8_t bytes= isa_addrmode_bytes(aix);
    uint8_t op1= mem_read(addr+1);
    uint8_t op2= mem_read(addr+2);
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
      isa_snprint_op(buf,sizeof buf, aix,opsT);
      Serial.print(' ');
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
static void cmddasm_main( int argc, char * argv[] ) {
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


static const char cmddasm_longhelp[] PROGMEM = 
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


