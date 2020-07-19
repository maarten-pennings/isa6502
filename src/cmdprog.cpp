// cmdprog.cpp - a command (prog) to edit and compile an assembler program

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include "isa.h"
#include "cmd.h"
#include "cmdprog.h"


// todo: prog file [load name, save name, del name, dir]
// todo: add "prog help" to explain <line> syntax 
// todo: ".DW" with label" does not work (introduce .DV vector)
// todo: warning about segment overlap, with knowledge that we have 64x1k mirrors

// todo: in compile 'on line xx' move to start of line 'ERROR: line xx:
// todo: compiling JMP 0000 suggests to use ZPG (also asm)
// todo: add spaces so that all inst and all pragma's are 4 long

// todo: some statuses report number of objects - shall we add decimal between brackets?
// todo: are line numbers 03X everywhere

// ==========================================================================
// Fixed length string
// ==========================================================================

// A memory manager for strings of (max) length FS_SIZE.
// The memory manager has FS_NUM slots (blocks of FS_SIZE bytes).
#define FS_NUM  20 // Number of fixed-length-strings (power of two is good for the "mod")
#define FS_SIZE  8 // Length of the fixed-length-strings (padded with 0s; terminating 0 is not stored)
typedef char   fs_t[FS_SIZE];
static fs_t    fs_store[FS_NUM]; // slot 0 not used
static uint8_t fs_rover; // Pointer used to search next free slot

// Initialize the memory manager
static void fs_init() {
  for(int fsx=0; fsx<FS_NUM; fsx++) fs_store[fsx][0]='\0'; // mark all as free
  // Note slot 0 will always store the empty string
  fs_rover=0;
}

// This is the "malloc" of the fixed string memory manager. 
// It allocates a slot and stores `s`.
// Returns the slot where `s` is stored, or 0 for error.
// When 0 is returned, there could be multiple reasons for error
//  - no string was passed (s==0)
//  - an empty string is passed (*s=0)
//  - the string is too long
//  - the string store is depleted (no more blocks)
static uint8_t fs_add(char*s) {
  if( s==0 || *s==0 ) return 0; // no empty strings
  if( strlen(s)>FS_SIZE ) return 0; // this store accepts strings with the length equaling the block size (so no terminating 0 is stored), but not more
  int fs_rover_old= fs_rover;
  while( 1 ) {
    if( fs_rover==0 ) {fs_rover++; continue; } // skip slot 0
    if( fs_store[fs_rover][0]=='\0' ) { 
      int i=0;
      while( *s!='\0' && i<FS_SIZE ) fs_store[fs_rover][i++]= *s++; // copy the chars of the string
      while( i<FS_SIZE ) fs_store[fs_rover][i++]= '\0'; // pad slot with zero's
      return fs_rover;
    }
    fs_rover= (fs_rover+1)%FS_NUM;
    if( fs_rover==fs_rover_old ) return 0; // store full
  }
}

// Returns true when the two fixed string indices have the same string value 
static bool fs_eq(uint8_t fsx1, uint8_t fsx2 ) {
  if( fsx1<1 || fsx1>=FS_NUM ) return false; // index out of range
  if( fsx2<1 || fsx2>=FS_NUM ) return false; // index out of range
  char * s1= &fs_store[fsx1][0];
  char * s2= &fs_store[fsx2][0];
  while( *s1!='\0' && *s2!='\0' ) {
    if( *s1++ != *s2++ ) return false;
  }
  return *s1==*s2;
}

// This is the "free" of the memory manager.
// Marks slot `fsx` as empty, making it available for `add` again.
// It is safe to fs_del(0) - it does nothing
static void fs_del(uint8_t fsx) {
  if( fsx<1 || fsx>=FS_NUM ) return; // index out of range
  fs_store[fsx][0]='\0';  // mark slot as empty
}

// This is an alternative "malloc" for the fixed-string memory manager. 
// It allocates a slot and stores an array `bytes` containing `size` bytes.
// The crucial difference with the plain fs_add is that here the byte array may contain a 0 byte.
// Returns the slot where the array is stored, or 0 for error.
// When 0 is returned, there could be multiple reasons for error
//  - no bytes are passed (bytes==0)
//  - an empty bytes array is passed (size<=0)
//  - the array is too long
//  - the string store is depleted (no more blocks)
static uint8_t fs_add_raw(uint8_t * bytes, int size) {
  // Implementation solves the problem that fs_add() does not allow 0s.
  // For fs_add, a zero signals end of the string (and zeros are used to pad the string).
  // So in the fs_add_raw implementation, the MSB of the bytes are set to 1 (so bytes are never 0),
  // and all those MSB bits are collected in the first byte (which also gets its MSB 1).
  uint8_t encbytes[FS_SIZE+1]; // +1 to have space for the terminating 0
  // This sets all MSB in bytes to 1, because fs_store can not store 00 bytes
  // The MSBs are collected in the first byte (which also has its MSB set to 1)
  if( bytes==0 ) return 0; // no array is not allowed
  if( size<=0 ) return 0; // empty array not allowed
  if( size>FS_SIZE-1 ) return 0; // too long array not allowed (we need one byte for the MSBs)
  if( size>8 ) return 0; // extra check in case the macro FS_SIZE is increased above 8; the below code can not store all MSBs then
  uint8_t msbs=0x80; // This is the first byte, collecting all MSBs. It has its own MSB set.
  for( int i=0; i<size; i++ )  {
    if( bytes[i] & 0x80 ) msbs|= 1<<i;
    encbytes[i+1] = bytes[i] | 0x80; // slot zero to collect all MSBs
  }
  encbytes[size+1]='\0'; // terminate
  encbytes[0]=msbs;
  return fs_add((char*)encbytes);
}

// Writes the `fsx`-string (the string from slot `fsx`) to buffer `str`.
// If the `fsx`-string is shorter than `minlen`, spaces will be written (appended)
// until the `fsx`-string plus spaces equals `minlen`. Writes a terminating 0.
// However, writes at most `size` bytes to `str`, so if the `fsx`-string is longer than `size` the
// `fsx`-string will be truncated. The terminating zero will always be added (except when size==0).
// Returns the number of characters that would be written if size were high enough (excluding the terminating 0).
// In other words, when returnvalue<size the complete `fsx`-string (with padding _and_ terminating 0) is written.
static int fs_snprint(char*str, int size, int minlen, uint8_t fsx) {
  if( fsx>=FS_NUM ) return 0; // index out of range (0 is accepted)
  if( fsx!=0 && fs_store[fsx][0]=='\0' ) return 0; // empty slot
  char * s= fs_store[fsx];
  int len=0;
  // Write `fsx`-string
  while( *s!='\0' && len<FS_SIZE ) {
    if( size>0 ) { size--; *str++= (size==0)?'\0':*s;  }
    s++; len++;
  }
  // Write padding
  while( len<minlen ) {
    if( size>0 ) { size--; *str++= (size==0)?'\0':' ';  }
    len++;
  }  
  // Write terminating 0
  if( size>0 ) { size--; *str++='\0'; }
  // len++; // snprintf doesn't count the terminating 0
  // Returns wish length
  return len;
}

// Returns the raw bytes from slot `fsx`, by writing them into `bytes`.
// `bytes` must have size FS_SIZE (actually FS_SIZE-1 is enough).
// Return value is amount of valid bytes in `bytes`.
// Note `bytes` may be 0 in which case still the size is returned.
static uint8_t fs_get_raw(uint8_t fsx, uint8_t * bytes) {
  if( fsx<1 || fsx>=FS_NUM || fs_store[fsx][0]=='\0' ) return 0; // index out of range or free slot
  uint8_t * encbytes= (uint8_t*)fs_store[fsx];
  uint8_t msbs=encbytes[0];
  uint8_t * p = &encbytes[1]; // item 0 is MSBs so start reading at 1
  uint8_t ix = 0; // start writing bytes[] at 0
  while( *p!=0 && ix<FS_SIZE ) {
    uint8_t bit= msbs & 1;
    msbs>>= 1;
    if( bytes ) { if( bit ) bytes[ix]=*p|0x80; else bytes[ix]=*p&~0x80; }
    p++; ix++;
  }
  return ix;
}

// Returns number of free slots.
static int fs_free(void) {
  int free= FS_NUM-1; // slot 0 excluded
  for( int fsx=1; fsx<FS_NUM; fsx++)
    if( fs_store[fsx][0]!='\0' ) free--;
  return free;
}

// For debugging: prints all slots
static void fs_dump(void) {
  cmd_printf_P( PSTR("String store (%X slots free)\r\n"), fs_free() ); 
  for( int fsx=0; fsx<FS_NUM; fsx++) {
    cmd_printf_P( PSTR("%02X."), fsx ); 
    if( fsx==0 ) {
      cmd_printf_P(PSTR("reserved\r\n"));
    } else if( fs_store[fsx][0]=='\0' ) {
      cmd_printf_P(PSTR("free\r\n"));
    } else {
      uint8_t buf[FS_SIZE+1];
      fs_snprint((char*)buf,FS_SIZE+1,0,fsx);
      cmd_printf_P(PSTR("\"%s\""),buf);
      if( buf[0] & 0x80 ) { // Is this raw bytes instead of a string?
        Serial.print('=');
        char sep='(';
        int len= fs_get_raw(fsx,buf);
        for( int i=0; i<len; i++) { cmd_printf_P(PSTR("%c,%02X"),sep,buf[i]); sep=','; }
        Serial.print(')');
      }
      Serial.println();
    }
  }
}

// ==========================================================================
// Lines (storage)
// ==========================================================================

// Stores lines of a program (assembler source)
// These are the tags of the line types
#define LN_TAG_ERR         0
#define LN_TAG_COMMENT     1 // ; This is a silly program
#define LN_TAG_PRAGMA_ORG  2 //          .ORG 0200
#define LN_TAG_PRAGMA_DB   3 // data     .DB 03,01,04,01,05,09
#define LN_TAG_PRAGMA_DW   4 // vects    .DW 1234,5678,9abc
#define LN_TAG_PRAGMA_EB   5 // pi1      .EB 31
#define LN_TAG_PRAGMA_EW   6 // pi2      .EW 3141
#define LN_TAG_INST        7 // loop     LDA #12

// To save RAM, we pack all line types
#define PACKED __attribute__((packed))

// Stores "; This is a silly program"
typedef struct ln_comment_s {
  uint8_t cmt_fsxs[5]; // Split the (long) comment over multiple fixed strings (5 because that is the longest other type: ln_inst_s)
} PACKED ln_comment_t;

// Stores "         .ORG 0200"
typedef struct ln_org_s {
  uint16_t addr;
} PACKED ln_org_t;

// Stores "data     .DB 03,01,04,01,05,09"
typedef struct ln_bytes_s {
  uint8_t lbl_fsx;
  uint8_t bytes_fsx;
} PACKED ln_bytes_t;

// Stores "vects    .DW 1234,5678,9abc"
typedef struct ln_words_s {
  uint8_t lbl_fsx;
  uint8_t words_fsx;
} PACKED ln_words_t;

// Stores "pi1      .EB 31"
typedef struct ln_eqbyte_s {
  uint8_t lbl_fsx;
  uint8_t byte;
} PACKED ln_eqbyte_t;

// Stores "pi2      .EW 3141"
typedef struct ln_eqword_s {
  uint8_t lbl_fsx;
  uint16_t word;
} PACKED ln_eqword_t;

// The most important line type holds instructions. It has some flags.
#define LN_FLAG_OPisLBL       1 // operand is symbol (so label is used instead of number)
#define LN_FLAG_ABSforREL 2 // branch uses ABS notation (so '+' prefix is dropped)
// We could add a flags for dec,hec,oct,bin.
// We could add flags for expressions <, >, +1

// Stores "loop     LDA #12"
typedef struct ln_inst_s {
  uint8_t lbl_fsx;
  uint8_t opcode;
  uint16_t op; // either byte, or word, or fsx
  uint8_t flags;
} PACKED ln_inst_t;

// Stores any line (tag tells which)
typedef struct ln_s {
  uint8_t tag;
  union {
    ln_comment_t  cmt;
    ln_org_t      org;
    ln_bytes_t    bytes;
    ln_words_t    words;
    ln_eqbyte_t   eqbyte;
    ln_eqword_t   eqword;
    ln_inst_t     inst;
  };
} PACKED ln_t ;

// The line store, i.e. the program source code
#define LN_NUM 32
static ln_t ln_store[LN_NUM];
// The number of lines in the line store, so ln_store[0..ln_num) is in use
static uint16_t ln_num;

// Initializes the store.
static void ln_init(void) {
  ln_num=0;
  if( sizeof(ln_t)!=6 ) cmd_printf_P(PSTR("ERROR: packing or padding problem\r\n"));
}


// ==========================================================================
// Lines (parsing)
// ==========================================================================


// Returns 1 if `s` has the chars allowed in a label - same as identifier in other languages
// Returns 0 otherwise (empty string, illegal chars, starts with digit)
static int ln_islabel(char * s) {
  if( s==0 || *s=='\0' )  return 0;
  int foundalpha=0;
  while( *s!=0 ) {
    char c= *s++;
    if( c=='_' ) { foundalpha=1; continue; }
    if( 'a'<=c && c<='z' ) { foundalpha=1; continue; }
    if( 'A'<=c && c<='Z' ) { foundalpha=1; continue; }
    if( '0'<=c && c<='9' && foundalpha ) continue;
    return 0;
  }
  return 1;
}

// Returns 1 if the `s` is a reserved word.
// Reserved words are all mnemonics (eg LDA), all addressing mode names (eg IND), and all register names.
// Also, all hex-lookalike ("dead") are considered reserved.
static int ln_isreserved(char * s) {
  if( s==0 || *s==0 ) return 0;
  if( isa_instruction_find(s) ) return 1;
  if( isa_addrmode_find(s)    ) return 1;
  if( strcasecmp_P(s,PSTR("A"  ))==0  ) return 1;
  if( strcasecmp_P(s,PSTR("X"  ))==0  ) return 1;
  if( strcasecmp_P(s,PSTR("Y"  ))==0  ) return 1;
  if( strcasecmp_P(s,PSTR("S"  ))==0  ) return 1; // stack
  if( strcasecmp_P(s,PSTR("PCL"))==0  ) return 1;
  if( strcasecmp_P(s,PSTR("PCH"))==0  ) return 1;
  if( strcasecmp_P(s,PSTR("PC" ))==0  ) return 1; // for PLH+PCL
  if( strcasecmp_P(s,PSTR("PSR"))==0  ) return 1; // status
  if( strcasecmp_P(s,PSTR("SR" ))==0  ) return 1; // alternative for status
  int ishex=1;
  while( *s!='\0' ) {
    ishex= ishex && ( ('a'<=*s && *s<='f') || ('A'<=*s && *s<='F') || ('0'<=*s && *s<='9') ); 
    s++;
  }
  return ishex;
}

// buffer used for all ln_parse_xxx() parsers
static ln_t ln_temp; 

// Parses `argc`/`argv` for a comment, and returns the parsed comment line (or 0 on parse error)
// Note: a series of spaces in the comment is collapsed to 1 (the cmd parser does that)
static ln_t * ln_parse_comment(int argc, char* argv[]) {
  ln_temp.tag= LN_TAG_COMMENT;

  char buf[FS_SIZE+1];
  unsigned int cmtix=0; // index into ln_temp.cmt.cmt_fsxs[]
  int bufix=0; // index into buf[]
  int argix=0; // index into argv[]
  int charix= 0; // index into argv[argix]
  while( argix<argc ) {
    char ch;
    if( argv[argix][charix]=='\0' ) {
      argix++; charix=0; // next argv
      ch=' ';
      if( argix==argc ) break;
    } else {
      ch= argv[argix][charix++];
    }
    if( cmtix==sizeof(ln_temp.cmt.cmt_fsxs) ) { cmd_printf_P(PSTR("WARNING: comment truncated (line too long)\r\n")); return 0; }
    buf[bufix++]= ch;
    if( bufix==FS_SIZE ) {
      buf[FS_SIZE]= 0;
      ln_temp.cmt.cmt_fsxs[cmtix]= fs_add(buf);
      if( ln_temp.cmt.cmt_fsxs[cmtix]==0 ) { cmd_printf_P(PSTR("WARNING: comment truncated (out of string memory)\r\n")); return 0; } 
      cmtix++;
      bufix= 0;
    }
  }
  if( bufix>0 ) {
    buf[bufix]= 0;
    ln_temp.cmt.cmt_fsxs[cmtix]= fs_add(buf);
    if( ln_temp.cmt.cmt_fsxs[cmtix]==0 ) cmd_printf_P(PSTR("WARNING: comment truncated (out of string memory)\r\n"));
    cmtix++;
  }
  if( cmtix<sizeof(ln_temp.cmt.cmt_fsxs) ) ln_temp.cmt.cmt_fsxs[cmtix]=0;
  return &ln_temp;
}

// Parses the line consisting of three fragments: `label` `pragma` `operand` for a pragma.
// Either `label` or `operand` may be 0.
// Returns the parsed pragma line (or 0 on parse error)
static ln_t * ln_parse_pragma( char * label, char * pragma, char * operand ) {
  if( ln_isreserved(label) ) {
    cmd_printf_P(PSTR("ERROR: label uses reserved word (or hex lookalike)\r\n")); 
    return 0; 
  }
  uint8_t lbl_fsx= fs_add(label); 
  if( label!=0 && *label!='\0' && lbl_fsx==0 ) {
    cmd_printf_P(PSTR("ERROR: label too long or out of string memory\r\n")); 
    return 0; 
  }
  // WARNING: free resource lbl_fsx if we can not add line

  if( strcasecmp_P(pragma,PSTR(".ORG"))==0 ) {
    uint16_t addr;
    if( !cmd_parse(operand,&addr) ) {
      cmd_printf_P(PSTR("ERROR: addr must be 0000..FFFF\r\n"));   
      goto free_lvl_fsx;      
    }
    if( lbl_fsx!=0 ) { fs_del(lbl_fsx); cmd_printf_P(PSTR("WARNING: .ORG does not have label\r\n")); } 
    ln_temp.tag= LN_TAG_PRAGMA_ORG;
    ln_temp.org.addr= addr;
    return &ln_temp;
  } else if( strcasecmp_P(pragma,PSTR(".DB"))==0 ) {
    uint8_t bytes[FS_SIZE-1]; // to collect the bytes
    int bytesix=0;
    while( *operand!='\0' ) {
      // The string is not empty, so get a number
      char buf[3]; // to collect hex chars of the bytes (max "12\0")
      int bufix= 0;
      while( *operand!=',' && *operand!='\0' ) {
        buf[bufix++]= *operand++;
        if( bufix==sizeof(buf) ) {
          cmd_printf_P(PSTR("ERROR: byte %X too long\r\n"),bytesix+1); 
          goto free_lvl_fsx;        
        }
      }
      buf[bufix++]='\0';
      uint16_t word;
      if( !cmd_parse(buf,&word) || word>0xff ) {
        cmd_printf_P(PSTR("ERROR: byte %X must be 00..FF\r\n"),bytesix+1); 
        goto free_lvl_fsx;      
      }
      if( bytesix==sizeof(bytes) ) {
        cmd_printf_P(PSTR("ERROR: too many bytes\r\n")); 
        goto free_lvl_fsx;        
      }
      bytes[bytesix++]= word;
      if( *operand==',' ) operand++;
    }
    if( bytesix==0 ) {
      cmd_printf_P(PSTR("ERROR: bytes missing\r\n")); 
      goto free_lvl_fsx;        
    }
    uint8_t bytes_fsx= fs_add_raw(bytes,bytesix);
    if( bytes_fsx==0 ) {
      cmd_printf_P(PSTR("ERROR: out of string memory (for bytes)\r\n")); 
      goto free_lvl_fsx;      
    }
    ln_temp.tag= LN_TAG_PRAGMA_DB;
    ln_temp.bytes.lbl_fsx= lbl_fsx;
    ln_temp.bytes.bytes_fsx= bytes_fsx;
    return &ln_temp;
  } else if( strcasecmp_P(pragma,PSTR(".DW"))==0 ) {
    uint16_t words[(FS_SIZE-1)/2]; // to collect the words
    int wordsix=0;
    while( *operand!='\0' ) {
      // The string is not empty, so get a number
      char buf[5]; // to collect hex chars of the words (max "1234\0")
      int bufix= 0;
      while( *operand!=',' && *operand!='\0' ) {
        buf[bufix++]= *operand++;
        if( bufix==sizeof(buf) ) {
          cmd_printf_P(PSTR("ERROR: word %X too long\r\n"), wordsix+1); 
          goto free_lvl_fsx;        
        }
      }
      buf[bufix++]='\0';
      uint16_t word;
      if( !cmd_parse(buf,&word) ) {
        cmd_printf_P(PSTR("ERROR: word %X must be 0000..FFFF\r\n"),wordsix+1); 
        goto free_lvl_fsx;      
      }
      if( wordsix==sizeof(words) ) {
        cmd_printf_P(PSTR("ERROR: too many words\r\n")); 
        goto free_lvl_fsx;        
      }
      words[wordsix++]= word;
      if( *operand==',' ) operand++;
    }
    if( wordsix==0 ) {
      cmd_printf_P(PSTR("ERROR: words missing\r\n")); 
      goto free_lvl_fsx;        
    }
    uint8_t words_fsx= fs_add_raw((uint8_t*)words,wordsix*2);
    if( words_fsx==0 ) {
      cmd_printf_P(PSTR("ERROR: out of string memory (for words)\r\n")); 
      goto free_lvl_fsx;      
    }
    ln_temp.tag= LN_TAG_PRAGMA_DW;
    ln_temp.words.lbl_fsx= lbl_fsx;
    ln_temp.words.words_fsx= words_fsx;
    return &ln_temp;
  } else if( strcasecmp_P(pragma,PSTR(".EB"))==0 ) {
    uint16_t byte;
    if( lbl_fsx==0 ) {
      cmd_printf_P(PSTR("ERROR: .EB needs label\r\n")); 
      goto free_lvl_fsx;      
    }
    if( !cmd_parse(operand,&byte) || byte>0xff ) {
      cmd_printf_P(PSTR("ERROR: byte must be 00..FF\r\n")); 
      goto free_lvl_fsx;      
    }
    ln_temp.tag= LN_TAG_PRAGMA_EB;
    ln_temp.eqbyte.lbl_fsx= lbl_fsx;
    ln_temp.eqbyte.byte= byte;
    return &ln_temp;
  } else if( strcasecmp_P(pragma,PSTR(".EW"))==0 ) {
    uint16_t word;
    if( lbl_fsx==0 ) {
      cmd_printf_P(PSTR("ERROR: .EW must have label\r\n")); 
      goto free_lvl_fsx;      
    }
    if( !cmd_parse(operand,&word) ) {
      cmd_printf_P(PSTR("ERROR: word must be 0000..FFFF\r\n")); 
      goto free_lvl_fsx;      
    }
    ln_temp.tag= LN_TAG_PRAGMA_EW;
    ln_temp.eqword.lbl_fsx= lbl_fsx;
    ln_temp.eqword.word= word;
    return &ln_temp;
  } 
  cmd_printf_P(PSTR("ERROR: unknown pragma\r\n")); 
  // goto free_lvl_fsx;
  
free_lvl_fsx:    
  fs_del(lbl_fsx); 
  return 0;
}

// Parses the line consisting of three fragments: `label` `iix` `operand` for an instruction.
// Either `label` or `operand` may be 0. `iix` is the instruction index (isa table)
// Returns the parsed instruction line (or 0 on parse error)
static ln_t * ln_parse_inst( char * label, int iix, char * operand ) {
  char opbuf[16]; // "(<12345678),Y"
  if( operand==0 ) opbuf[0]='\0'; else strncpy(opbuf,operand,sizeof(opbuf));
  
  // Label
  if( ln_isreserved(label) ) {
    cmd_printf_P(PSTR("ERROR: label uses reserved word (or hex lookalike)\r\n")); 
    return 0; 
  }
  uint8_t lbl_fsx= fs_add(label); 
  if( label!=0 && *label!='\0' && lbl_fsx==0 ) {
    cmd_printf_P(PSTR("ERROR: out of string memory for label\r\n")); 
    return 0; 
  }
  // WARNING: free resource lbl_fsx if we can not add line

  // Addressing mode and opcode
  uint8_t flags= 0;
  int aix= isa_parse(opbuf); // This removes "addressing chars",and leaves the actual operand chars in `opbuf`
  if( aix==0 ) {
    cmd_printf_P(PSTR("ERROR: unknown addressing mode syntax\r\n")); 
    goto free_lvl_fsx;      
  }
  if( aix==ISA_AIX_ABS && isa_instruction_opcodes(iix,ISA_AIX_REL)!=ISA_OPCODE_INVALID ) {
    // The syntax is ABS, but the instruction has REL, accept (allows "BEQ loop", next to "BEQ +03")
    flags|= LN_FLAG_ABSforREL;
    aix= ISA_AIX_REL;
  }
  uint8_t opcode;
  opcode= isa_instruction_opcodes(iix,aix);
  if( opcode==ISA_OPCODE_INVALID ) {
    cmd_printf_P(PSTR("ERROR: instruction does not have that addressing mode\r\n")); 
    goto free_lvl_fsx;        
  }
  
  // Operand
  uint16_t op;
  if( opbuf[0]=='\0' ) {
    // No argument
  } else if( cmd_parse(opbuf,&op) ) {
    // `op` is the operand (a number) parsed from `opbuf` 
    // `flags` should _not_ have LN_FLAG_OPisLBL
  } else {
    // not a hex number, so maybe a label
    if( !ln_islabel(opbuf) ) {
      cmd_printf_P(PSTR("ERROR: operand does not have label syntax\r\n")); 
      goto free_lvl_fsx;        
    }
    if( ln_isreserved(opbuf) ) {
      cmd_printf_P(PSTR("ERROR: operand uses reserved word\r\n")); 
      goto free_lvl_fsx;        
    }
    // This is a label, `op` is the label index in the string store. Set the SYM flag
    flags |= LN_FLAG_OPisLBL;
    op= fs_add(opbuf); 
    if( op==0 ) {
      cmd_printf_P(PSTR("ERROR: out of string memory for operand\r\n")); 
      goto free_lvl_fsx;        
    }
  }

  // Create line
  ln_temp.tag= LN_TAG_INST;
  ln_temp.inst.lbl_fsx= lbl_fsx;
  ln_temp.inst.opcode= opcode;
  ln_temp.inst.flags= flags;
  ln_temp.inst.op= op;
  
  return &ln_temp;
  
free_lvl_fsx:    
  fs_del(lbl_fsx);
  return 0;  
}

// Parses an input line into ln_t. Returns 0 on failure.
// Note, uses a static buffer for ln_t, so only one call at a time.
static ln_t * ln_parse(int argc, char* argv[]) {
  // Analyze the argv pattern, guess the line type and call the parse for the line type
  if( argc==0 ) { 
    cmd_printf_P(PSTR("ERROR: empty line\r\n")); 
    return 0; 
  }
  // ; comment
  // LABEL .PRAGMA OPERAND
  //       .PRAGMA OPERAND
  // LABEL .PRAGMA            (does not exist right now)
  //       .PRAGMA            (does not exist right now)
  // LABEL OPC OPERAND  
  //       OPC OPERAND  
  // LABEL OPC
  //       OPC
  if( argv[0][0]==';' ) { 
    // ; comment
    if( argv[0][1]=='\0' ) return ln_parse_comment(argc-1,argv+1);
    cmd_printf_P(PSTR("ERROR: comment must have space after ;\r\n")); 
    return 0;
  }
  // LABEL .PRAGMA OPERAND
  //       .PRAGMA OPERAND
  // LABEL .PRAGMA            (does not exist right now)
  //       .PRAGMA            (does not exist right now)
  // LABEL OPC OPERAND  
  //       OPC OPERAND  
  // LABEL OPC
  //       OPC
  if( argc==1 ) {
    //       .PRAGMA            (does not exist right now)
    //       OPC
    if( argv[0][0]=='.' ) return ln_parse_pragma(0,argv[0],0); 
    uint8_t inst= isa_instruction_find(argv[0]);
    if( inst!=0 ) return ln_parse_inst(0,inst,0); 
    cmd_printf_P(PSTR("ERROR: unknown instruction\r\n")); 
    return 0; 
  }
  // LABEL .PRAGMA OPERAND
  //       .PRAGMA OPERAND
  // LABEL .PRAGMA            (does not exist right now)
  // LABEL OPC OPERAND  
  //       OPC OPERAND  
  // LABEL OPC
  if( argc==2 ) {
    //       .PRAGMA OPERAND
    //       OPC OPERAND  
    // LABEL OPC
    if( argv[0][0]=='.' ) return ln_parse_pragma(0,argv[0],argv[1]);
    int iix= isa_instruction_find(argv[0]);
    if( iix!=0 ) return ln_parse_inst(0,iix,argv[1]);
    iix= isa_instruction_find(argv[1]);
    if( iix!=0 ) return ln_parse_inst(argv[0],iix,0);
    cmd_printf_P(PSTR("ERROR: unknown instruction (with label or operand)\r\n")); 
    return 0;
  }
  // LABEL .PRAGMA OPERAND
  // LABEL OPC OPERAND  
  if( argc==3 ) {
    // LABEL .PRAGMA OPERAND
    // LABEL OPC OPERAND  
    if( argv[1][0]=='.' ) return ln_parse_pragma(argv[0],argv[1],argv[2]);
    int iix= isa_instruction_find(argv[1]);
    if( iix!=0 ) return ln_parse_inst(argv[0],iix,argv[2]);
    cmd_printf_P(PSTR("ERROR: unknown instruction (with label and operand)\r\n")); 
    return 0;
  }
  cmd_printf_P(PSTR("ERROR: expected 'label inst op'\r\n")); 
  return 0;
}

// Frees all "fixed strings" owned by the line `ln`.
void ln_del(ln_t * ln) {
  switch( ln->tag ) {
  case LN_TAG_COMMENT       : 
    for( uint8_t i=0; i<sizeof(ln->cmt.cmt_fsxs); i++ ) fs_del(ln->cmt.cmt_fsxs[i]); 
    return;
  case LN_TAG_PRAGMA_ORG    : 
    // skip
    return;
  case LN_TAG_PRAGMA_DB     : 
    fs_del(ln->bytes.lbl_fsx); 
    fs_del(ln->bytes.bytes_fsx);
    return;
  case LN_TAG_PRAGMA_DW     : 
    fs_del(ln->words.lbl_fsx); 
    fs_del(ln->words.words_fsx);
    return;
  case LN_TAG_PRAGMA_EB     : 
    fs_del(ln->eqbyte.lbl_fsx); 
    return;
  case LN_TAG_PRAGMA_EW     : 
    fs_del(ln->eqword.lbl_fsx); 
    return;
  case LN_TAG_INST          : 
    fs_del(ln->inst.lbl_fsx); 
    if( ln->inst.flags & LN_FLAG_OPisLBL ) fs_del(ln->inst.op); 
    return;
  }
  cmd_printf_P(PSTR("ERROR: delete: internal error (tag %X)\r\n"), ln->tag);
}


// ==========================================================================
// Lines (printing)
// ==========================================================================

// Prints the line `ln` (which must be of type comment) to the buffer `str`, which has size `size`.
// Returns the number of bytes that would have been printed to `str` if the size was big enough.
static int ln_snprint_comment(char*str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_COMMENT );
  int res, len=0;
  if( ln->cmt.cmt_fsxs[0]=='\0' ) { if(size>0) *str='\0'; return len; } // skip printing ";"
  res= snprintf_P(str,size,PSTR("; ")); str+=res; size-=res; len+=res;
  int i=0;
  while( ln->cmt.cmt_fsxs[i]!=0 ) {
    res= fs_snprint(str,size,0,ln->cmt.cmt_fsxs[i]); str+=res; size-=res; len+=res;
    i++;
  }
  return len;
}

// Prints the line `ln` (which must be of type .org) to the buffer `str`, which has size `size`.
// Returns the number of bytes that would have been printed to `str` if the size was big enough.
static int ln_snprint_org(char * str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_PRAGMA_ORG );
  int res, len=0; 
  // Print indent (we misuse string 0 as empty string to get an indent of FS_SIZE)
  res=fs_snprint(str, size, FS_SIZE, 0); str+=res; size-=res; len+=res;
  // Print org pragma with address
  res=snprintf_P(str, size, PSTR(" .ORG %04X"),ln->org.addr); str+=res; size-=res; len+=res;
  return len;
}

// Prints the line `ln` (which must be of type .bytes) to the buffer `str`, which has size `size`.
// Returns the number of bytes that would have been printed to `str` if the size was big enough.
static int ln_snprint_bytes(char * str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_PRAGMA_DB );
  int res, len=0; 
  // Print label (or indent)
  res=fs_snprint(str, size, FS_SIZE, ln->bytes.lbl_fsx); str+=res; size-=res; len+=res;
  // Print pragma
  res=snprintf_P(str, size, PSTR(" .DB")); str+=res; size-=res; len+=res;
  // Print bytes
  uint8_t bytes[FS_SIZE];
  uint8_t num= fs_get_raw(ln->bytes.bytes_fsx,bytes);
  char c=' ';
  for( uint8_t i=0; i<num; i++) { 
    res=snprintf_P(str, size, PSTR("%c%02X"), c, bytes[i]); str+=res; size-=res; len+=res;
    c=','; 
  }
  return len;
}

// Prints the line `ln` (which must be of type .words) to the buffer `str`, which has size `size`.
// Returns the number of bytes that would have been printed to `str` if the size was big enough.
static int ln_snprint_words(char * str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_PRAGMA_DW );
  int res, len=0; 
  // Print label (or indent)
  res=fs_snprint(str, size, FS_SIZE, ln->bytes.lbl_fsx); str+=res; size-=res; len+=res;
  // Print pragma
  res=snprintf_P(str, size, PSTR(" .DW")); str+=res; size-=res; len+=res;
  // Print words
  uint16_t words[FS_SIZE/2];
  uint8_t num= fs_get_raw(ln->words.words_fsx,(uint8_t*)words);
  char c=' ';
  for( uint8_t i=0; i<num/2; i++) { 
    res=snprintf_P(str, size, PSTR("%c%04X"), c, words[i]); str+=res; size-=res; len+=res;
    c=','; 
  }
  return len;
}

// Prints the line `ln` (which must be of type .eqbyte) to the buffer `str`, which has size `size`.
// Returns the number of bytes that would have been printed to `str` if the size was big enough.
static int ln_snprint_eqbyte(char * str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_PRAGMA_EB );
  int res, len=0; 
  // Print label (or indent)
  res=fs_snprint(str, size, FS_SIZE, ln->bytes.lbl_fsx); str+=res; size-=res; len+=res;
  // Print pragma with value
  res=snprintf_P(str, size, PSTR(" .EB %02X"),ln->eqbyte.byte); str+=res; size-=res; len+=res;
  return len;
}

// Prints the line `ln` (which must be of type .eqword) to the buffer `str`, which has size `size`.
// Returns the number of bytes that would have been printed to `str` if the size was big enough.
static int ln_snprint_eqword(char * str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_PRAGMA_EW );
  int res, len=0; 
  // Print label (or indent)
  res=fs_snprint(str, size, FS_SIZE, ln->bytes.lbl_fsx); str+=res; size-=res; len+=res;
  // Print pragma with value
  res=snprintf_P(str, size, PSTR(" .EW %04X"),ln->eqword.word); str+=res; size-=res; len+=res;
  return len;
}

// Prints the line `ln` (which must be of type instruction) to the buffer `str`, which has size `size`.
// Returns the number of bytes that would have been printed to `str` if the size was big enough.
static int ln_snprint_inst(char * str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_INST );
  int res, len=0; 
  // Print label (or indent) plus a space
  res=fs_snprint(str, size, FS_SIZE+1, ln->bytes.lbl_fsx); str+=res; size-=res; len+=res;
  // Print mnemonic
  uint8_t iix= isa_opcode_iix(ln->inst.opcode);
  res=isa_snprint_iname(str, size, 4, iix ); str+=res; size-=res; len+=res;
  // Compose operand
  uint8_t aix= isa_opcode_aix(ln->inst.opcode);
  if( ln->inst.flags & LN_FLAG_ABSforREL ) aix= ISA_AIX_ABS;
  uint8_t bytes= isa_addrmode_bytes(aix);
  char opbuf[FS_SIZE+1]; // a label, or byte or word
  if( ln->inst.flags & LN_FLAG_OPisLBL ) fs_snprint(opbuf,FS_SIZE+1,0,ln->inst.op); // op is an fsx
  else if( bytes==1 ) opbuf[0]='\0';
  else if( bytes==2 ) snprintf_P(opbuf,FS_SIZE+1,PSTR("%02X"),ln->inst.op); // op is a byte
  else if( bytes==3 ) snprintf_P(opbuf,FS_SIZE+1,PSTR("%04X"),ln->inst.op); // op is a word
  // Print operand (opbuf)
  res=isa_snprint_op(str,size,aix,opbuf); str+=res; size-=res; len+=res;
  return len;
}

// Prints the line `ln` (any type) to the buffer `str`, which has size `size`.
// Returns the number of bytes that would have been printed to `str` if the size was big enough.
static int ln_snprint(char * str, int size, ln_t * ln) {
  switch( ln->tag ) {
  case LN_TAG_COMMENT     : return ln_snprint_comment(str,size,ln);
  case LN_TAG_PRAGMA_ORG  : return ln_snprint_org(str,size,ln);
  case LN_TAG_PRAGMA_DB   : return ln_snprint_bytes(str,size,ln);
  case LN_TAG_PRAGMA_DW   : return ln_snprint_words(str,size,ln);
  case LN_TAG_PRAGMA_EB   : return ln_snprint_eqbyte(str,size,ln);
  case LN_TAG_PRAGMA_EW   : return ln_snprint_eqword(str,size,ln);
  case LN_TAG_INST        : return ln_snprint_inst(str,size,ln);
  }
  return snprintf_P(str,size,PSTR("internal error (tag %d)"),ln->tag);
}


// ==========================================================================
// Compiling (data)
// ==========================================================================

// Stores compile info on a line (only the address at the moment)
typedef struct comp_ln_s {
  uint16_t addr;
} comp_ln_t;

// Stores compile info on a fixed string
#define COMP_FLAGS_FSUSE     1 // When fixed string is a "using occurrence" (right hand side label)
#define COMP_FLAGS_FSDEF     2 // When fixed string is a "defining occurrence" (left hand side label)
#define COMP_FLAGS_FSOTHER   4 // When fixed string is a other (comment, bytes, words)
#define COMP_FLAGS_TYPEBYTE  8 // When occurrence is a byte
#define COMP_FLAGS_TYPEWORD 16 // When occurrence is a word
#define COMP_FLAGS_REFD     32 // Only for FSDEFs: When there is a using occurrence (FSUSE)
typedef struct comp_fs_s {
  uint16_t val;  // for an FSDEF the value (the address, or the [eq]byte or [eq]word)
  uint8_t  flags;// from the above COMP_FLAGS_XXX
  uint8_t  defx; // index of the (first) definition
  uint16_t lix;  // Line number owning this string
} comp_fs_t;

// Stores compile info for each org section
#define ORG_NUM 6
typedef struct comp_org_s {
  uint16_t addr1;
  uint16_t addr2;
  uint16_t lix;
} comp_org_t;

// Stores compile info on entire program
typedef struct comp_s {
  comp_ln_t  ln[LN_NUM]; 
  comp_fs_t  fs[FS_NUM];
  comp_org_t org[ORG_NUM];
  uint8_t    org_num;
  bool       add_reset_vector; // if true, poke 0200 at fffc/fffd 
} PACKED comp_t ;

static comp_t comp_result;


// ==========================================================================
// Compiling (result)
// ==========================================================================
// After compiling, the following functions return the compiled code on a line basis

// After compiling: returns the address of a line
static uint16_t comp_get_addr(uint16_t lix) {
  if( lix>=ln_num ) { cmd_printf_P(PSTR("ERROR: internal error (line index %X)\r\n"), lix); return 0; }
  return comp_result.ln[lix].addr;
}

// (*) This is a hack, especially for .bytes or .words. 
// It is mandatory to always first call comp_get_numbytes(), which fills the below two globals.
// Only then comp_get_byte() may be called, which uses these globals
static uint8_t comp_numbytes;
static uint8_t comp_fs_buf[FS_SIZE];

// After compiling, returns the number of bytes generated for line number `lix`
// If 0 is returned, the line does not generate code.
// if >0 is returned, use comp_get_byte(lix,bix) with nix=[0..comp_get_numbytes)
// to get all code bytes.
static uint8_t comp_get_numbytes(uint16_t lix) {
  ln_t * ln= &ln_store[lix]; 
  switch( ln->tag ) {
  case LN_TAG_COMMENT    : comp_numbytes=0; break; // only a comment, no code
  case LN_TAG_PRAGMA_ORG : comp_numbytes=0; break; // only a pragma to define address, no code
  case LN_TAG_PRAGMA_DB  : comp_numbytes=fs_get_raw(ln->bytes.bytes_fsx,comp_fs_buf); break; // HACK (*)
  case LN_TAG_PRAGMA_DW  : comp_numbytes=fs_get_raw(ln->bytes.bytes_fsx,comp_fs_buf); break; // HACK (*)
  case LN_TAG_PRAGMA_EB  : comp_numbytes=0; break; // only defines a label to be a byte
  case LN_TAG_PRAGMA_EW  : comp_numbytes=0; break; // only defines a label to be a word
  case LN_TAG_INST       : comp_numbytes= isa_addrmode_bytes(isa_opcode_aix(ln->inst.opcode)); break;
  default                : cmd_printf_P(PSTR("ERROR: internal error (tag %X)\r\n"),ln->tag); break; 
  }
  return comp_numbytes;
}
  
// After compiling, returns byte `bix` for line number `lix`
static uint8_t comp_get_byte(uint16_t lix, uint8_t bix) {
  if( bix>=comp_numbytes ) { cmd_printf_P(PSTR("ERROR: internal error (byte index %X)\r\n"),bix); return 0; }
  ln_t * ln= &ln_store[lix]; 
  uint8_t b;
  switch( ln->tag ) {
    default                   : b=0; cmd_printf_P(PSTR("ERROR: internal error (tag %X)\r\n"),ln->tag); break; 
    case LN_TAG_PRAGMA_DB     : b= comp_fs_buf[bix]; break; // HACK (*)
    case LN_TAG_PRAGMA_DW     : b= comp_fs_buf[bix]; break; // HACK (*)
    case LN_TAG_INST          : {
      // Does op contain the real opcode or a label
      uint16_t val = ( ln->inst.flags & LN_FLAG_OPisLBL ) ? comp_result.fs[comp_result.fs[ln->inst.op].defx].val : ln->inst.op ;
      if( ln->inst.flags & LN_FLAG_ABSforREL ) val = val-(2+comp_get_addr(lix)); 
      switch( bix ) {
        case 0 : { b= ln->inst.opcode; break; }  
        case 1 : { b= (val>>0) & 0xFF; break; }
        case 2 : { b= (val>>8) & 0xFF; break; }
        default: { cmd_printf_P(PSTR("ERROR: internal error (byte index %X)\r\n"),bix); b= 0; break; }  
      }
      break;
    };
  }
  return b;
}

// ==========================================================================
// Compiling (the translator itself)
// ==========================================================================


// Pass one of the compiler: collect all addresses comp_result.ln[x].addr), and labels (comp_result.fs[x])
static void comp_compile_pass1( int * errors, int * warnings ) {
  (void)errors;
  for( uint16_t lix=0; lix<ln_num; lix++ ) {
    ln_t * ln= &ln_store[lix]; 
    if( ln->tag==LN_TAG_COMMENT ) {
      for( uint8_t i=0; i<sizeof(ln->cmt.cmt_fsxs); i++ ) {
        uint8_t fsx= ln->cmt.cmt_fsxs[i];
        comp_fs_t * cfs= &comp_result.fs[fsx];
        cfs->flags= COMP_FLAGS_FSOTHER;
        cfs->lix= lix;
      }
      continue;
    }
    if( ln->tag==LN_TAG_PRAGMA_EB ) {
      uint8_t lbl= ln->eqbyte.lbl_fsx;
      if( lbl==0 ) {
        cmd_printf_P(PSTR("ERROR: label missing for .EB\r\n")); 
        (*warnings)++;
      } else {
        comp_fs_t * cfs= &comp_result.fs[lbl];
        cfs->flags= COMP_FLAGS_FSDEF | COMP_FLAGS_TYPEBYTE;
        cfs->val= ln->eqbyte.byte;
        cfs->lix= lix;
      }
      continue;
    }
    if( ln->tag==LN_TAG_PRAGMA_EW ) { 
      uint8_t lbl= ln->eqword.lbl_fsx;
      if( lbl==0 ) {
        cmd_printf_P(PSTR("ERROR: label missing for .EW\r\n")); 
        (*warnings)++;
      } else {
        comp_fs_t * cfs= &comp_result.fs[lbl];
        cfs->flags= COMP_FLAGS_FSDEF | COMP_FLAGS_TYPEWORD;
        cfs->val= ln->eqword.word;
        cfs->lix= lix;
      }
      continue;
    }
    if( ln->tag==LN_TAG_PRAGMA_ORG ) { 
      if( comp_result.org_num+1==ORG_NUM )  { 
        cmd_printf_P(PSTR("ERROR: too many .ORGs\r\n")); 
        (*errors)++; 
      } else {
        // start next org section
        comp_result.org_num++;
        comp_result.org[comp_result.org_num].addr1= ln->org.addr; 
        comp_result.org[comp_result.org_num].addr2= comp_result.org[comp_result.org_num].addr1;
        comp_result.org[comp_result.org_num].lix= lix;
      }
      continue;
    } 
    if( comp_result.org_num==0 && comp_result.org[0].addr1==comp_result.org[0].addr2 ) {
      cmd_printf_P(PSTR("WARNING: no .ORG, assuming %04X\r\n"),comp_result.org[comp_result.org_num].addr1);
      (*warnings)++;
    }
    if( ln->tag==LN_TAG_PRAGMA_DB ) { 
      uint8_t lbl= ln->bytes.lbl_fsx;
      if( lbl!=0 ) {
        comp_fs_t * cfs= &comp_result.fs[lbl];
        cfs->flags= COMP_FLAGS_FSDEF | COMP_FLAGS_TYPEWORD; // Word, because the label is the address of the bytes
        cfs->val= comp_result.org[comp_result.org_num].addr2;
        cfs->lix= lix;
      }
      uint8_t fsx= ln->bytes.bytes_fsx;
      if( fsx!=0 ) {
        comp_fs_t * cfs= &comp_result.fs[fsx];
        cfs->flags= COMP_FLAGS_FSOTHER;
        cfs->lix= lix;
      }
      comp_result.ln[lix].addr= comp_result.org[comp_result.org_num].addr2;
      comp_result.org[comp_result.org_num].addr2+= fs_get_raw(ln->bytes.bytes_fsx, 0);
      continue;
    } 
    if( ln->tag==LN_TAG_PRAGMA_DW ) { 
      uint8_t lbl= ln->words.lbl_fsx;
      if( lbl!=0 ) {
        comp_fs_t * cfs= &comp_result.fs[lbl];
        cfs->flags= COMP_FLAGS_FSDEF | COMP_FLAGS_TYPEWORD; // Word, because the label is the address of the words
        cfs->val= comp_result.org[comp_result.org_num].addr2;
        cfs->lix= lix;
      }
      uint8_t fsx= ln->words.words_fsx;
      if( fsx!=0 ) {
        comp_fs_t * cfs= &comp_result.fs[fsx];
        cfs->flags= COMP_FLAGS_FSOTHER;
        cfs->lix= lix;
      }
      comp_result.ln[lix].addr= comp_result.org[comp_result.org_num].addr2;
      comp_result.org[comp_result.org_num].addr2+= fs_get_raw(ln->words.words_fsx, 0);
      continue;
    } 
    if( ln->tag != LN_TAG_INST ) { 
      cmd_printf_P(PSTR("ERROR: internal error (tag)\r\n")); 
      (*errors)++;
      return;
    }
    // Does inst have a label
    uint8_t lbl= ln->inst.lbl_fsx;
    if( lbl==0 ) {
      // skip
    } else {
      comp_fs_t * cfs= &comp_result.fs[lbl];
      cfs->flags= COMP_FLAGS_FSDEF | COMP_FLAGS_TYPEWORD; // Word, because the label is the address of the instruction
      cfs->val= comp_result.org[comp_result.org_num].addr2;
      cfs->lix= lix;
    }
    // Get instruction size
    uint8_t opcode= ln->inst.opcode;
    uint8_t aix= isa_opcode_aix(opcode);
    uint8_t bytes= isa_addrmode_bytes(aix);
    comp_result.ln[lix].addr= comp_result.org[comp_result.org_num].addr2;
    comp_result.org[comp_result.org_num].addr2+= bytes;
    // Administrate label in operand
    if( ln->inst.flags & LN_FLAG_OPisLBL ) {
      uint8_t lbl= ln->inst.op;
      comp_fs_t * cfs= &comp_result.fs[lbl];
      switch(aix) {
      case ISA_AIX_ABS :
      case ISA_AIX_ABX :
      case ISA_AIX_ABY :
      case ISA_AIX_IND :
        cfs->flags= COMP_FLAGS_FSUSE | COMP_FLAGS_TYPEWORD; // Word, because the addressing mode uses an address
        cfs->lix= lix;
        break;         
      case ISA_AIX_IMM :
      case ISA_AIX_REL :
      case ISA_AIX_ZXI :
      case ISA_AIX_ZIY :
      case ISA_AIX_ZPG :
      case ISA_AIX_ZPX :
      case ISA_AIX_ZPY :
        if( ln->inst.flags & LN_FLAG_ABSforREL ) 
          cfs->flags= COMP_FLAGS_FSUSE | COMP_FLAGS_TYPEWORD; // Word, because the instruction is REL but ABS syntax is used
        else 
          cfs->flags= COMP_FLAGS_FSUSE | COMP_FLAGS_TYPEBYTE; // Word, because the addressing mode uses an byte value
        cfs->lix= lix;
        break;         
      case ISA_AIX_ACC :
      case ISA_AIX_IMP :
        // skip
        break;
      case ISA_AIX_0Ea :
      default :
        cmd_printf_P(PSTR("ERROR: internal error (tag)\r\n")); 
        (*errors)++;
        return;
      }      
    }
  }
  comp_result.org_num++; // no fix that it is not current but number of .org segments
}

// Pass two of the compiler: link labels
static void comp_compile_pass2( int * errors, int * warnings ) {
  (void)errors;
  (void)warnings;
  // Cross ref the defs and uses  
  for( int fsx1=1; fsx1<FS_NUM; fsx1++) if( fs_store[fsx1][0]!='\0' ) {
    comp_fs_t * cfs= &comp_result.fs[fsx1];
    cfs->defx= 0;
    if( cfs->flags & COMP_FLAGS_FSOTHER ) continue;
    // for FSDEF and FSUSE, find first def
    for( int fsx2=1; fsx2<FS_NUM; fsx2++) if( fs_store[fsx2][0]!='\0' ) {
      if( fs_eq(fsx1,fsx2) ) {
        if( comp_result.fs[fsx2].flags & COMP_FLAGS_FSDEF ) {
          // Found first def, link to it. If links is from use, flag that
          cfs->defx= fsx2;
          if( cfs->flags & COMP_FLAGS_FSUSE ) comp_result.fs[fsx2].flags |= COMP_FLAGS_REFD;
        }
      }
    }
  }
}

// Pass three of the compiler: check labels
static void comp_compile_pass3( int * errors, int * warnings ) {
  // Check consistency
  char buf[FS_SIZE+1];
  for( int fsx=1; fsx<FS_NUM; fsx++) if( fs_store[fsx][0]!='\0' ) {
    fs_snprint(buf,FS_SIZE+1,0,fsx);
    comp_fs_t * cfs= &comp_result.fs[fsx];
    if( cfs->flags & COMP_FLAGS_FSOTHER ) {
      // skip
    } else if( cfs->flags & COMP_FLAGS_FSUSE ) {
      if( cfs->defx==0 ) { cmd_printf_P(PSTR("ERROR: no definition for \"%s\" on line %X\r\n"), &buf[0], cfs->lix); (*errors)++; }
      comp_fs_t * cfs2= &comp_result.fs[cfs->defx];
      if( cfs->flags & COMP_FLAGS_TYPEBYTE ) {
        if( ! (cfs2->flags & COMP_FLAGS_TYPEBYTE) ) { cmd_printf_P(PSTR("ERROR: \"%s\" on line %X is used as byte but defined as word on line %X\r\n"),&buf[0],cfs->lix,cfs2->lix); (*errors)++; }
      }
      if( cfs->flags & COMP_FLAGS_TYPEWORD ) {
        if( ! (cfs2->flags & COMP_FLAGS_TYPEWORD) ) { cmd_printf_P(PSTR("ERROR: \"%s\" on line %X is used as word but defined as byte on line %X\r\n"),&buf[0],cfs->lix,cfs2->lix); (*errors)++; }
      }
    } else if( cfs->flags & COMP_FLAGS_FSDEF ) {
      comp_fs_t * cfs2= &comp_result.fs[cfs->defx];
      if( cfs->defx!=fsx ) { cmd_printf_P(PSTR("ERROR: double definition for \"%s\" on line %X and %X"),&buf[0],cfs->lix,cfs2->lix);  (*errors)++; }
      if( !( cfs->flags & COMP_FLAGS_REFD ) )  { cmd_printf_P(PSTR("WARNING: no usage of \"%s\" on line %X\r\n"),&buf[0],cfs->lix);  (*warnings)++; }
    }
  }
}

// Pass four of the compiler: check lines
static void comp_compile_pass4( int * errors, int * warnings ) {
  for( uint16_t lix=0; lix<ln_num; lix++ ) {
    ln_t * ln= &ln_store[lix]; 
    if( ln->tag == LN_TAG_INST ) {
      #define PAGE(a) (((a)>>8)&0xff)
      uint8_t opcode= ln->inst.opcode;
      uint8_t aix= isa_opcode_aix(opcode);
      uint16_t op = ( ln->inst.flags & LN_FLAG_OPisLBL ) ? comp_result.fs[comp_result.fs[ln->inst.op].defx].val : ln->inst.op ;
      if( aix==ISA_AIX_REL && ln->inst.flags & LN_FLAG_ABSforREL ) {
        // check if the ABS address is in range of the REL
        uint16_t src_addr = comp_get_addr(lix)+2; 
        uint16_t dst_addr= op;
        if( ((dst_addr>src_addr)&&(dst_addr-src_addr>0x7f)) || ((dst_addr<src_addr)&&(src_addr-dst_addr>0x80)) ) {
          cmd_printf_P(PSTR("ERROR: branch to far on line %X\r\n"),lix); (*errors)++;
        } else {
          if( PAGE(src_addr)!=PAGE(dst_addr) ) { cmd_printf_P(PSTR("WARNING: branch to other page on line %X has one clock tick penalty\r\n"),lix); (*warnings)++; }
        } 
      }
      if( aix==ISA_AIX_REL && !(ln->inst.flags & LN_FLAG_ABSforREL) ) {
        // check if REL is to a different page: warning for extra clock tick
        uint16_t src_addr = comp_get_addr(lix)+2; 
        uint16_t dst_addr = src_addr+(int8_t)op;
        if( PAGE(src_addr)!=PAGE(dst_addr) ) { cmd_printf_P(PSTR("WARNING: branch to other page on line %X has one clock tick penalty\r\n"),lix); (*warnings)++; }
      }      
      if( aix==ISA_AIX_ABS && op<0x100 ) { cmd_printf_P(PSTR("WARNING: suggest ZPG instead of ABS on line %X\r\n"),lix); (*warnings)++; }
      if( aix==ISA_AIX_ABX && op<0x100 ) { cmd_printf_P(PSTR("WARNING: suggest ZPX instead of ABX on line %X\r\n"),lix); (*warnings)++; }
      if( aix==ISA_AIX_ABY && op<0x100 ) { cmd_printf_P(PSTR("WARNING: suggest ZPY instead of ABY on line %X\r\n"),lix); (*warnings)++; }
      // todo: There is a sentence in the original 6502 datasheet, footnote 1 on page 6, eg for LDA 'ADD 1 TO "N" IF PAGE BOUNDARY IS CROSSED' do not yet understand
    }
  }
}

// Pass five of the compiler: check (.org) sections
static void comp_compile_pass5( int * errors, int * warnings ) {
  bool found_fffc= false;
  bool found_fffd= false;
  for( uint8_t oix=0; oix<comp_result.org_num; oix++ ) {
    if( comp_result.org[oix].addr1==comp_result.org[oix].addr2 ) {
      // .org section is empty
      if( oix==0 ) continue;
      cmd_printf_P(PSTR("WARNING: .ORG section on line %X empty\r\n"),comp_result.org[oix].lix); (*warnings)++;
    } else {
      // .org section oix is not empty, does it overlap with any other
      for( uint8_t oix2=oix+1; oix2<comp_result.org_num; oix2++ ) if( comp_result.org[oix2].addr1!=comp_result.org[oix2].addr2 ) {
        // .org section oix2 is also not empty, does oix overlap with it
        bool a1= comp_result.org[oix2].addr1<=comp_result.org[oix].addr1 && comp_result.org[oix].addr1<comp_result.org[oix2].addr2;
        bool a2= comp_result.org[oix2].addr1<=comp_result.org[oix].addr2-1 && comp_result.org[oix].addr2-1<comp_result.org[oix2].addr2;
        if( a1 || a2 ) {
          cmd_printf_P(PSTR("WARNING: .ORG section on line %X overlaps with the one on line %X\r\n"),comp_result.org[oix].lix,comp_result.org[oix2].lix); (*warnings)++;
        }
      }
      // does it contain the reset vector?
      found_fffc |= comp_result.org[oix].addr1<=0xfffc && 0xfffc<comp_result.org[oix].addr2;
      found_fffd |= comp_result.org[oix].addr1<=0xfffd && 0xfffd<comp_result.org[oix].addr2;
    }
  }
  if( ! found_fffc && ! found_fffd ) { cmd_printf_P(PSTR("WARNING: reset vector missing (FFFC and/or FFFD), assuming 0200\r\n")); (*warnings)++; }
  else if( ! found_fffc || ! found_fffd ) { cmd_printf_P(PSTR("ERROR: reset vector corrupt\r\n")); (*errors)++; }
  comp_result.add_reset_vector= ! found_fffc && ! found_fffd;
}

static bool comp_compile( void ) {
  int errors=0;
  int warnings=0;
  // Create first (implicit) org section
  comp_result.org_num=0; // during pass 1, num is the current org index (so it runs 1 behind)
  comp_result.org[comp_result.org_num].addr1= 0x200; // default org
  comp_result.org[comp_result.org_num].addr2= comp_result.org[comp_result.org_num].addr1;
  comp_result.org[comp_result.org_num].lix= 0xffff; // not used, but should not trigger print in list
  // Start the passes
  comp_compile_pass1(&errors,&warnings);
  comp_compile_pass2(&errors,&warnings);
  comp_compile_pass3(&errors,&warnings);
  comp_compile_pass4(&errors,&warnings);
  comp_compile_pass5(&errors,&warnings);
  cmd_printf_P(PSTR("INFO: errors %X, warnings %X\r\n"),errors, warnings); 
  return errors==0;
}

static void comp_map( void ) {
  int count=0;
  Serial.println();
  cmd_printf_P(PSTR("labels: lbl id, line num, lbl, Refd//Word/Byte//Other/Def/Use, def lbl id, val\r\n")); 
  for( int fsx=1; fsx<FS_NUM; fsx++) {
    if( fs_store[fsx][0]=='\0' ) continue;
    comp_fs_t * cfs= &comp_result.fs[fsx];
    if( cfs->flags & COMP_FLAGS_FSOTHER ) continue;
    uint8_t buf[FS_SIZE+1];
    fs_snprint((char*)buf,FS_SIZE+1,0,fsx);
    cmd_printf_P(PSTR(" %02X. (ln %03X) \"%s\""),fsx,cfs->lix,(char*)buf);
    if( buf[0] & 0x80 ) { // Is this raw bytes instead of a string?
      Serial.print('=');
      char sep='(';
      int len= fs_get_raw(fsx,buf);
      for( int i=0; i<len; i++) { cmd_printf_P(PSTR("%c,%02X"),sep,buf[i]); sep=','; }
      Serial.print(')');
    }
    Serial.print(' ');
    if( cfs->flags & COMP_FLAGS_REFD    ) Serial.print('R'); else Serial.print('r');
    if( cfs->flags & COMP_FLAGS_TYPEWORD) Serial.print('W'); else Serial.print('w');
    if( cfs->flags & COMP_FLAGS_TYPEBYTE) Serial.print('B'); else Serial.print('b');
    if( cfs->flags & COMP_FLAGS_FSOTHER ) Serial.print('O'); else Serial.print('o');
    if( cfs->flags & COMP_FLAGS_FSDEF   ) Serial.print('D'); else Serial.print('d');
    if( cfs->flags & COMP_FLAGS_FSUSE   ) Serial.print('U'); else Serial.print('u');
    cmd_printf_P(PSTR(" (def %X)"),cfs->defx,HEX);
    if( cfs->flags & COMP_FLAGS_FSDEF   ) { cmd_printf_P(PSTR(" val %X"),cfs->val); }
    Serial.println();
    count++;
  }
  if( count==0 ) Serial.println(f(" none"));
  Serial.println();
  cmd_printf_P(PSTR("sections: section id, line num, start addr, end addr\r\n")); 
  for( uint8_t oix=0; oix<comp_result.org_num; oix++ ) {
    if( oix==0 && comp_result.org[oix].addr1==comp_result.org[oix].addr2 ) continue;
    cmd_printf_P(PSTR(" %02X. "),oix);
    if( oix==0 ) {
      Serial.print(F("(impl) ")); 
    } else {
      cmd_printf_P(PSTR("(ln %03X) "),comp_result.org[oix].lix);
    }
    cmd_printf_P(PSTR(" %04X-%04X "),comp_result.org[oix].addr1,comp_result.org[oix].addr2);
  }
}


static void comp_list( void ) {
  uint8_t oix= 0;
  char buf[40]; 
  Serial.println();
  for(uint16_t lix=0; lix<ln_num; lix++) {
    if( oix+1<comp_result.org_num && comp_result.org[oix+1].lix==lix ) { // A new .ORG section
      if( comp_result.org[oix].addr1!=comp_result.org[oix].addr2 ) cmd_printf_P(PSTR("%04X |             | section %X end\r\n"),comp_result.org[oix].addr2, oix); 
      oix++;
    }
    ln_t * ln= &ln_store[lix]; 
    // Print address and code bytes
    uint16_t addr= comp_get_addr(lix);
    uint8_t bix=0; // num bytes already printed
    uint8_t len= comp_get_numbytes(lix); // bytes to print
    if( len==0 ) {    
      Serial.print(F("     |             ")); 
    } else { 
      cmd_printf_P(PSTR("%04X | "),addr); 
      while( bix<len && bix<4 ) {
        cmd_printf_P(PSTR("%02X "),comp_get_byte(lix,bix)); 
        bix++;
      }
      for(int i=len; i<4; i++ ) Serial.print(F("   "));
    }
    // Print line
    ln_snprint(buf,40,ln);
    cmd_printf_P(PSTR("| %03X %s\r\n"),lix,buf); // END-OF_LINE
    // Special case: too many bytes to print, so we print a next line
    if( bix<len ) { 
      cmd_printf_P(PSTR("%04X | "),addr+bix); 
      while( bix<len ) {
        cmd_printf_P(PSTR("%02X "),comp_get_byte(lix,bix)); 
        bix++;
      }
      for(int i=len; i<8; i++ ) Serial.print(F("   "));
      cmd_printf_P(PSTR("| more bytes\r\n")); 
    } 
  }
  // print final .ORG section end
  cmd_printf_P(PSTR("%04X |             | section %X end\r\n"),comp_result.org[oix].addr2, oix);
  // Vector?
  if( comp_result.add_reset_vector ) {
    cmd_printf_P(PSTR("FFFC | 00 02       | implicit section with reset vector\r\n")); 
    cmd_printf_P(PSTR("FFFD |             | section end\r\n")); 
  }
}

static void comp_bin( void ) {
  uint8_t oix= 0;
  int count= 0;
  Serial.println();
  for(uint16_t lix=0; lix<ln_num; lix++) {
    if( oix+1<comp_result.org_num && comp_result.org[oix+1].lix==lix ) { // A new .ORG section
    if( comp_result.org[oix].addr1!=comp_result.org[oix].addr2 ) { Serial.println(); count=0;  }
      oix++; 
    }
    uint8_t len= comp_get_numbytes(lix); 
    if( len==0 ) continue;
    uint16_t addr= comp_get_addr(lix);
    for( uint8_t bix=0; bix<len; bix++ ) {
      if( count==0 ) cmd_printf_P(PSTR("%04X:"),addr); 
      cmd_printf_P(PSTR(" %02X"),comp_get_byte(lix,bix)); 
      count= (count+1) % 16;
      if( count==0 ) Serial.println();
    }
  }
  if( count>0 ) Serial.println();
  // Vector?
  if( comp_result.add_reset_vector ) {
    cmd_printf_P(PSTR("FFFC: 00 02\r\n")); 
  }
}

static void comp_install( void ) {
  int count=0;
  for(uint16_t lix=0; lix<ln_num; lix++) {
    uint16_t addr= comp_get_addr(lix);
    uint8_t len= comp_get_numbytes(lix);
    for(uint8_t bix=0; bix<len; bix++ ) {
      mem_write(addr+bix,comp_get_byte(lix,bix));
      count++;
    }
  }
  if( comp_result.add_reset_vector ) {
    mem_write(0xFFFC,0x00);
    mem_write(0xFFFD,0x02);
    count+=2;
  }
  cmd_printf_P(PSTR("INFO: installed a program of %X bytes\r\n"),count);  
}


// ==========================================================================
// Command handling
// ==========================================================================


static void cmdprog_new(int argc, char * argv[]) {
  if( argc>3 ) { cmd_printf_P(PSTR("ERROR: too many arguments\r\n")); return; }
  if( argc==3 ) {
    if( !cmd_isprefix(PSTR("example"),argv[2]) ) { cmd_printf_P(PSTR("ERROR: expected 'example'\r\n")); return; }
  }
  // Delete all lines
  for( uint16_t i=0; i<ln_num; i++ ) ln_del(&ln_store[i]); 
  ln_num= 0;
  // Insert example
  if( argc==3 ) {
    cmd_addstr_P(PSTR("prog insert\r"));
    cmd_addstr_P(PSTR("; hello all of you\r"));
    cmd_addstr_P(PSTR("         .ORG 0200\r"));
    cmd_addstr_P(PSTR("count    .EB 05\r"));
    cmd_addstr_P(PSTR("         LDX #count\r"));
    cmd_addstr_P(PSTR("loop     LDA data,x\r"));
    cmd_addstr_P(PSTR("         STA 8000\r"));
    cmd_addstr_P(PSTR("         DEX\r"));
    cmd_addstr_P(PSTR("         BNE loop\r"));
    cmd_addstr_P(PSTR("stop     JMP stop\r"));
    cmd_addstr_P(PSTR("         .ORG 0300\r"));
    cmd_addstr_P(PSTR("data     .DB 48,65,6C,6C,6F\r"));
    cmd_addstr_P(PSTR("         .ORG FFFC\r"));
    cmd_addstr_P(PSTR("         .DW 0200\r"));
    cmd_addstr_P(PSTR("\r"));
  } 
}

static void cmdprog_list(int argc, char * argv[]) {
  uint16_t num1, num2;
  if( argc==2 ) {
    num1=0; num2=ln_num-1; 
  } else if( argc==3 ) { 
    if( !cmd_parse(argv[2],&num1) ) { cmd_printf_P(PSTR("ERROR: expected hex <num1>\r\n")); return; }
    if( num1>=ln_num ) { cmd_printf_P(PSTR("ERROR: <num1> too high\r\n")); return; }
    num2= num1;
  } else if( argc==4 ) {
    if( argv[2][0]=='-' && argv[2][1]=='\0' ) { num1=0; }
    else if( !cmd_parse(argv[2],&num1) ) { cmd_printf_P(PSTR("ERROR: expected hex <num1>\r\n")); return; }
    if( num1>=ln_num ) { cmd_printf_P(PSTR("ERROR: <num1> too high\r\n")); return; }
    if( argv[3][0]=='-' && argv[3][1]=='\0' ) { num2=ln_num-1; }    
    else if( !cmd_parse(argv[3],&num2) ) { cmd_printf_P(PSTR("ERROR: expected hex <num2>\r\n")); return; }
    if( num2>=ln_num ) num2=ln_num-1;
    if( num2<num1 ) { cmd_printf_P(PSTR("ERROR: <num2> less than <num1>\r\n")); return; }
  } else {
    cmd_printf_P(PSTR("ERROR: too many arguments\r\n")); return;
  }
  char buf[40];
  if( ln_num>0 ) for(uint16_t i=num1; i<=num2; i++) {
    ln_snprint(buf,40,&ln_store[i]);
    cmd_printf_P(PSTR("%03X %s\r\n"),i,buf);
  }
}

static void cmdprog_delete(int argc, char * argv[]) {
  uint16_t num1, num2;
  if( argc==2 ) {
    cmd_printf_P(PSTR("ERROR: expected <num1> and <num2>\r\n")); return;
  } else if( argc==3 ) { 
    if( !cmd_parse(argv[2],&num1) ) { cmd_printf_P(PSTR("ERROR: expected hex <num1>\r\n")); return; }
    if( num1>=ln_num ) { cmd_printf_P(PSTR("ERROR: <num1> too high\r\n")); return; }
    num2= num1;
  } else if( argc==4 ) {
    if( argv[2][0]=='-' && argv[2][1]=='\0' ) { num1=0; }
    else if( !cmd_parse(argv[2],&num1) ) { cmd_printf_P(PSTR("ERROR: expected hex <num1>\r\n")); return; }
    if( num1>=ln_num ) { cmd_printf_P(PSTR("ERROR: <num1> too high\r\n")); return; }
    if( argv[3][0]=='-' && argv[3][1]=='\0' ) { num2=ln_num-1; }    
    else if( !cmd_parse(argv[3],&num2) ) { cmd_printf_P(PSTR("ERROR: expected hex <num2>\r\n")); return; }
    if( num2>=ln_num ) { cmd_printf_P(PSTR("ERROR: <num2> too high\r\n")); return; }
    if( num2<num1 ) { cmd_printf_P(PSTR("ERROR: <num2> less than <num1>\r\n")); return; }
  } else {
    cmd_printf_P(PSTR("ERROR: too many arguments\r\n")); return;
  }
  // Delete `len` lines: num1..num2
  uint16_t len= num2-num1+1;
  for( uint16_t i=num1; i<=num2; i++ ) ln_del(&ln_store[i]); 
  // Move all remaining lines up
  num2++;
  while( num2<ln_num ) ln_store[num1++]= ln_store[num2++];
  // Report
  ln_num-= len;
  cmd_printf_P(PSTR("deleted %X lines\r\n"),len); 
}

static void cmdprog_move(int argc, char * argv[]) {
  if( argc<5 ) { cmd_printf_P(PSTR("ERROR: expected 3 line numbers\r\n")); return; }
  uint16_t num1;
  if( !cmd_parse(argv[2],&num1) ) { cmd_printf_P(PSTR("ERROR: expected hex <num1>\r\n")); return; }    
  uint16_t num2;
  if( !cmd_parse(argv[3],&num2) ) { cmd_printf_P(PSTR("ERROR: expected hex <num2>\r\n")); return; }    
  uint16_t num3;
  if( !cmd_parse(argv[4],&num3) ) { cmd_printf_P(PSTR("ERROR: expected hex <num3>\r\n")); return; }    
  if( num1>=ln_num ) { cmd_printf_P(PSTR("ERROR: <num1> does not exist\r\n")); return; }
  if( num2>=ln_num ) { cmd_printf_P(PSTR("ERROR: <num2> does not exist\r\n")); return; }
  if( num1>num2 ) { cmd_printf_P(PSTR("ERROR: <num2> must be at least <num1>\r\n")); return; }
  if( num3>=ln_num ) { cmd_printf_P(PSTR("ERROR: <num3> to high\r\n")); return; }
  if( num1<=num3 && num3<=num2 ) { cmd_printf_P(PSTR("ERROR: <num3> can not be within <num1>..<num2>\r\n")); return; }
  if( num3==num2+1 ) { cmd_printf_P(PSTR("ERROR: move to same location ignored\r\n")); return; }
  // Report result before num1 and num2 are changed
  cmd_printf_P(PSTR("INFO: moved %X lines\r\n"),num2+1-num1);
  // Actual move
  while( num1<=num2 ) {
    ln_temp= ln_store[num1];
    if( num3<num1 ) { // move before
       for( uint16_t i=num1; i>num3; i--) ln_store[i]= ln_store[i-1];
       ln_store[num3]= ln_temp;
       num1++; num3++;
    } else { // move after
       for( uint16_t i=num1; i<num3; i++) ln_store[i]= ln_store[i+1];
       ln_store[num3-1]= ln_temp;
       num2--;
    }
  }
}

static uint16_t cmdprog_insert_linenum;

static void cmdprog_insert_stream(int argc, char * argv[]) {
  if( argc==0 ) { // no arguments toggles streaming mode
    if( cmd_get_streamfunc()==0 ) cmd_set_streamfunc(cmdprog_insert_stream); else cmd_set_streamfunc(0);
  } else {
    if( ln_num>=LN_NUM ) { cmd_printf_P(PSTR("ERROR: out of line memory\r\n")); return; }
    ln_t * ln= ln_parse(argc, argv);
    if( ln!=0 ) {
      for( uint16_t n=ln_num; n>cmdprog_insert_linenum; n-- ) ln_store[n]= ln_store[n-1];
      ln_store[cmdprog_insert_linenum++]= *ln; 
      ln_num++;
    }
  }
  // Update the streaming prompt (will only be shown in streaming mode)
  char buf[8]; snprintf_P(buf,sizeof buf, PSTR("P:%03X> "),cmdprog_insert_linenum); cmd_set_streamprompt(buf);
  
}

static void cmdprog_compile(int argc, char * argv[]) {
  // prog compile [ list | install | map ]
  if( argc>3 ) { cmd_printf_P(PSTR("ERROR: too many arguments\r\n"));  return; }
  int cmd= 0;
  if( argc==3 ) {
    if( cmd_isprefix(PSTR("map"),argv[2]) ) cmd=1;
    else if( cmd_isprefix(PSTR("install"),argv[2]) ) cmd=2;
    else if( cmd_isprefix(PSTR("list"),argv[2]) ) cmd=3;
    else if( cmd_isprefix(PSTR("bin"),argv[2]) ) cmd=4;
    else { cmd_printf_P(PSTR("ERROR: unexpected arguments\r\n")); return; }
  }
  bool ok=comp_compile();
  if( cmd==0 ) { return; }
  if( cmd==1 ) { comp_map(); return; }
  if( !ok ) { return; } 
  if( cmd==2 ) { comp_install(); return; }
  if( cmd==3 ) { comp_list(); return; }
  if( cmd==4 ) { comp_bin(); return; }
}


// ==========================================================================
// The command definition
// ==========================================================================


// The main command handler
static void cmdprog_main(int argc, char * argv[]) {
  if( argc>1 && cmd_isprefix(PSTR("insert"),argv[1]) ) { 
    if( argc==2 ) { 
      cmdprog_insert_linenum= ln_num;
      argc-=2; argv+=2; // skip 'prog insert addr'
    } else {
      uint16_t linenum;
      if( !cmd_parse(argv[2],&linenum) ) { cmd_printf_P(PSTR("ERROR: expected hex <linenum>\r\n")); return; }    
      if( linenum>ln_num ) { cmd_printf_P(PSTR("ERROR: <linenum> too high\r\n")); return; }
      cmdprog_insert_linenum= linenum;
      argc-=3; argv+=3; // skip 'prog insert addr'
    }
    cmdprog_insert_stream(argc, argv); 
    return;
  }
  if( argc>1 && cmd_isprefix(PSTR("replace"),argv[1]) ) { 
    if( argc<4 ) { cmd_printf_P(PSTR("ERROR: expected <linenum> and <line>\r\n")); return; }
    uint16_t linenum;
    if( !cmd_parse(argv[2],&linenum) ) { cmd_printf_P(PSTR("ERROR: expected hex <linenum>\r\n")); return; }    
    if( linenum>=ln_num ) { cmd_printf_P(PSTR("ERROR: <linenum> does not exist\r\n")); return; }
    ln_t * ln= ln_parse(argc-3, argv+3);
    if( ln!=0 ) { ln_del(&ln_store[linenum]);  ln_store[linenum]= *ln; }
    return;
  }
  if( argc>1 && strcmp_P(argv[1],PSTR("new"))==0 ) {
    cmdprog_new(argc,argv);
    return;
  }
  if( argc>1 && cmd_isprefix(PSTR("list"),argv[1]) ) { 
    cmdprog_list(argc,argv);
    return;
  }
  if( argc>1 && cmd_isprefix(PSTR("move"),argv[1]) ) { 
    cmdprog_move(argc,argv);
    return;
  }
  if( argc>1 && cmd_isprefix(PSTR("delete"),argv[1]) ) { 
    cmdprog_delete(argc,argv);
    return;
  }
  if( argc>1 && cmd_isprefix(PSTR("compile"),argv[1]) ) { 
    cmdprog_compile(argc,argv);
    return;
  }
  if( argc>1 && cmd_isprefix(PSTR("stat"),argv[1]) ) { 
    cmd_printf_P(PSTR("lines  used %X/%X"),ln_num,LN_NUM); 
    cmd_printf_P(PSTR("labels used %X/%X"),FS_NUM-1-fs_free(),FS_NUM-1); 
    if( argc>2 && cmd_isprefix(PSTR("strings"),argv[1]) ) fs_dump();
    return;
  }
  cmd_printf_P(PSTR("ERROR: unexpected arguments\r\n")); 
}


// Note cmd_register needs all strings to be PROGMEM strings. For longhelp we do that manually
const char cmdprog_longhelp[] PROGMEM = 
  "SYNTAX: prog list [<num1> [<num2>]]\r\n"
  "- lists the program from the line number <num1> to <num2>\r\n"
  "- if both <num1> and <num2> absent lists whole program"
  "- if <num2> is absent lists only line <num1>\r\n"
  "- if both present, lists lines <num1> upto <num2>\r\n"
  "- if both present, they may be '-', meaning 0 for <num1> and last for <num2>\r\n"
  "SYNTAX: prog stat [strings]\r\n"
  "- shows the memory usage of the program (and optionally the string table)\r\n"
  "SYNTAX: prog new [example]\r\n"
  "- deletes all lines of the program - 'new' can not be abbreviated\r\n"
  "- if 'example' is present, supplies example program\r\n"
  "SYNTAX: prog insert [<linenum> [<line>] ]\r\n"
  "- inserts <line> to program at position <linenum>\r\n"
  "- if <line> is absent, starts streaming mode (empty line ends it)\r\n"
  "- if <linenum> is absent, starts streaming mode at end of program\r\n"
  "SYNTAX: prog replace <linenum> <line> ]\r\n"
  "- overwrites the program at position <linenum> with <line>\r\n"
  "SYNTAX: prog move <num1> <num2> <num3>\r\n"
  "- moves lines <num1> up to and including <num2> to just before <num3>\r\n"
  "SYNTAX: prog delete <num1> [<num2>]\r\n"
  "- deletes the program lines from the line number <num1> to <num2>\r\n"
  "- if <num2> is absent deletes only line <num1>\r\n"
  "- if both present, deletes lines <num1> upto <num2>\r\n"
  "- if both present, they may be '-', meaning 0 for <num1> and last for <num2>\r\n"
  "SYNTAX: prog compile [ list | install | map | bin ]\r\n"
  "- compiles the program; giving info\r\n"
  "- 'list' compiles and produces an instruction listing\r\n"
  "- 'install' compiles and writes to memory\r\n"
  "- 'map' compiles and produces a table of labels and sections\r\n"
  "- 'bin' shows the generated binary\r\n"
;


// Note cmd_register needs all strings to be PROGMEM strings. For the short string we do that inline with PSTR.
void cmdprog_register(void) {
  fs_init();
  ln_init();
  cmd_register(cmdprog_main, PSTR("prog"), PSTR("edit and compile a program"), cmdprog_longhelp);
}
