// cmdmem.cpp - implements several commands to access memory (write, read)

#include <Arduino.h>
#include "cmd.h"
#include "cmdmem.h"


// cmdread =============================================================================


// Next addr to read
uint16_t cmdread_addr;


// Reads 'num' bytes from memory, starting at 'addr'.
// Prints all values to Serial (in lines of 'CMD_BYTESPERLINE' bytes).
#define CMD_BYTESPERLINE 16
static void cmdread_read( uint16_t addr, uint16_t num ) {
  char buf[8];
  int n= 0;
  while( num>0 ) {
    if( n%CMD_BYTESPERLINE==0 ) { snprintf_P(buf,sizeof(buf),PSTR("%04x:"),addr); Serial.print(buf); }
    uint8_t data= mem_read(addr);
    snprintf_P(buf,sizeof(buf),PSTR(" %02x"),data); Serial.print(buf);
    n++; num--; addr++; // auto wraps
    if( n%CMD_BYTESPERLINE==0 ) { Serial.println(); }
  }
  cmdread_addr= addr;
}


#define CMDREAD_NUM 0x40 // also in help
// The handler for the "read" command
static void cmdread_main( int argc, char * argv[] ) {
  // read [ <addr> [ <num> ] ]
  if( argc==1 ) { cmdread_read(cmdread_addr,CMDREAD_NUM); return; }
  // Parse addr
  uint16_t addr;
  if( argv[1][0]=='-' && argv[1][1]=='\0' ) 
    addr= cmdread_addr;
  else 
    if( !cmd_parse(argv[1],&addr) ) { Serial.println(F("ERROR: read: expected hex <addr>")); return; }
  if( argc==2 ) { cmdread_read(addr,CMDREAD_NUM); return; }
  // Parse num
  uint16_t num;
  if( !cmd_parse(argv[2],&num) ) { Serial.println(F("ERROR: read: expected hex <num>")); return; }
  if( argc==3 ) { cmdread_read(addr,num); return; }
  Serial.println(F("ERROR: read: too many arguments"));
}


static const char cmdread_longhelp[] PROGMEM = 
  "SYNTAX: read [ <addr> [ <num> ] ]\n"
  "- reads <num> bytes from memory, starting at location <addr>\n"
  "- when <num> is absent, it defaults to 40\n"
  "- when <addr> is absent or -, it defaults to \"previous\" address\n"
  "- <addr> and <num> are 0000..FFFF\n"
;

  
// Note cmd_register needs all strings to be PROGMEM strings. For the short string we do that inline with PSTR.
void cmdread_register(void) {
  cmd_register(cmdread_main, PSTR("read"), PSTR("read from memory"), cmdread_longhelp);
}


// cmdwrite =============================================================================


// Next addr to write to in streaming mode
uint16_t cmdwrite_addr;


static void cmdwrite_stream( int argc, char * argv[] ) {
  if( argc==0 ) { // no arguments toggles streaming mode
    if( cmd_get_streamfunc()==0 ) cmd_set_streamfunc(cmdwrite_stream); else cmd_set_streamfunc(0);
  }
  for( int i=0; i<argc; i++ ) {
    uint16_t data;
    if( !cmd_parse(argv[i],&data) || data>0xFF ) { 
      Serial.print(F("ERROR: write: <data> must be 00..FF, not '")); 
      Serial.print(argv[i]); Serial.print(F(", "));
      if( i<argc-1 ) Serial.print(F("rest ")); 
      Serial.println(F("ignored"));
      break; // Not return, we need to update streaming prompt
    }
    mem_write(cmdwrite_addr, data);
    cmdwrite_addr++;
  }
  // Set the streaming prompt (will only be shown in streaming mode)
  char buf[8]; snprintf_P(buf,sizeof buf, PSTR("w%04x> "),cmdwrite_addr); cmd_set_streamprompt(buf);
}


// The statistics command handler
static void cmdwrite_main(int argc, char * argv[]) {
  if( argc<2 ) {  Serial.println(F("ERROR: write: insufficient arguments (<addr>)")); return; }
  if( !cmd_parse(argv[1],&cmdwrite_addr) ) {  Serial.println(F("ERROR: write: expected hex <addr>")); return; }
  extern uint16_t cmddasm_addr;
  cmddasm_addr= cmdwrite_addr; // set dasm pointer (so that a 'dasm' command shows what has just been written)
  cmdread_addr= cmdwrite_addr; // set read pointer (so that a 'read' command shows what has just been written)
  cmdwrite_stream(argc-2,argv+2); // skip 'write addr'
}


// Note cmd_register needs all strings to be PROGMEM strings. For longhelp we do that manually
static const char cmdwrite_longhelp[] PROGMEM = 
  "SYNTAX: write <addr> <data>...\n"
  "- writes <data> byte to memory location <addr>\n"
  "- multiple <data> bytes allowed (auto increment of <addr>)\n"
  "- if <data> is absent, starts streaming mode (empty line ends it)\n"
  "NOTE:\n"
  "- <data> is 00..FF\n"
  "- <addr> is 0000..FFFF, but physical memory is limited and mirrored\n"
;


// Note cmd_register needs all strings to be PROGMEM strings. For the short string we do that inline with PSTR.
void cmdwrite_register(void) {
  cmd_register(cmdwrite_main, PSTR("write"), PSTR("write to memory (supports streaming)"), cmdwrite_longhelp);
}

