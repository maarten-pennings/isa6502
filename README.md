# isa6502
Arduino library with tables describing 6502 instructions

## Introduction

This library contains several table that store the 6502 instruction set.
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
or 0x1000 in flash. 

Most of this is hidden in the library.
Except for the return values of type `char *`.
If you want to `print` those, embed them in the `f(...)` macro.
If you want to compare then, or get their length, use the `_P` version from the standard library: `strcmp_P` or `strlen_p`.
See the [isa6502dump](#isa6502dump) example.

For details on PROGMEM see [below](#progmem-details).

## Examples

There are examples of increasing code size.


### isa6502basic

There is a [basic example](examples/isa6502basic).
It includes the 6502 tables from this library, and just prints them.

Note that it shows how to print using `f()`:

```cpp
  Serial.print(  f(isa_addrmode_aname(aix)) ); 
```

and how to compare a string from a table using an `_p()` variant:

```cpp
  char aname[4]; 
  strcpy_P( aname, isa_addrmode_aname(aix) );
```

For details on PROGMEM (`f()` and `_p()`) see [below](#progmem-details).

### isa6502man

The next example is an interactive variant.
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

### isa6502prog

The third example is also based on my [command interpreter](https://github.com/maarten-pennings/cmd).
It uses all commands includes in this library; not only `man` , but also `read` and `write`, and `dasm` and `asm`.

Here a demo of a `write` that is `dasm`'ed.

```txt
Welcome to isa6502prog, using  is6502 lib V5

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
 
## PROGMEM details

Recall that a pointer `char * p` with a value 0x1000 could address location 0x1000 in DATAMEM (RAM) or 0x1000 in PROGMEM (flash). 
Technically the AVR has two different instructions for that, and the C compiler must generate those.
An unqualified `*p` in C is always a RAM location. To read from flash we need a modifier like `pgm_read_byte(p)`. This inserts
an `lpm` instruction (load from program memory).

The run-time library supports this; for many functions like `memcmp(p1,p2)` (where both p1 and p2 are in RAM) there is
a variant `memcmp_P(p1,p2)` (where `p2` is in PROGMEM).

A special case is a string. If a string is in PROGMEM, all its characters have an address in PROGMEM. 
Arduino could have made a `Serial.print_p()`. But `print()` is an overloaded (cpp) method of Serial.
It magically maps to `print(char)`, `print(int)`, or `print(char*)`. However there is no type difference between a `char *` in RAM 
and a `char *` in flash. The trick Arduino used is to make an empty class (`class __FlashStringHelper`), 
and cast the `char *` to that class if that pointer points to flash. With the trick, overloading can be used again.

To store a variable in PROGMEM, add attribute `PROGMEM` (and make sure it is constant and not on the stack - `static`). 
To print it, use the above mentioned `pgm_read_byte()`/`pgm_read_word()` on the _address_ of the integer.

```cpp
static const int i PROGMEM = 5;
Serial.println( (char) pgm_read_byte(&i) );
```

On my machine PROGMEM is defined in `C:\Users\Maarten\AppData\Local\Arduino15\packages\arduino\tools\avr-gcc\7.3.0-atmel3.6.1-arduino5\avr\include\avr\pgmspace.h`, roughly (simplified) as `#define  PROGMEM   __attribute__((__progmem__))`.

To put a whole (character) array in PROGMEM, again add `PROGMEM` but also note the `const` and the `static`. 
To print the first character, use the above mentioned `pgm_read_byte()`. 
To print the whole string, use the `f()` macro, so that the correct overloaded version of `print()` is selected by the compiler.

```cpp
static const char s[] PROGMEM = "The content";
Serial.println( (char) pgm_read_byte(s) );
Serial.println( f(s) );
```

For some reason, the `f()` macro must be defined by ourselves.

```cpp
#include <avr/pgmspace.h>
#define f(s) ((__FlashStringHelper *)(s))
```

An alternative to having an array, is to have a pointer (to an array). Note that `s` is in RAM, but the chars are in PROGMEM.

```cpp
const char * s = PSTR("Content");
Serial.println( (char) pgm_read_byte(s) );
Serial.println( f(s) );
```

For simple literals, for example in `print`, there is no need to make an explicit variable:

```cpp
Serial.println( f( PSTR("Example") ) );
```

This occurs so often, that Arduino has made a macro for that: `F()`. Why `F` is publsihed and `f` not is a mystery to me:

```cpp
Serial.println( F("Example") );
```

It gets even harder when a table of strings needs to be mapped to flash. Note that `strings` is an array of three pointers, 
all of which are in PROGMEM. So, `pgm_read_word` is needed to read the pointer. That pointed is dressed up as a `__FlashStringHelper`
so that the right version of `print` is selected. That `print` uses `pgm_read_byte` to read the characters, which are also in
PROGMEM.

```cpp
static const char s0[] PROGMEM = "foo";
static const char s1[] PROGMEM = "bar";
static const char s2[] PROGMEM = "baz";
static const char * const strings[] PROGMEM = { s0, s1, s2 };
Serial.println( f(pgm_read_word(&strings[2])) );
```

Unfortunately, `PSTR` can only be used within a function, so ths **does not work**.

```cpp
static const char * const strings[] PROGMEM = { PSTR("foo"), PSTR("bar"), PSTR("baz") };
```

(end of doc)
