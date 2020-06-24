// cmdwrite.cpp - command to write to memory


#include <Arduino.h>
#include "cmd.h"
#include "cmdwrite.h"


// Next addr to write to in streaming mode
static uint16_t cmdwrite_addr;


// This notification called for a write command; first changed address is passed.
// For laziness, the handling is implemented here, not on app level
static void cmdwrite_notify(uint16_t addr) {
  // set read/dasm pointer (so that they can show what has just been written)
  extern uint16_t cmddasm_addr;
  cmddasm_addr= addr; 
  extern uint16_t cmdread_addr;
  cmdread_addr= addr; 
}


static void cmdwrite_stream( int argc, char * argv[] ) {
  if( argc==0 ) { // no arguments toggles streaming mode
    if( cmd_get_streamfunc()==0 ) cmd_set_streamfunc(cmdwrite_stream); else cmd_set_streamfunc(0);
  }
  int i;
  for( i=0; i<argc; i++ ) {
    if( cmd_isprefix(PSTR("seq"),argv[i]) ) { // seq <data> <num>
      if( !(i+1<argc) ) { Serial.print(F("ERROR: seq must have <data>")); goto exit;}
      if( !(i+2<argc) ) { Serial.print(F("ERROR: seq must have <data> and <num>")); i+=1; goto exit;}
      uint16_t data, num;
      if( !cmd_parse(argv[i+1],&data) || data>0xFF ) { Serial.print(F("ERROR: seq <data> must be 00..FF")); i+=2; goto exit; }
      if( !cmd_parse(argv[i+2],&num) ) { Serial.print(F("ERROR: seq <num> must be 0000..FFFF")); i+=2; goto exit; }
      while( num>0 ) {
        mem_write(cmdwrite_addr, data );
        cmdwrite_addr++; num--;
      }
      i+=2;
    } else if( cmd_isprefix(PSTR("read"),argv[i]) ) { // read <addr> <num>
      if( !(i+1<argc) ) { Serial.print(F("ERROR: read must have <addr>")); goto exit;}
      if( !(i+2<argc) ) { Serial.print(F("ERROR: read must have <addr> and <num>")); i+=1; goto exit;}
      uint16_t addr, num;
      if( !cmd_parse(argv[i+1],&addr) ) { Serial.print(F("ERROR: read <addr> must be 0000..FFFF")); i+=2; goto exit; }
      if( !cmd_parse(argv[i+2],&num) ) { Serial.print(F("ERROR: read <num> must be 0000..FFFF")); i+=2; goto exit; }
      if( cmdwrite_addr<addr ) { // copy forward in order to not overwrite self
        while( num>0 ) {
          mem_write(cmdwrite_addr, mem_read(addr) );
          cmdwrite_addr++; addr++; num--;
        }        
      } else { // copy backward in order to not overwrite self
        uint16_t dest=cmdwrite_addr+num; addr+=num;
        while( num>0 ) {
          mem_write(dest, mem_read(addr) );
          cmdwrite_addr++; dest--; addr--; num--;
        }        
      }
      i+=2;
    } else { // <data>
      uint16_t data;
      if( !cmd_parse(argv[i],&data) || data>0xFF ) { Serial.print(F("ERROR: <data> must be 00..FF")); goto exit; }
      mem_write(cmdwrite_addr, data);
      cmdwrite_addr++;
    }
  }

exit: // print ignore message and update prompt
  if( i<argc ) {
    // aborted half way with "goto exit"
    Serial.print(F(", ")); 
    if( i<argc-1 ) Serial.print(F("rest "));  
    Serial.println(F("ignored"));
  }
  // Set the streaming prompt (will only be shown in streaming mode)
  char buf[10]; snprintf_P(buf,sizeof buf, PSTR("W:%04X> "),cmdwrite_addr); cmd_set_streamprompt(buf);
}


// The command handler for the "write" command
static void cmdwrite_main(int argc, char * argv[]) {
  uint16_t addr;
  if( argc<2 ) {  Serial.println(F("ERROR: insufficient arguments (<addr>)")); return; }
  if( !cmd_parse(argv[1],&addr) ) {  Serial.println(F("ERROR: expected hex <addr>")); return; }
  cmdwrite_addr= addr;
  cmdwrite_notify(addr); // Notify first changes address
  cmdwrite_stream(argc-2,argv+2); // skip 'write addr'
}


// Note cmd_register needs all strings to be PROGMEM strings. For longhelp we do that manually
static const char cmdwrite_longhelp[] PROGMEM = 
  "SYNTAX: write <addr> <data>...\n"
  "- writes <data> byte to memory location <addr>\n"
  "- multiple <data> bytes allowed (auto increment of <addr>)\n"
  "- if <data> is absent, starts streaming mode (empty line ends it)\n"
  "- <data> can also be a 'seq' or 'read' macro\n"
  "- use 'seq <data> <num>' to write <num> times <data>\n"
  "- use 'read <addr> <num>' to copy <num> bytes from <addr>\n"
  "NOTE:\n"
  "- <data> is 00..FF\n"
  "- <addr> and <num> is 0000..FFFF, but physical memory is limited and mirrored\n"
;


// Note cmd_register needs all strings to be PROGMEM strings. For the short string we do that inline with PSTR.
void cmdwrite_register(void) {
  cmd_register(cmdwrite_main, PSTR("write"), PSTR("write to memory (supports streaming)"), cmdwrite_longhelp);
}

