// cmdman.cpp - a man page command for the 6502 instruction set architecture

#include <Arduino.h>
#include <stdint.h>
#include "cmd.h"
#include "isa.h"


int cmdman_findmid(const char*stack,const char*needle);
// Returns 0 if needle is found at pos 0 of stack, otherwise returns -1.
// Note that needle may contain '?' (matches any char), or '*' matching 0 or more chars.
int cmdman_findleft(const char*stack,const char*needle) {
  while( *needle!=0 ) {
    if( *needle=='*' ) { int p=cmdman_findmid(stack,needle+1); return p>=0?0:-1; }
    if( *needle=='?' ) { stack++; needle++; continue; }
    if( toupper(*needle)==toupper((char)pgm_read_byte(stack)) ) { stack++; needle++; continue; }
    return -1;
  }
  return 0;
}

// Returns position of needle in stack, otherwise returns -1.
// Note that needle may contain '?' (matches any char), or '*' matching 0 or more chars.
int cmdman_findmid(const char*stack,const char*needle) {
  if( *needle==0 ) { return 0; }
  int ix=0;
  while( pgm_read_byte(stack)!=0 ) {
    int p= cmdman_findleft(stack,needle);
    if( p>=0 ) { return p+ix; }
    stack++;
    ix++;
  }
  return -1;
}


#define CMDMAN_NAMESPERLINE 16
static void cmdman_printindex() {
  int n;
  // Instructions
  n= 0;
  for( int iix= ISA_IIX_FIRST; iix<ISA_IIX_LAST; iix++ ) {
    if( n%CMDMAN_NAMESPERLINE==0 ) Serial.print(F("man: inst: "));
    Serial.print(f(isa_instruction_iname(iix))); Serial.print(' ');
    if( (n+1)%CMDMAN_NAMESPERLINE==0 ) Serial.println();
    n++;
  }
  if( n%CMDMAN_NAMESPERLINE!=0 ) Serial.println();
  // Addrmodes
  n= 0;
  for( int aix= ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++ ) {
    if( n%CMDMAN_NAMESPERLINE==0 ) Serial.print(F("man: addr: "));
    Serial.print(f(isa_addrmode_aname(aix))); Serial.print(' ');
    if( (n+1)%CMDMAN_NAMESPERLINE==0 ) Serial.println();
    n++;
  }
  if( n%CMDMAN_NAMESPERLINE!=0 ) Serial.println();
}


static void cmdman_printregs() {
  Serial.println( F("man: regs: A   - accumulator") ); 
  Serial.println( F("man: regs: X   - index register") ); 
  Serial.println( F("man: regs: Y   - index register") ); 
  Serial.println( F("man: regs: S   - stack pointer (low byte, high byte is 01)") ); 
  Serial.println( F("man: regs: PCL - program counter low byte") ); 
  Serial.println( F("man: regs: PCH - program counter high byte") ); 
  Serial.println( F("man: regs: PSR - program status register") ); 
  Serial.println( F("man: regs:       N/7: 1=negative") ); 
  Serial.println( F("man: regs:       V/6: 1=overflow") ); 
  Serial.println( F("man: regs:       -/5:") ); 
  Serial.println( F("man: regs:       B/4: 1=BRK executed") ); 
  Serial.println( F("man: regs:       D/3: 1=decimal mode active") ); 
  Serial.println( F("man: regs:       I/2: 1=IRQ disabled") ); 
  Serial.println( F("man: regs:       Z/1: 1=zero") ); 
  Serial.println( F("man: regs:       C/0: 1=carry") ); 
}


static void cmdman_printinstruction(int iix) {
  Serial.print( F("man: name: ") ); Serial.print( f(isa_instruction_iname(iix)) ); Serial.println(F(" (instruction)"));  
  Serial.print( F("man: desc: ") ); Serial.println( f(isa_instruction_desc(iix)) ); 
  Serial.print( F("man: help: ") ); Serial.println( f(isa_instruction_help(iix)) );  
  Serial.print( F("man: flag: ") ); Serial.println( f(isa_instruction_flags(iix)) );  
  Serial.print( F("man: addr: ") ); 
  for( int aix= ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++ ) {
    uint8_t opcode= isa_instruction_opcodes(iix,aix);
    if( opcode!=ISA_OPCODE_INVALID ) { Serial.print(f(isa_addrmode_aname(aix))); Serial.print(' ');}
  }
  Serial.println();
}


static void cmdman_printaddrmode(int aix) {
  Serial.print( F("man: name: ") ); Serial.print( f(isa_addrmode_aname(aix)) ); Serial.println(F(" (addressing mode)"));  
  Serial.print( F("man: desc: ") ); Serial.println( f(isa_addrmode_desc(aix)) );  
  Serial.print( F("man: sntx: ") ); Serial.println( f(isa_addrmode_syntax(aix)) );  
  Serial.print( F("man: size: ") ); Serial.print( isa_addrmode_bytes(aix) ); Serial.println(F(" bytes"));  
  int n=0;
  for( int iix= ISA_IIX_FIRST; iix<ISA_IIX_LAST; iix++ ) {
    uint8_t opcode= isa_instruction_opcodes(iix,aix);
    if( opcode!=ISA_OPCODE_INVALID ) { 
      if( n%CMDMAN_NAMESPERLINE==0 ) Serial.print(F("man: inst: "));
      Serial.print(f(isa_instruction_iname(iix))); Serial.print(' ');
      if( (n+1)%CMDMAN_NAMESPERLINE==0 ) Serial.println();
      n++;
    }
  }
  if( n%CMDMAN_NAMESPERLINE!=0 ) Serial.println();
}


static void cmdman_printopcode(uint8_t opcode) {
  uint8_t iix= isa_opcode_iix(opcode);
  uint8_t aix= isa_opcode_aix(opcode);
  Serial.print( F("man: name: ") ); Serial.print( f(isa_instruction_iname(iix)) ); Serial.print('.'); Serial.print(f(isa_addrmode_aname(aix)) );
  Serial.print( F(" (opcode ") ); if( opcode<16 ) Serial.print('0'); Serial.print(opcode,HEX); Serial.println(')');
  Serial.print( F("man: sntx: ") ); Serial.print( f(isa_instruction_iname(iix)) ); Serial.println( f(isa_addrmode_syntax(aix)+3) );
  Serial.print( F("man: desc: ") ); Serial.print( f(isa_instruction_desc(iix)) ); Serial.print(F(" - ")); Serial.println( f(isa_addrmode_desc(aix)) );
  Serial.print( F("man: help: ") ); Serial.println( f(isa_instruction_help(iix)) );  
  Serial.print( F("man: flag: ") ); Serial.println( f(isa_instruction_flags(iix)) );  
  Serial.print( F("man: size: ") ); Serial.print( isa_addrmode_bytes(aix) ); Serial.println(F(" bytes"));
  Serial.print( F("man: time: ") ); Serial.print(isa_opcode_cycles(opcode)); Serial.print(F(" ticks")); 
  if( isa_opcode_xcycles(opcode)>0 ) { 
    Serial.print(F(" (upto ")); Serial.print(isa_opcode_xcycles(opcode)); Serial.println(F(" extra)")); 
  } else {
    Serial.println();
  }
}


static void cmdman_printfind(char * word) {
  int n= 0;
  for( int iix= ISA_IIX_FIRST; iix<ISA_IIX_LAST; iix++ ) {
    int p= cmdman_findmid( isa_instruction_desc(iix) , word );
    if( p>=0 ) {
      Serial.print( F("man: find: ") ); 
      Serial.print( f(isa_instruction_iname(iix))); 
      Serial.print(F(" - "));
      Serial.print( f(isa_instruction_desc(iix)) ); 
      Serial.println();
      n++;
    }
  }
  /*
  for( int aix= ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++ ) {
    int p= cmdman_findmid( isa_addrmode_desc(aix) , word );
    if( p>=0 ) {
      Serial.print( F("man: find: ") ); 
      Serial.print( f(isa_addrmode_aname(aix))); 
      Serial.print(F(" - "));
      Serial.print( f(isa_addrmode_desc(aix)) ); 
      Serial.println();
      n++;
    }
  }
  */
  if( n==0 ) {
    Serial.println( F("man: find: nothing found") ); 
  } else {
    Serial.print( F("man: find: ") ); Serial.print(n); Serial.println( F(" results") ); 
  }
}


void cmdman_printtable_opcode_line() {
  Serial.print(F("+--+"));
  for( int x=0; x<16; x++ ) {
    Serial.print(F("---")); Serial.print('+');
  }
  Serial.println();
}
void cmdman_printtable_opcode() {
  cmdman_printtable_opcode_line();
  Serial.print(F("|  |"));
  for( int x=0; x<16; x++ ) {
    Serial.print('0'); Serial.print(x,HEX); Serial.print(' '); Serial.print('|');
  }
  Serial.println();
  for( int y=0; y<16; y++) {
    if( y%4==0 ) cmdman_printtable_opcode_line();
    Serial.print('|'); Serial.print(y,HEX); Serial.print('0'); Serial.print('|');
    for( int x=0; x<16; x++ ) {
      int opcode=y*16+x;
      int iix= isa_opcode_iix(opcode);
      if( iix!=0 ) Serial.print(f(isa_instruction_iname(iix))); else Serial.print(F("   ")); 
      Serial.print('|');
    }
    Serial.println();
    Serial.print(F("|  |"));
    for( int x=0; x<16; x++ ) {
      int opcode=y*16+x;
      int aix= isa_opcode_aix(opcode);
      if( aix!=0 ) Serial.print(f(isa_addrmode_aname(aix))); else Serial.print(F("   "));
      Serial.print('|');
    }
    Serial.println();
  }
  cmdman_printtable_opcode_line();
}


void cmdman_printtable_inst_line() {
  Serial.print(F("+---+"));
  for( int aix= ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++ ) {
    Serial.print(F("---")); Serial.print('+');
  }
  Serial.println();
}
void cmdman_printtable_inst() {
  cmdman_printtable_inst_line();
  Serial.print(F("|   |"));
  for( int aix= ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++ ) {
    Serial.print(f(isa_addrmode_aname(aix))); Serial.print('|');
  }
  Serial.println();
  for( int iix= ISA_IIX_FIRST; iix<ISA_IIX_LAST; iix++ ) {
    if( iix%8==1 ) cmdman_printtable_inst_line();
    Serial.print('|');
    Serial.print(f(isa_instruction_iname(iix))); 
    Serial.print('|');
    for( int aix= ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++ ) {
      uint8_t opcode= isa_instruction_opcodes(iix,aix);
      if( opcode!=ISA_OPCODE_INVALID ) { 
        Serial.print(' '); if( opcode<16 ) Serial.print('0'); Serial.print(opcode,HEX); 
      } else Serial.print(F("   "));
      Serial.print('|');
    }
    Serial.println();
  }
  cmdman_printtable_inst_line();
}



// The statistics command handler
static void cmdman_main(int argc, char * argv[]) {
  // Note cmd_isprefix needs a PROGMEM string. PSTR stores the string in PROGMEM.
  if( argc==1 ) { 
    cmdman_printindex();
    return;
  }
  if( cmd_isprefix(PSTR("find"),argv[1]) ) { 
    if( argc==2 ) { Serial.println(F("ERROR: man: find: need a search word")); return; }
    if( argc==3 ) { cmdman_printfind(argv[2]); return; }
    Serial.println(F("ERROR: man: find: only one search word allowed")); 
    return;
  }
  if( cmd_isprefix(PSTR("regs"),argv[1]) ) { 
    if( argc==2 ) { cmdman_printregs(); return; }
    Serial.println(F("ERROR: man: regs: no arguments allowed")); 
    return;
  }
  if( cmd_isprefix(PSTR("table"),argv[1]) ) { 
    if( argc==2 ) { Serial.println(F("ERROR: man: table: need a type")); return; }
    if( argc==3 ) { 
      if( cmd_isprefix(PSTR("opcode"),argv[2]) ) { cmdman_printtable_opcode(); return; }
      if( cmd_isprefix(PSTR("inst"),argv[2]) ) { cmdman_printtable_inst(); return; }
      Serial.println(F("ERROR: man: table: unknown type")); 
      return;
    }
    Serial.println(F("ERROR: man: find: only one type allowed")); 
    return;
  }
  if( argc==2 ) {
    int ix;
    ix= isa_instruction_find(argv[1]);
    if( ix!=0 ) { cmdman_printinstruction(ix); return; }
    ix= isa_addrmode_find(argv[1]);
    if( ix!=0 ) { cmdman_printaddrmode(ix); return; }
    uint16_t opcode;
    bool ok= cmd_parse(argv[1],&opcode);
    if( ok && opcode<=0xff) { cmdman_printopcode(opcode); return; }
    Serial.println(F("ERROR: man: must have <inst>, <addrmode>, or <hexnum> in range 0..ff"));
    return;
  }
  if( argc==3 ) { 
    int iix= isa_instruction_find(argv[1]);
    if( iix==0 ) { Serial.println(F("ERROR: man <inst> <addrmode>: <inst> does not exist")); return; }
    int aix= isa_addrmode_find(argv[2]);
    if( aix==0 ) { Serial.println(F("ERROR: man <inst> <addrmode>: <addrmode> does not exist")); return; }
    uint8_t opcode= isa_instruction_opcodes(iix,aix ); 
    if( opcode!=ISA_OPCODE_INVALID ) { cmdman_printopcode(opcode); return; }
    Serial.println(F("ERROR: man <inst> <addrmode>: <inst> does not have <addrmode>")); 
    return; 
  }
  Serial.println(F("ERROR: man: unexpected arguments")); 
}

// Note cmd_register needs all strings to be PROGMEM strings. For longhelp we do that manually
const char cmdman_longhelp[] PROGMEM = 
  "SYNTAX: man\n"
  "- shows an index of instruction types (eg LDA) and addressing modes (eg ABS)\n"
  "SYNTAX: man <inst>\n"
  "- shows the details of the instruction type <inst> (eg LDA)\n"
  "SYNTAX: man <addrmode>\n"
  "- shows the details of the addressing mode <addrmode> (eg ABS)\n"
  "SYNTAX: man <hexnum> | ( <inst> <addrmode> )\n"
  "- shows the details of the instruction variant with opcode <hexnum>\n"
  "- alternatively, the variant is identified with type and addressing mode\n"
  "SYNTAX: man find <word>\n"
  "- lists the instruction types, or addressing modes with <word> in description\n"
  "- word may contain * (matches zero or more chars)\n"
  "- word may contain ? (matches any char)\n"
  "SYNTAX: man table [ opcode | inst ]\n"
  "- prints a table of opcodes respectively instructions\n"
  "SYNTAX: man regs\n"
  "- lists details of the registers\n"
;

// Note cmd_register needs all strings to be PROGMEM strings. For the short string we do that inline with PSTR.
void cmdman_register(void) {
  cmd_register(cmdman_main, PSTR("man"), PSTR("manual pages for the 6502 instructions"), cmdman_longhelp);
}
