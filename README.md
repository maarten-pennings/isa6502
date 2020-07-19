# isa6502

Arduino library with tables describing the 6502 instruction set architecture (ISA).

The library also includes commands (for a separate command interpreter) 
that implement a man page browser (for the instructions), an in-line assembler/disassembler, 
and even a simple "file" based assembler.

There is a [test](test/#readme.md) suite for the commands.

## Introduction

This library contains several tables that store the 6502 instruction set.
See for example [this excellent work](https://www.masswerk.at/6502/6502_instruction_set.html).
The following tables are available:

- **addressing mode**  
  example: `ABS` (absolute), `ZPG` (zero page)
- **instruction**  
  example: `LDA` (load accumulator), `JMP` (jump)
- **opcode**  
  example: 0xA9 (`LDA.IMM`)
  
In order to save RAM, the tables are stored in flash.
On AVR, that is known as PROGMEM.
Note that the AVRs have a Harvard architecture.
This means that every address occurs twice, once as DATAMEM (RAM), once as PROGMEM (flash).
This means that if a pointer like `char * p` has the value 0x1000, it could address location 0x1000 in RAM 
or 0x1000 in flash. The compiler doesn't know, the programmer has to help.

Most of this is hidden in the library.
Except for the return values of type `char *`.
For example, the library offers the function

```cpp
/*PROGMEM*/ const char * isa_addrmode_aname ( int aix );    
```

In other words `isa_addrmode_aname` returns a pointer to characters in PROGMEM.

- If you want to `print` those, embed them in the `f(...)` macro.  
  `Serial.print( f(isa_addrmode_aname(4)) )`
- If you want to `printf` those, use the `%S` format (capital S).  
  `sprintf(buf, "out[%S]",isa_addrmode_aname(4))`
- If you want to compare then, or get their length, use the `_P` version from the standard library: `strcmp_P` or `strlen_P`.  
  `if( strcmp_P("ABS",isa_addrmode_aname(4))==0 ) {}`

For actual sample code, see [isa6502basic](#isa6502basic) or one of the other [examples](examples).

For more explanation on PROGMEM see [below](#progmem-details).

## Examples

There are examples of increasing code size.

### isa6502basic

There is a [basic example](examples/isa6502basic).
It includes the 6502 tables from this library, and just prints them.

Note that this example shows how to deal with PROGMEM strings.
For example, `f` typecasts the string, so that `print` knows to get the chars from PROGMEM.

```cpp
  Serial.print(  f(isa_addrmode_aname(aix)) ); 
```

This is another example that shows how to deal with PROGMEM strings.
There are `_P()` variants of many standard string functions.
Those variants get the chars of the (second) string from PROGMEM.

```cpp
  char aname[4]; 
  strcpy_P( aname, isa_addrmode_aname(aix) );
```

For details on PROGMEM (`f()` and `_P()`) see [below](#progmem-details).

### isa6502man

The [next example](examples/isa6502man) is an interactive variant.
It uses my [command interpreter](https://github.com/maarten-pennings/cmd)
to offer a `man` command, so that the user can query the 6502 tables.

This library comes with multiple commands, but this [example](examples/isa6502man), only uses [man](src/cmdman.cpp).

Here is an example of a session.

```text
Welcome to isa6502man, using  is6502 lib V5

Type 'help' for help
>> help man
SYNTAX: man
- shows an index of instruction types (eg LDA) and addressing modes (eg ABS)
SYNTAX: man <inst>
- shows the details of the instruction type <inst> (eg LDA)
SYNTAX: man <addrmode>
- shows the details of the addressing mode <addrmode> (eg ABS)
SYNTAX: man <hexnum> | ( <inst> <addrmode> )
- shows the details of the instruction variant with opcode <hexnum>
- alternatively, the variant is identified with type and addressing mode
SYNTAX: man find <pattern>
- lists the instruction types, if <pattern> matches their description
- <pattern> is a series of letters; the match is case insensitive
- <pattern> may contain *, this matches zero or more chars
- <pattern> may contain ?, this matches any char
SYNTAX: man table opcode
- prints a 16x16 table of opcodes
SYNTAX: man table <pattern>
- prints a table of instructions (that match pattern - default pattern is *)
SYNTAX: man regs
- lists details of the registers
>>
>> man LDX
name: LDX (instruction)
desc: load index X with memory
help: X <- M
flag: NvxbdiZc
addr: ABS ABY IMM ZPG ZPY 
>>
>> man ZPG
name: ZPG (addressing mode)
desc: Zero page
sntx: OPC *LL
size: 2 bytes
inst: ADC AND ASL BIT CMP CPX CPY DEC EOR INC LDA LDX LDY LSR ORA ROL 
inst: ROR SBC STA STX STY 
>>
>> man LDX ZPG
name: LDX.ZPG (opcode A6)
sntx: LDX *LL
desc: load index X with memory - Zero page
help: X <- M
flag: NvxbdiZc
size: 2 bytes
time: 3 ticks
>>
>> man A6
name: LDX.ZPG (opcode A6)
sntx: LDX *LL
desc: load index X with memory - Zero page
help: X <- M
flag: NvxbdiZc
size: 2 bytes
time: 3 ticks
>>
>> man find S*X
STX - store index X in memory
STY - store index Y in memory
TAX - transfer accumulator to index X
TAY - transfer accumulator to index Y
TSX - transfer stack pointer to index X
TXA - transfer index X to accumulator
TXS - transfer index X to stack register
TYA - transfer index Y to accumulator
found 8 results
>> 
```

### isa6502mem

The [third example](examples/isa6502mem) is also based on my [command interpreter](https://github.com/maarten-pennings/cmd).
It uses several commands included in this library; not only `man` , but also `read` and `write`, and `dasm` and `asm`.

Here a demo of a `write` that is `dasm`'ed.

```txt
Welcome to isa6502mem, using  is6502 lib V5

Type 'help' for help
>> write fffc 00 00 66 66 78 D8 A2 FF 9A A9 00 8D 00 80 A9 FF 8D 00 80 D0 F4
>> dasm
fffc 00       BRK
fffd 00       BRK
fffe 66 66    ROR *66
0000 78       SEI
0001 d8       CLD
0002 a2 ff    LDX #ff
0004 9a       TXS
0005 a9 00    LDA #00
>> d
0007 8d 00 80 STA 8000
000a a9 ff    LDA #ff
000c 8d 00 80 STA 8000
000f d0 f4    BNE +f4 (0005)
0011 00       BRK
0012 00       BRK
0013 00       BRK
0014 00       BRK
>> 
>> 
```

Here a demo of an `asm`'d program that is `read`.

```txt
>> asm
A:0200> sei
A:0201> cld
A:0202> ldx ff
INFO: asm: suggest ZPG instead of ABS (try - for undo)
A:0205> -
A:0202> ldx #ff
A:0204> txs
A:0205> lda #00
A:0207> sta 8000
A:020a> lda #ff
A:020c> sta 8000
A:020f> bne 0205
A:0211> 
>>
>> d
0200 78       SEI
0201 d8       CLD
0202 a2 ff    LDX #ff
0204 9a       TXS
0205 a9 00    LDA #00
0207 8d 00 80 STA 8000
020a a9 ff    LDA #ff
020c 8d 00 80 STA 8000
>> d
020f d0 f4    BNE +f4 (0205)
0211 00       BRK
0212 00       BRK
0213 00       BRK
0214 00       BRK
0215 00       BRK
0216 00       BRK
0217 00       BRK
>> 
>> r
0200: 78 d8 a2 ff 9a a9 00 8d 00 80 a9 ff 8d 00 80 d0
0210: f4 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0220: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0230: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
>> 
```

Note
 - The `asm` command was not given an address, and defaults to `0200` (page 0 and 1 are typically reserved on a 6502).
 - At address 0202 I made a mistake, I typed `ldx ff` where I intended `ldx #ff`.
 - The assembler gave a warning, because the operand size (1 byte) did not match the instruction (2 bytes).
 - I used the "undo" feature (`-` goes one instruction back), and corrected.
 - At address 020f I used absolute addressing syntax for `bne`, but that instruction only has relative addressing. The assembler silently accepts.
 - I quit the streaming mode at address 0211 by entering a blank line.
 - Then I gave twice the disassemble command (which knows the last written or shown address). This confirms the assembled code is correct.
 - Finally, a gave a read command (which also knows the last written address). 


### isa6502prog

The [fourth example](examples/isa6502prog) is also based on my [command interpreter](https://github.com/maarten-pennings/cmd).
It adds one commands included in this library `prog`.
This command implements a simple assembler.


## PROGMEM details

Recall that a pointer `char * p` with a value 0x1000 could address location 0x1000 in DATAMEM (RAM) or 0x1000 in PROGMEM (flash). 
Technically the AVR has two different instructions for that, and the C compiler must generate those.
An unqualified `*p` in C is always a RAM location. To read from flash we need a modifier like `pgm_read_byte(p)`. This inserts
an `lpm` instruction (load from program memory) instead of `ld` (load from ram).

The run-time library supports PROGMEM; for many functions like `memcmp(p1,p2)` (where both p1 and p2 are in RAM) there is
a variant `memcmp_P(p1,p2)` (where `p2` is in PROGMEM).

### Scalar in PROGEM

To store a (scalar) variable in PROGMEM, add attribute `PROGMEM` (and make sure it is constant and global - not on the stack). 
To print it, use the above mentioned `pgm_read_byte()`/`pgm_read_word()` on the _address_ of the integer (signed char).

```cpp
const signed char i PROGMEM = 5;
Serial.println( (signed char) pgm_read_byte(&i) );
```

For a real `integer` we need to replace `pgm_read_byte` with `pgm_read_word` and hope `int` has the same size as `word`.

```cpp
const int i PROGMEM = 5;
Serial.println( (int) pgm_read_word(&i) );
```

On my machine PROGMEM is defined in 
`C:\Users\Maarten\AppData\Local\Arduino15\packages\arduino\tools\avr-gcc\7.3.0-atmel3.6.1-arduino5\avr\include\avr\pgmspace.h`, 
roughly (simplified) as `#define  PROGMEM   __attribute__((__progmem__))`.

### Array in PROGEM

To store a whole (character) array in PROGMEM, again add `PROGMEM` but also note the `const` (and it must be global - not on the stack). 
To print its first character, use the above mentioned `pgm_read_byte()`. 

```cpp
const char s[] PROGMEM = "The content";
Serial.println( (char) pgm_read_byte(&s[0]) );
```

A special case is a printing the whole string. If a string is in PROGMEM, all its characters have an address in PROGMEM. 
Arduino could have made a `Serial.print_P()`. But `print()` is an overloaded (cpp) method of Serial.
It selects the right variant, `print(char)`, `print(int)`, `print(char*)`, etc, based on the type of the parameter. 
However there is no type difference between a `char *` in RAM and a `char *` in PROGMEM.
The trick Arduino uses is to make an empty class (`class __FlashStringHelper`), 
and cast a `char *` to that class if that pointer points to flash. With the trick, the string has a cpp class type,
and overloading can be used again.

```cpp
const char s[] PROGMEM = "The content";
Serial.println( (__FlashStringHelper *)(s) );
```

For some reason, Arduino did not make a handy shortcut for that.
So, I made my own `f()` macro 

```cpp
#include <avr/pgmspace.h>
#define f(s) ((__FlashStringHelper *)(s))
```

so that I can write

```cpp
const char s[] PROGMEM = "The content";
Serial.println( f(s) );
```

### RAM pointer to PROGMEM array

An alternative to having an array `s`, is to have a pointer `s` to an array, see fragment below. 
Note that `s` itself is in RAM, but the chars it points to, the actual array, is in PROGMEM.
We need something to map a literal to PROGMEM; the standard header `pgmspace.h` defines a helper macro `PSTR` for that.
This is the [simplified](https://github.com/vancegroup-mirrors/avr-libc/blob/master/avr-libc/include/avr/pgmspace.h#L401)
version (the [real](https://github.com/vancegroup-mirrors/avr-libc/blob/master/avr-libc/include/avr/pgmspace.h#L404) is harder):

```cpp
# define PSTR(s) ((const PROGMEM char *)(s))
```

So letting a pointer `s` point to characters in PROGMEM is now written as

```cpp
const char * s = PSTR("Content");
Serial.println( (char) pgm_read_byte(&s[0]) );
Serial.println( f(s) );
```

For literals in `print`, there is no need to make an explicit variable:

```cpp
Serial.println( f( PSTR("Example") ) );
```

This combination `f(PSTR("..."))` occurs so often, that Arduino has made a macro for that: `F("...")`. 

```cpp
Serial.println( F("Example") );
```

You now maybe understand why _my_ macro is called lower-case `f()`.
Why `F` is published by Arduino and `f` not, is a mystery to me.
By the way, on my system `F` is published in 
[wstring.h](https://github.com/Robots-Linti/DuinoPack-v1.2/blob/master/win/hardware/arduino/avr/cores/arduino/WString.h#L38): 
`C:\Program Files (x86)\Arduino\hardware\arduino\avr\cores\arduino\WString.h`.

### PROGMEM pointers to PROGMEM array

Storing string gets harder when a _table_ of strings needs to be mapped to flash. 
In the code below `strings` is an array of three pointers, all of which are in PROGMEM. 
So, `pgm_read_word` is needed to read the pointer. 
That pointed is dressed up as a `__FlashStringHelper` so that the right version of `print` is selected. 
That `print` uses `pgm_read_byte` to read the characters, which are also in PROGMEM.

```cpp
static const char s0[] PROGMEM = "foo";
static const char s1[] PROGMEM = "bar";
static const char s2[] PROGMEM = "baz";
static const char * const strings[] PROGMEM = { s0, s1, s2 };
Serial.println( f(pgm_read_word(&strings[2])) );
```

Unfortunately, `PSTR` can only be used within a function, so the following **does not work**.
I do not know why, but even [Arduino site](https://www.arduino.cc/reference/en/language/variables/utilities/progmem/) 
uses the above structure and not the below.

```cpp
static const char * const strings[] PROGMEM = { PSTR("foo"), PSTR("bar"), PSTR("baz") };
```

(end of doc)
