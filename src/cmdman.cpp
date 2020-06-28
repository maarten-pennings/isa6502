// cmdman.cpp - a man page command for the 6502 instruction set architecture


#include <Arduino.h>
#include <stdint.h>
#include "cmd.h"
#include "isa.h"


static int cmdman_findmid(const char*stack,const char*needle); // forward
// Returns 0 if needle is found at pos 0 ("left") of stack, otherwise returns -1.
// Note that needle may contain '?' (matches any char), or '*' matching 0 or more chars.
// Note that stack is in PROGMEM and needle in RAM.
static int cmdman_findleft(/*PROGMEM*/const char*stack,const char*needle) {
  //Serial.print("lft('"); Serial.print(f(stack)); Serial.print("','"); Serial.print(needle); Serial.println("')");   
  while( *needle!=0 ) {
    if( *needle=='*' ) { int p=cmdman_findmid(stack,needle+1); return p>=0?0:-1; }
    char _stack= (char)pgm_read_byte(stack);
    if( *needle=='?' && _stack!='\0' ) { stack++; needle++; continue; }
    if( toupper(*needle)==toupper(_stack) ) { stack++; needle++; continue; }
    return -1;
  }
  return 0;
}


// Returns position of needle in stack, otherwise returns -1.
// Note that needle may contain '?' (matches any char), or '*' matching 0 or more chars.
// Note that stack is in PROGMEM and needle in RAM.
static int cmdman_findmid(/*PROGMEM*/const char*stack,const char*needle) {
  //Serial.print("mid('"); Serial.print(f(stack)); Serial.print("','"); Serial.print(needle); Serial.println("')");   
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
// Prints a list of all instruction types and all addressing modes
static void cmdman_printindex() {
  int n;
  // Instructions
  n= 0;
  for( int iix= ISA_IIX_FIRST; iix<ISA_IIX_LAST; iix++ ) {
    if( n%CMDMAN_NAMESPERLINE==0 ) Serial.print(F("inst:"));
    cmd_printf_P( PSTR(" %S"), isa_instruction_iname(iix) );
    if( (n+1)%CMDMAN_NAMESPERLINE==0 ) Serial.println();
    n++;
  }
  if( n%CMDMAN_NAMESPERLINE!=0 ) Serial.println();
  // Addrmodes
  n= 0;
  for( int aix= ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++ ) {
    if( n%CMDMAN_NAMESPERLINE==0 ) Serial.print(F("addr:"));
    cmd_printf_P( PSTR(" %S"), isa_addrmode_aname(aix) );
    if( (n+1)%CMDMAN_NAMESPERLINE==0 ) Serial.println();
    n++;
  }
  if( n%CMDMAN_NAMESPERLINE!=0 ) Serial.println();
}


// Prints a list of all registers (and the PSR flags)
static void cmdman_printregs() {
  Serial.println( F("A   - accumulator") ); 
  Serial.println( F("X   - index register") ); 
  Serial.println( F("Y   - index register") ); 
  Serial.println( F("S   - stack pointer (low byte, high byte is 01)") ); 
  Serial.println( F("PCL - program counter low byte") ); 
  Serial.println( F("PCH - program counter high byte") ); 
  Serial.println( F("PSR - program status register") ); 
  Serial.println( F("      N/7: negative") ); 
  Serial.println( F("      V/6: overflow") ); 
  Serial.println( F("      -/5:") ); 
  Serial.println( F("      B/4: BRK executed") ); 
  Serial.println( F("      D/3: decimal mode active") ); 
  Serial.println( F("      I/2: IRQ disabled") ); 
  Serial.println( F("      Z/1: zero") ); 
  Serial.println( F("      C/0: carry") ); 
}


// Prints the details of instruction type with index `ìix`
static void cmdman_printinstruction(int iix) {
  cmd_printf_P( PSTR("name: %S (instruction)\r\n"), isa_instruction_iname(iix) );  
  cmd_printf_P( PSTR("desc: %S\r\n"), isa_instruction_desc(iix) ); 
  cmd_printf_P( PSTR("help: %S\r\n"), isa_instruction_help(iix) );  
  cmd_printf_P( PSTR("flag: %S\r\n"), isa_instruction_flags(iix) );  
  cmd_printf_P( PSTR("addr: ") ); 
  for( int aix= ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++ ) {
    uint8_t opcode= isa_instruction_opcodes(iix,aix);
    if( opcode!=ISA_OPCODE_INVALID ) cmd_printf_P( PSTR("%S "), isa_addrmode_aname(aix) );
  }
  Serial.println();
}


// Prints the details of addressing mode with index `aix`
static void cmdman_printaddrmode(int aix) {
  cmd_printf_P( PSTR("name: %S (addressing mode)\r\n"), isa_addrmode_aname(aix) );  
  cmd_printf_P( PSTR("desc: %S\r\n"), isa_addrmode_desc(aix) );  
  cmd_printf_P( PSTR("sntx: %S\r\n"), isa_addrmode_syntax(aix) );  
  cmd_printf_P( PSTR("size: %d bytes\r\n"), isa_addrmode_bytes(aix) );
  int n=0;
  for( int iix= ISA_IIX_FIRST; iix<ISA_IIX_LAST; iix++ ) {
    uint8_t opcode= isa_instruction_opcodes(iix,aix);
    if( opcode!=ISA_OPCODE_INVALID ) { 
      if( n%CMDMAN_NAMESPERLINE==0 ) Serial.print(F("inst: "));
      cmd_printf_P( PSTR("%S "), isa_instruction_iname(iix) );
      if( (n+1)%CMDMAN_NAMESPERLINE==0 ) Serial.println();
      n++;
    }
  }
  if( n%CMDMAN_NAMESPERLINE!=0 ) Serial.println();
}


// Prints the details of opcode (instruction variant) `opcode`
static void cmdman_printopcode(uint8_t opcode) {
  uint8_t iix= isa_opcode_iix(opcode);
  uint8_t aix= isa_opcode_aix(opcode);
  if( iix==ISA_IIX_0Ei || aix==ISA_AIX_0Ea ) { 
    cmd_printf_P( PSTR("name: not in use (opcode %02X)\r\n"), opcode );
    return;
  }
  cmd_printf_P( PSTR("name: %S.%S (opcode %02X)\r\n"), isa_instruction_iname(iix), isa_addrmode_aname(aix), opcode );
  cmd_printf_P( PSTR("sntx: %S%S\r\n"), isa_instruction_iname(iix), isa_addrmode_syntax(aix)+3 ); // +3 to skip "OPC"
  cmd_printf_P( PSTR("desc: %S - %S\r\n"), isa_instruction_desc(iix), isa_addrmode_desc(aix) );
  cmd_printf_P( PSTR("help: %S\r\n"), isa_instruction_help(iix) );  
  cmd_printf_P( PSTR("flag: %S\r\n"), isa_instruction_flags(iix) );  
  cmd_printf_P( PSTR("size: %d bytes\r\n"), isa_addrmode_bytes(aix) );
  cmd_printf_P( PSTR("time: %d ticks"), isa_opcode_cycles(opcode) ); 
  if( isa_opcode_xcycles(opcode)==1 ) { 
    Serial.println(F(" (add 1 if page boundary is crossed)")); 
  } else if( isa_opcode_xcycles(opcode)==2 ) { 
    Serial.println(F(" (add 1 if branch occurs, add 1 extra if branch to other page)")); 
  } else if( isa_opcode_xcycles(opcode)>2 ) { // Not in current table
    Serial.print(F(" (upto ")); Serial.print(isa_opcode_xcycles(opcode)); Serial.println(F(" extra)")); 
  } else {
    Serial.println();
  }
}


// Prints a list of instruction types (with description) that match search term `word`
static void cmdman_printfind(char * word) {
  int n= 0;
  for( int iix= ISA_IIX_FIRST; iix<ISA_IIX_LAST; iix++ ) {
    int p= cmdman_findmid( isa_instruction_desc(iix) , word );
    if( p>=0 ) {
      cmd_printf_P( PSTR("%S - %S\r\n"), isa_instruction_iname(iix), isa_instruction_desc(iix) ); 
      n++;
    }
  }
  if( n==0 ) {
    cmd_printf_P( PSTR("no instructions found for '%s'\r\n"), word ); 
  } else {
    cmd_printf_P( PSTR("found %d instructions\r\n"), n ); 
  }
}


// Prints a 16x16 table of all opcodes
static void cmdman_printtable_opcode_line() {
  Serial.print(F("+--+"));
  for( int x=0; x<16; x++ ) {
    Serial.print(F("---+")); 
  }
  Serial.println();
}
static void cmdman_printtable_opcode() {
  cmdman_printtable_opcode_line();
  Serial.print(F("|  |"));
  for( int x=0; x<16; x++ ) {
    cmd_printf_P( PSTR("0%X |"), x);
  }
  Serial.println();
  for( int y=0; y<16; y++) {
    if( y%4==0 ) cmdman_printtable_opcode_line();
    cmd_printf_P( PSTR("|%X0|"), y);
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


// Prints a table of all instructions types with a columns for all addressing modes
static void cmdman_printtable_inst_line() {
  Serial.print(F("+---+"));
  for( int aix= ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++ ) {
    Serial.print(F("---+")); 
  }
  Serial.println();
}
static void cmdman_printtable_inst_header() {
  cmdman_printtable_inst_line();
  Serial.print(F("|   |"));
  for( int aix= ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++ ) {
    Serial.print(f(isa_addrmode_aname(aix))); Serial.print('|');
  }
  Serial.println();
}
static void cmdman_printtable_inst(const char * pattern) {
  int n= 0;
  for( int iix= ISA_IIX_FIRST; iix<ISA_IIX_LAST; iix++ ) {
    if( cmdman_findmid(isa_instruction_iname(iix),pattern)==-1 ) continue;
    if( n==0 ) cmdman_printtable_inst_header();
    if( n%8==0 ) cmdman_printtable_inst_line();
    Serial.print('|');
    Serial.print(f(isa_instruction_iname(iix))); 
    Serial.print('|');
    for( int aix= ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++ ) {
      uint8_t opcode= isa_instruction_opcodes(iix,aix);
      if( opcode!=ISA_OPCODE_INVALID ) { 
        cmd_printf_P( PSTR(" %02X"), opcode );
      } else Serial.print(F("   "));
      Serial.print('|');
    }
    Serial.println();
    n++;
  }
  if( n==0 ) {
    cmd_printf_P( PSTR("no matching instructions found for '%s'\r\n"), pattern ); 
  } else {
    cmdman_printtable_inst_line();
    cmd_printf_P( PSTR("found %d instructions\r\n"), n ); 
  }
}


// The handler for the "man" command
static void cmdman_main(int argc, char * argv[]) {
  // Note cmd_isprefix needs a PROGMEM string. PSTR stores a string in PROGMEM.
  if( argc==1 ) { 
    cmdman_printindex();
    return;
  }
  if( cmd_isprefix(PSTR("find"),argv[1]) ) { 
    if( argc==2 ) { Serial.println(F("ERROR: need a search word")); return; }
    if( argc==3 ) { cmdman_printfind(argv[2]); return; }
    Serial.println(F("ERROR: only one search word allowed")); 
    return;
  }
  if( cmd_isprefix(PSTR("regs"),argv[1]) ) { 
    if( argc==2 ) { cmdman_printregs(); return; }
    Serial.println(F("ERROR: no arguments allowed")); 
    return;
  }
  if( cmd_isprefix(PSTR("table"),argv[1]) ) { 
    if( argc==3 && cmd_isprefix(PSTR("opcode"),argv[2]) ) { cmdman_printtable_opcode(); return; }
    if( argc==3 ) { cmdman_printtable_inst(argv[2]); return; }
    if( argc==2 ) { cmdman_printtable_inst("*"); return; }
    Serial.println(F("ERROR: too many arguments")); 
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
    Serial.println(F("ERROR: must have <inst>, <addrmode>, <hexnum>; or a subcommand"));
    return;
  }
  if( argc==3 ) { 
    int iix= isa_instruction_find(argv[1]);
    if( iix==0 ) { cmd_printf_P( PSTR("ERROR: instruction '%s' does not exist\r\n"), argv[1]); return; }
    int aix= isa_addrmode_find(argv[2]);
    if( aix==0 ) { cmd_printf_P( PSTR("ERROR: addressing mode '%s' does not exist\r\n"), argv[2]); return; }
    uint8_t opcode= isa_instruction_opcodes(iix,aix ); 
    if( opcode==ISA_OPCODE_INVALID ) { cmd_printf_P( PSTR("ERROR: instruction '%s' does not have addressing mode '%s'\r\n"), argv[1], argv[2]); return;}
    cmdman_printopcode(opcode); 
    return; 
  }
  Serial.println(F("ERROR: unexpected arguments")); 
}


// Note cmd_register needs all strings to be PROGMEM strings. For longhelp we do that manually
const char cmdman_longhelp[] PROGMEM = 
  "SYNTAX: man\r\n"
  "- shows an index of instruction types (eg LDA) and addressing modes (eg ABS)\r\n"
  "SYNTAX: man <inst>\r\n"
  "- shows the details of the instruction type <inst> (eg LDA)\r\n"
  "SYNTAX: man <addrmode>\r\n"
  "- shows the details of the addressing mode <addrmode> (eg ABS)\r\n"
  "SYNTAX: man <hexnum> | ( <inst> <addrmode> )\r\n"
  "- shows the details of the instruction variant with opcode <hexnum>\r\n"
  "- alternatively, the variant is identified with type and addressing mode\r\n"
  "SYNTAX: man find <pattern>\r\n"
  "- lists the instruction types, if <pattern> matches their description\r\n"
  "- <pattern> is a series of letters; the match is case insensitive\r\n"
  "- <pattern> may contain *, this matches zero or more chars\r\n"
  "- <pattern> may contain ?, this matches any char\r\n"
  "SYNTAX: man table opcode\r\n"
  "- prints a 16x16 table of opcodes\r\n"
  "SYNTAX: man table <pattern>\r\n"
  "- prints a table of instructions (that match pattern - default pattern is *)\r\n"
  "SYNTAX: man regs\r\n"
  "- lists details of the registers\r\n"
;


// Note cmd_register needs all strings to be PROGMEM strings. For the short string we do that inline with PSTR.
void cmdman_register(void) {
  cmd_register(cmdman_main, PSTR("man"), PSTR("manual pages for the 6502 instructions"), cmdman_longhelp);
}
