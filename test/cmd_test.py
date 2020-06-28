# cmd_test.py - test cases for the isa6502prog example
# https://docs.python.org/3/library/unittest.html

import cmd
import time
import unittest
import serial


try:
  # ports=cmd.findports()
  port= 'COM11'
  c= cmd.Cmd(port)
  c.close(True)
except serial.SerialException :  
  print(f"ERROR: no command interpreter on port {port} (see cmd_test.py line 12)")
  exit(-1)


##########################################################################
### cmd (the interpreter itself)
##########################################################################

class Test_cmd(unittest.TestCase):
  def setUp(self):
    self.cmd= cmd.Cmd(port)
    
  def tearDown(self):
    self.cmd.close()
    self.cmd= None

  # Each character received by the command interpreter is appended to its input buffer.
  def test_buffered(self):
    self.cmd.serial.write(b"he") # directly send some chars (should be buffered)
    r= self.cmd.exec("lp")
    self.assertIn("Available commands",r)

  # The command interpreter echo's each received character, so local echo is not needed.
  def test_echo(self):
    r= self.cmd.exec("echo enable")
    self.assertEqual(r, "echo: enabled\r\n")
    r= self.cmd.exec("echo disable")
    self.assertEqual(r, "echo disable\r\necho: disabled\r\n") # echo was on, so we get our command back

  # Backspace deletes the last character from the input buffer (echo’s BS SPC BS).
  def test_bs(self):
    self.cmd.exec("echo enable")
    r= self.cmd.exec("#A\b")
    self.assertIn("#A\b \b",r) # A is erased with BS SPC BS
    self.cmd.exec("echo disable")

  # All characters from // till end-of-line are considered comment.
  def test_comment(self):
    r= self.cmd.exec("help help xxx")
    self.assertEqual(r,"ERROR: too many arguments\r\n")
    r= self.cmd.exec("help help // xxx")
    self.assertIn("SYNTAX: help",r)

  # The input buffer size is 128 characters (max number of characters until end-of-line).
  def test_buf1024(self):
    # First check that 127 chars fit
    self.cmd.serial.write(b"echo ")
    for line in range(0,(128-5-1)//16): 
        self.cmd.serial.write(b"0123456789abcdef")
        time.sleep(0.1) # Prevent "characters lost"
    r= self.cmd.exec("0123456PQR")
    self.assertNotIn("_\b",r) # no visual-alarm send back
    self.assertIn("PQR",r)
    # Now check that char nr 128 does not fit
    self.cmd.serial.write(b"echo ")
    for line in range(0,(128-5-1)//16): 
        self.cmd.serial.write(b"0123456789abcdef")
        time.sleep(0.1) # Prevent "characters lost"
    r= self.cmd.exec("0123456pqrs")
    self.assertIn("pqr",r)
    self.assertIn("_\b",r) # visual-alarm send back
    self.assertNotIn("pqrs",r)

  # The command can have 32 arguments max
  def test_arg32(self):
    # First check that 32 args fit
    args="1"
    for arg in range(2,32): args= args+" "+str(arg)
    r= self.cmd.exec("echo "+args)
    self.assertEqual(args+"\r\n",r)
    # Now check that 33 does not fit
    args="1"
    for arg in range(2,33): args= args+" "+str(arg)
    r= self.cmd.exec("echo "+args)
    self.assertEqual("ERROR: too many arguments\r\n",r)

  # The empty string is a valid command.
  def test_empty(self):
    r= self.cmd.exec("")
    self.assertEqual(r, "") 

  # When a command name is not in the list of supported commands, an error is given    
  def test_unknown(self):
    r= self.cmd.exec("foo")
    self.assertEqual(r, "ERROR: command not found (try help)\r\n" )


##########################################################################
### Help
##########################################################################

class Test_help(unittest.TestCase):
  def setUp(self):
    self.cmd= cmd.Cmd(port)
    
  def tearDown(self):
    self.cmd.close()
    self.cmd= None

  # Commands and arguments may be shortened.
  def test_shorten(self):
    r= self.cmd.exec("help")
    self.assertIn("help -",r) 
    r= self.cmd.exec("hel")
    self.assertIn("help -",r) 
    r= self.cmd.exec("he")
    self.assertIn("help -",r) 
    r= self.cmd.exec("h")
    self.assertIn("help -",r) 
    r= self.cmd.exec("help help")
    self.assertIn("SYNTAX: help",r)
    r= self.cmd.exec("help hel")
    self.assertIn("SYNTAX: help",r)
    r= self.cmd.exec("help he")
    self.assertIn("SYNTAX: help",r)
    r= self.cmd.exec("help h")

  # The long help gives details
  def test_longhelp(self):
    r= self.cmd.exec("help help")
    # Check if all sections are there
    self.assertIn("SYNTAX: help",r) 
    self.assertIn("SYNTAX: help <cmd>",r) 
    self.assertIn("NOTES:",r) 
    # Check notes
    self.assertIn("shortened",r) 
    self.assertIn(">>",r) 
    self.assertIn("//",r) 
    self.assertIn("@",r) 

  # Erroneous args
  def test_errargs(self):
    r= self.cmd.exec("help OOPS")
    self.assertIn("ERROR: command not found (try 'help')\r\n",r) 
    r= self.cmd.exec("help help OOPS")
    self.assertIn("ERROR: too many arguments\r\n",r) 

  ## Test for main features ##############################################
  
  # The help command lists all commands
  def test_list(self):
    r= self.cmd.exec("help")
    self.assertIn("asm -",r) 
    self.assertIn("dasm -",r) 
    self.assertIn("echo -",r) 
    self.assertIn("help -",r) 
    self.assertIn("man -",r) 
    self.assertIn("read -",r) 
    self.assertIn("write -",r) 
    self.assertIn("prog -",r)

  # The help <cmd> command lists details
  def test_sub(self):
    r= self.cmd.exec("help help")
    self.assertIn("SYNTAX: help",r) 


##########################################################################
### echo
##########################################################################

class Test_echo(unittest.TestCase):
  def setUp(self):
    self.cmd= cmd.Cmd(port)
    
  def tearDown(self):
    self.cmd.close()
    self.cmd= None

  # Commands and arguments may be shortened.
  def test_shorten(self):
    # Main full and shortened
    r= self.cmd.exec("echo")
    self.assertIn("echo: disabled\r\n",r) 
    r= self.cmd.exec("e")
    self.assertIn("echo: disabled\r\n",r) 
    # Sub 'line' full and shortened
    r= self.cmd.exec("e line Hello")
    self.assertIn("Hello\r\n",r) 
    r= self.cmd.exec("e l Hello")
    self.assertIn("Hello\r\n",r) 
    # Sub 'error' full and shortened
    r= self.cmd.exec("e error")
    self.assertIn("echo: errors: ",r) 
    r= self.cmd.exec("e e")
    self.assertIn("echo: errors: ",r) 
    # Sub 'enable' full and shortened
    r= self.cmd.exec("e enable")
    self.assertEqual("echo: enabled\r\n",r) 
    r= self.cmd.exec("e en")
    self.assertIn("echo: enabled\r\n",r) 
    # Sub 'disable' full and shortened
    r= self.cmd.exec("e disable")
    self.assertIn("echo: disabled\r\n",r) 
    r= self.cmd.exec("e d")
    self.assertEqual("echo: disabled\r\n",r) 

  # The long help gives details
  def test_longhelp(self):
    r= self.cmd.exec("help echo")
    # Check if all sections are there
    self.assertIn("SYNTAX: echo [line] <word>...",r) 
    self.assertIn("SYNTAX: [@]echo errors",r) 
    self.assertIn("SYNTAX: [@]echo [ enable | disable ]",r) 
    self.assertIn("NOTES:",r) 

  # Erroneous args
  def test_errargs(self):
    r= self.cmd.exec("e en foo")
    self.assertIn("ERROR: unexpected argument after 'enable'\r\n",r) 
    r= self.cmd.exec("e dis foo")
    self.assertIn("ERROR: unexpected argument after 'disable'\r\n",r) 
    r= self.cmd.exec("e er foo")
    self.assertIn("ERROR: unexpected argument after 'errors'\r\n",r) 

  ## Test for main features ##############################################
  
  # The enable/disable subcommand enable/disable echoing and show echoing status
  def test_enabledisable(self):
    r= self.cmd.exec("echo")
    self.assertEqual("echo: disabled\r\n",r) 
    r= self.cmd.exec("echo enable")
    self.assertEqual("echo: enabled\r\n",r) 
    r= self.cmd.exec("echo disable")
    self.assertEqual("echo disable\r\necho: disabled\r\n",r) 
    # no output
    r= self.cmd.exec("@e enable")
    self.assertEqual("",r) 
    r= self.cmd.exec("@e d")
    self.assertEqual("@e d\r\n",r) 

  # The errors subcommand shows error status
  def test_echo_errors(self):
    r= self.cmd.exec("echo errors") # clear counter
    r= self.cmd.exec("echo errors")
    self.assertEqual("echo: errors: 0\r\n",r) 
    r= self.cmd.exec("echo errors step")
    self.assertEqual("echo: errors: stepped\r\n",r) 
    r= self.cmd.exec("echo errors")
    self.assertEqual("echo: errors: 1\r\n",r) 
    r= self.cmd.exec("@echo errors")
    self.assertEqual("",r) # @ for now output
    r= self.cmd.exec("@echo errors step")
    self.assertEqual("",r) # @ for now output

  # line subcommand
  def test_echo_line(self):
    r= self.cmd.exec("echo Hello, world!")
    self.assertIn("Hello, world!",r) 
    r= self.cmd.exec("echo Hello,  world!") # spaces get collapsed
    self.assertIn("Hello, world!",r) 
    r= self.cmd.exec("echo line enable")
    self.assertEqual("enable\r\n",r) 
    r= self.cmd.exec("echo line") # echo's an empty line
    self.assertEqual("\r\n",r) 


##########################################################################
### man
##########################################################################

class Test_man(unittest.TestCase):
  def setUp(self):
    self.cmd= cmd.Cmd(port)
    
  def tearDown(self):
    self.cmd.close()
    self.cmd= None

  # Commands and arguments may be shortened.
  def test_shorten(self):
    r= self.cmd.exec("man f add")
    self.assertIn("ADC - add",r) 
    r= self.cmd.exec("man t")
    self.assertIn("|ABS|ABX|",r) 
    r= self.cmd.exec("man t o")
    self.assertIn("|00 |01 |",r) 
    r= self.cmd.exec("man r")
    self.assertIn("accumulator",r) 

  # The long help gives details
  def test_longhelp(self):
    r= self.cmd.exec("help man")
    # Check if all sections are there
    self.assertIn("SYNTAX: man\r\n",r) 
    self.assertIn("SYNTAX: man <inst>\r\n",r) 
    self.assertIn("SYNTAX: man <addrmode>\r\n",r) 
    self.assertIn("SYNTAX: man <hexnum>",r) 
    self.assertIn("SYNTAX: man find",r) 
    self.assertIn("SYNTAX: man table",r) 
    self.assertIn("SYNTAX: man regs",r) 

  # Erroneous args
  def test_errargs(self):
    r= self.cmd.exec("man find")
    self.assertEqual("ERROR: need a search word\r\n",r) 
    r= self.cmd.exec("man find adc oops")
    self.assertEqual("ERROR: only one search word allowed\r\n",r) 
    r= self.cmd.exec("man regs oops")
    self.assertEqual("ERROR: no arguments allowed\r\n",r) 
    r= self.cmd.exec("man table adc oops")
    self.assertEqual("ERROR: too many arguments\r\n",r) 
    r= self.cmd.exec("man oops")
    self.assertEqual("ERROR: must have <inst>, <addrmode>, <hexnum>; or a subcommand\r\n",r) 
    r= self.cmd.exec("man foo bar")
    self.assertEqual("ERROR: instruction 'foo' does not exist\r\n",r) 
    r= self.cmd.exec("man adc bar")
    self.assertEqual("ERROR: addressing mode 'bar' does not exist\r\n",r) 
    r= self.cmd.exec("man adc rel")
    self.assertEqual("ERROR: instruction 'adc' does not have addressing mode 'rel'\r\n",r) 
    r= self.cmd.exec("man adc rel foo ")
    self.assertEqual("ERROR: unexpected arguments\r\n",r) 

  ## Test for main features ##############################################
  
  # The (empty) 'man' command lists all instructions and addressing modes
  def test_index(self):
    r= self.cmd.exec("man")
    self.assertIn("inst: ADC AND ASL BCC BCS BEQ BIT BMI BNE BPL BRK BVC BVS CLC CLD CLI\r\n",r) 
    self.assertIn("inst: CLV CMP CPX CPY DEC DEX DEY EOR INC INX INY JMP JSR LDA LDX LDY\r\n",r) 
    self.assertIn("inst: LSR NOP ORA PHA PHP PLA PLP ROL ROR RTI RTS SBC SEC SED SEI STA\r\n",r) 
    self.assertIn("inst: STX STY TAX TAY TSX TXA TXS TYA\r\n",r) 
    self.assertIn("addr: ABS ABX ABY ACC IMM IMP IND REL ZIX ZIY ZPG ZPX ZPY\r\n",r) 

  # The 'man xxx' command lists instruction, addressing mode or opcodes
  def test_instamode(self):
    r= self.cmd.exec("man adc")
    self.assertIn("name: ADC (instruction)",r) 
    self.assertIn("desc: ",r) 
    self.assertIn("help: ",r) 
    self.assertIn("flag: ",r) 
    self.assertIn("addr: ",r) 
    r= self.cmd.exec("man abs")
    self.assertIn("name: ABS (addressing mode)",r) 
    self.assertIn("desc: ",r) 
    self.assertIn("sntx: ",r) 
    self.assertIn("size: ",r) 
    self.assertIn("inst: ",r) 
    r= self.cmd.exec("man 44")
    self.assertIn("name: not in use",r) 
    r= self.cmd.exec("man 6a")
    self.assertIn("name: ROR.ACC (opcode 6A)",r) 
    self.assertIn("sntx: ",r) 
    self.assertIn("desc: ",r) 
    self.assertIn("help: ",r) 
    self.assertIn("flag: ",r) 
    self.assertIn("size: ",r) 
    self.assertIn("time: ",r) 
    r= self.cmd.exec("man ror acc")
    self.assertIn("name: ROR.ACC (opcode 6A)",r) 
    r= self.cmd.exec("man ror rel")
    self.assertIn("ERROR: instruction 'ror' does not have addressing mode 'rel'",r) 

  # The 'man find' command lists instructions found via help
  def test_find(self):
    r= self.cmd.exec("man find xxx")
    self.assertIn("no instructions found",r) 
    r= self.cmd.exec("man find branch")
    self.assertIn("BNE - branch on result not zero",r) 
    self.assertIn("found 8 instructions",r) 
    r= self.cmd.exec("man find branch*zero")
    self.assertIn("found 2 instructions",r) 
    r= self.cmd.exec("m f index???by")
    self.assertIn("found 4 instructions",r) 

  # The 'man table' command tabulates instructions
  def test_table(self):
    r= self.cmd.exec("man table opcode")
    t= "+--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+\r\n"\
       "|  |00 |01 |02 |03 |04 |05 |06 |07 |08 |09 |0A |0B |0C |0D |0E |0F |\r\n"\
       "+--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+\r\n"\
       "|00|BRK|ORA|   |   |   |ORA|ASL|   |PHP|ORA|ASL|   |   |ORA|ASL|   |\r\n"\
       "|  |IMP|ZIX|   |   |   |ZPG|ZPG|   |IMP|IMM|ACC|   |   |ABS|ABS|   |\r\n"\
       "|10|BPL|ORA|   |   |   |ORA|ASL|   |CLC|ORA|   |   |   |ORA|ASL|   |\r\n"\
       "|  |REL|ZIY|   |   |   |ZPX|ZPX|   |IMP|ABY|   |   |   |ABX|ABX|   |\r\n"\
       "|20|JSR|AND|   |   |BIT|AND|ROL|   |PLP|AND|ROL|   |BIT|AND|ROL|   |\r\n"\
       "|  |ABS|ZIX|   |   |ZPG|ZPG|ZPG|   |IMP|IMM|ACC|   |ABS|ABS|ABS|   |\r\n"\
       "|30|BMI|AND|   |   |   |AND|ROL|   |SEC|AND|   |   |   |AND|ROL|   |\r\n"\
       "|  |REL|ZIY|   |   |   |ZPX|ZPX|   |IMP|ABY|   |   |   |ABX|ABX|   |\r\n"\
       "+--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+\r\n"\
       "|40|RTI|EOR|   |   |   |EOR|LSR|   |PHA|EOR|LSR|   |JMP|EOR|LSR|   |\r\n"\
       "|  |IMP|ZIX|   |   |   |ZPG|ZPG|   |IMP|IMM|ACC|   |ABS|ABS|ABS|   |\r\n"\
       "|50|BVC|EOR|   |   |   |EOR|LSR|   |CLI|EOR|   |   |   |EOR|LSR|   |\r\n"\
       "|  |REL|ZIY|   |   |   |ZPX|ZPX|   |IMP|ABY|   |   |   |ABX|ABX|   |\r\n"\
       "|60|RTS|ADC|   |   |   |ADC|ROR|   |PLA|ADC|ROR|   |JMP|ADC|ROR|   |\r\n"\
       "|  |IMP|ZIX|   |   |   |ZPG|ZPG|   |IMP|IMM|ACC|   |IND|ABS|ABS|   |\r\n"\
       "|70|BVS|ADC|   |   |   |ADC|ROR|   |SEI|ADC|   |   |   |ADC|ROR|   |\r\n"\
       "|  |REL|ZIY|   |   |   |ZPX|ZPX|   |IMP|ABY|   |   |   |ABX|ABX|   |\r\n"\
       "+--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+\r\n"\
       "|80|   |STA|   |   |STY|STA|STX|   |DEY|   |TXA|   |STY|STA|STX|   |\r\n"\
       "|  |   |ZIX|   |   |ZPG|ZPG|ZPG|   |IMP|   |IMP|   |ABS|ABS|ABS|   |\r\n"\
       "|90|BCC|STA|   |   |STY|STA|STX|   |TYA|STA|TXS|   |   |STA|   |   |\r\n"\
       "|  |REL|ZIY|   |   |ZPX|ZPX|ZPY|   |IMP|ABY|IMP|   |   |ABX|   |   |\r\n"\
       "|A0|LDY|LDA|LDX|   |LDY|LDA|LDX|   |TAY|LDA|TAX|   |LDY|LDA|LDX|   |\r\n"\
       "|  |IMM|ZIX|IMM|   |ZPG|ZPG|ZPG|   |IMP|IMM|IMP|   |ABS|ABS|ABS|   |\r\n"\
       "|B0|BCS|LDA|   |   |LDY|LDA|LDX|   |CLV|LDA|TSX|   |LDY|LDA|LDX|   |\r\n"\
       "|  |REL|ZIY|   |   |ZPX|ZPX|ZPY|   |IMP|ABY|IMP|   |ABX|ABX|ABY|   |\r\n"\
       "+--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+\r\n"\
       "|C0|CPY|CMP|   |   |CPY|CMP|DEC|   |INY|CMP|DEX|   |CPY|CMP|DEC|   |\r\n"\
       "|  |IMM|ZIX|   |   |ZPG|ZPG|ZPG|   |IMP|IMM|IMP|   |ABS|ABS|ABS|   |\r\n"\
       "|D0|BNE|CMP|   |   |   |CMP|DEC|   |CLD|CMP|   |   |   |CMP|DEC|   |\r\n"\
       "|  |REL|ZIY|   |   |   |ZPX|ZPX|   |IMP|ABY|   |   |   |ABX|ABX|   |\r\n"\
       "|E0|CPX|SBC|   |   |CPX|SBC|INC|   |INX|SBC|NOP|   |CPX|SBC|INC|   |\r\n"\
       "|  |IMM|ZIX|   |   |ZPG|ZPG|ZPG|   |IMP|IMM|IMP|   |ABS|ABS|ABS|   |\r\n"\
       "|F0|BEQ|SBC|   |   |   |SBC|INC|   |SED|SBC|   |   |   |SBC|INC|   |\r\n"\
       "|  |REL|ZIY|   |   |   |ZPX|ZPX|   |IMP|ABY|   |   |   |ABX|ABX|   |\r\n"\
       "+--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+\r\n"
    self.assertEqual(t,r) 
    r= self.cmd.exec("man table ldx")
    self.assertIn("|LDX| AE|   | BE|   | A2|   |   |   |   |   | A6|   | B6|",r) 
    r= self.cmd.exec("man table ld?")
    self.assertIn("found 3 instructions",r) 
    r= self.cmd.exec("man table ??x")
    self.assertIn("found 7 instructions",r) 
    r= self.cmd.exec("man table b*x")
    self.assertIn("no matching instructions",r) 
    r= self.cmd.exec("man table")
    self.assertIn("found 56 instructions",r) 

  # The 'man reg' command lists registers
  def test_reg(self):
    r= self.cmd.exec("man reg")
    self.assertIn("A   -",r) 
    self.assertIn("X   -",r) 
    self.assertIn("Y   -",r) 
    self.assertIn("S   -",r) 
    self.assertIn("PCL -",r) 
    self.assertIn("PCH -",r) 
    self.assertIn("PSR -",r) 

##########################################################################
### read
##########################################################################

class Test_read(unittest.TestCase):
  def setUp(self):
    self.cmd= cmd.Cmd(port)
    
  def tearDown(self):
    self.cmd.close()
    self.cmd= None

  # Commands and arguments may be shortened.
  def test_shorten(self):
    self.cmd.exec("write 0200 11 22 33") # ensure we know what we read
    r= self.cmd.exec("r 0200 3") 
    self.assertIn("0200: 11 22 33\r\n",r) 

  # The long help gives details
  def test_longhelp(self):
    r= self.cmd.exec("help read")
    # Check if all sections are there
    self.assertIn("SYNTAX: read",r) 
    # Check notes
    self.assertIn("<addr> and <num> is 0000..FFFF, but physical memory is limited and mirrored",r) 

  # Erroneous args
  def test_errargs(self):
    r= self.cmd.exec("read foo")
    self.assertIn("ERROR: expected hex <addr>, not 'foo'\r\n",r) 
    r= self.cmd.exec("read 0200 bar")
    self.assertIn("ERROR: expected hex <num>, not 'bar'\r\n",r) 
    r= self.cmd.exec("read 0200 10 baz")
    self.assertIn("ERROR: too many arguments\r\n",r) 

  ## Test for main features ##############################################
  
  # The 'read' command has default addr and num
  def test_read(self):
    self.cmd.exec("write 0280 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF") 
    self.cmd.exec("write 0240 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF") 
    self.cmd.exec("write 0200 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF") 
    # addr from last write (default size)
    r= self.cmd.exec("read") 
    self.assertIn("0200: 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF\r\n",r) 
    self.assertIn("0220: ",r) 
    self.assertIn("0230: ",r) 
    self.assertNotIn("0240: ",r) 
    # addr from last read (default size)
    r= self.cmd.exec("read") 
    self.assertIn("0240: 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF\r\n",r) 
    self.assertIn("0250: ",r) 
    self.assertIn("0260: ",r) 
    self.assertIn("0270: ",r) 
    self.assertNotIn("0280: ",r) 
    # addr from last read (set size)
    r= self.cmd.exec("read - 2") 
    self.assertEqual("0280: 00 11\r\n",r) 
    r= self.cmd.exec("read - 2") 
    self.assertEqual("0282: 22 33\r\n",r) 
    # addr from last read (border case)
    r= self.cmd.exec("read - ") 
    self.assertIn("0284: 44 55 66",r) 
    self.assertIn("0294: ",r) 
    self.assertIn("02A4: ",r) 
    self.assertIn("02B4: ",r) 
    self.assertNotIn("02C4: ",r) 
    # addr from last read (explicit)
    r= self.cmd.exec("read 20A 04") 
    self.assertEqual("020A: AA BB CC DD\r\n",r) 
    r= self.cmd.exec("read - 01") 
    self.assertEqual("020E: EE\r\n",r) 
    # addr from last read (borde case)
    r= self.cmd.exec("read 240 00") 
    self.assertEqual("",r) 

  # The 'read' command also continues from assembler
  def test_sub(self):
    self.cmd.exec("write 0200 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF") 
    self.cmd.exec("asm 202 NOP")
    # addr from last asm
    r= self.cmd.exec("read - 2") 
    self.assertIn("0202: EA 33\r\n",r) 

##########################################################################
### write
##########################################################################

class Test_write(unittest.TestCase):
  def setUp(self):
    self.cmd= cmd.Cmd(port)
    
  def tearDown(self):
    self.cmd.close()
    self.cmd= None

  # Commands and arguments may be shortened.
  def test_shorten(self):
    # shorten 'write'
    self.cmd.exec("w 0200 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF")
    r= self.cmd.exec("r 0200 10") 
    self.assertEqual("0200: 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF\r\n",r) 
    # shorten 'read' inside `write`
    self.cmd.exec("w 20A r 202 03")
    r= self.cmd.exec("r 0200 10") 
    self.assertEqual("0200: 00 11 22 33 44 55 66 77 88 99 22 33 44 DD EE FF\r\n",r) 
    # shorten 'seq' inside `write`
    self.cmd.exec("w 202 s A5 03")
    r= self.cmd.exec("r 0200 10") 
    self.assertEqual("0200: 00 11 A5 A5 A5 55 66 77 88 99 22 33 44 DD EE FF\r\n",r) 

  # The long help gives details
  def test_longhelp(self):
    r= self.cmd.exec("help write")
    # Check if all sections are there
    self.assertIn("SYNTAX: write",r) 
    self.assertIn("NOTES:",r) 
    # Check notes
    self.assertIn("<addr> and <num> is 0000..FFFF, but physical memory is limited and mirrored",r) 

  # Erroneous args main
  def test_errargs_main(self):
    r= self.cmd.exec("write")
    self.assertEqual("ERROR: insufficient arguments, need <addr>\r\n",r) 
    r= self.cmd.exec("write foo")
    self.assertEqual("ERROR: expected hex <addr>, not 'foo'\r\n",r) 
    r= self.cmd.exec("write 200 222 33")
    self.assertEqual("ERROR: <data> must be 00..FF, not '222', rest ignored\r\n",r) 
    r= self.cmd.exec("write 200 foo")
    self.assertEqual("ERROR: <data> must be 00..FF, not 'foo', ignored\r\n",r) 

  # Erroneous args seq
  def test_errargs_seq(self):
    r= self.cmd.exec("write 200 seq")
    self.assertEqual("ERROR: seq must have <data>, ignored\r\n",r) 
    r= self.cmd.exec("write 200 seq foo")
    self.assertEqual("ERROR: seq must have <data> and <num>, ignored\r\n",r) 
    r= self.cmd.exec("write 200 seq foo bar")
    self.assertEqual("ERROR: seq <data> must be 00..FF, not 'foo', ignored\r\n",r) 
    r= self.cmd.exec("write 200 seq A5 bar")
    self.assertEqual("ERROR: seq <num> must be 0000..FFFF, not 'bar', ignored\r\n",r) 
    r= self.cmd.exec("write 200 seq 333 bar 22")
    self.assertEqual("ERROR: seq <data> must be 00..FF, not '333', rest ignored\r\n",r) 

  # Erroneous args read
  def test_errargs_read(self):
    r= self.cmd.exec("write 200 read")
    self.assertEqual("ERROR: read must have <addr>, ignored\r\n",r) 
    r= self.cmd.exec("write 200 read foo")
    self.assertEqual("ERROR: read must have <addr> and <num>, ignored\r\n",r) 
    r= self.cmd.exec("write 200 read foo bar")
    self.assertEqual("ERROR: read <addr> must be 0000..FFFF, not 'foo', ignored\r\n",r) 
    r= self.cmd.exec("write 200 read A5A5 bar")
    self.assertEqual("ERROR: read <num> must be 0000..FFFF, not 'bar', ignored\r\n",r) 
  
  ## Test for main features ##############################################
  
  # The write command writes bytes to memory (plain)
  def test_write_plain(self) :
    # write 16 bytes
    self.cmd.exec("write 0000 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF")
    r= self.cmd.exec("read 0000 10") 
    self.assertEqual("0000: 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF\r\n",r) 
    # write 8 bytes on border
    self.cmd.exec("write FFFC 18 19 1A 1B 1c 1d 1e 1f")
    r= self.cmd.exec("read FFFC 14") 
    self.assertEqual("FFFC: 18 19 1A 1B 1C 1D 1E 1F 44 55 66 77 88 99 AA BB\r\n000C: CC DD EE FF\r\n",r) 
    # write 1 byte
    self.cmd.exec("write 0005 25")
    r= self.cmd.exec("read 0004 3") 
    self.assertEqual("0004: 44 25 66\r\n",r) 

  # The write command writes bytes to memory (seq)
  def test_write_plain(self) :
    # write 16 bytes
    self.cmd.exec("write 0000 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF")
    r= self.cmd.exec("read 0000 10") 
    self.assertEqual("0000: 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF\r\n",r) 
    # seq 6 bytes on border
    self.cmd.exec("write FFFC 18 seq C0 6 1f")
    r= self.cmd.exec("read FFFC 10") 
    self.assertEqual("FFFC: 18 C0 C0 C0 C0 C0 C0 1F 44 55 66 77 88 99 AA BB\r\n",r) 
    # seq 1 byte
    self.cmd.exec("write 8 seq 00 1")
    r= self.cmd.exec("read 0007 3") 
    self.assertEqual("0007: 77 00 99\r\n",r) 
    # seq 0 byte
    self.cmd.exec("write 00A seq ee 0")
    r= self.cmd.exec("read 0009 3") 
    self.assertEqual("0009: 99 AA BB\r\n",r) 

  # The write command writes bytes to memory (read)
  def test_write_plain(self) :
    # read 4 bytes forwards
    self.cmd.exec("write 0000 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF")
    self.cmd.exec("write 0002 12 read 005 4 17")
    r= self.cmd.exec("read 0000 10") 
    self.assertEqual("0000: 00 11 12 55 66 77 88 17 88 99 AA BB CC DD EE FF\r\n",r) 
    # read 4 bytes backwards
    self.cmd.exec("write 0000 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF")
    self.cmd.exec("write 0008 18 read 006 4 1d") # after write 18, we have ... 55 66 77 18 99 AA ... so we read 66 77 18 99
    r= self.cmd.exec("read 0000 10") 
    self.assertEqual("0000: 00 11 22 33 44 55 66 77 18 66 77 18 99 1D EE FF\r\n",r) 
    # read 1 byte
    self.cmd.exec("write 2 read 1 1")
    r= self.cmd.exec("read 0000 4") 
    self.assertEqual("0000: 00 11 11 33\r\n",r) 
    # read 0 byte
    self.cmd.exec("write 5 read 1 0")
    r= self.cmd.exec("read 0004 3") 
    self.assertEqual("0004: 44 55 66\r\n",r) 

  # The write command writes bytes to memory (stream)
  def test_write_stream(self) :
    # write streaming mode
    self.cmd.exec("write 0600 seq 5A 10")
    r= self.cmd.exec("write 0600", "> ")
    self.assertEqual("W:0600",r) 
    r= self.cmd.exec("22", "> ")
    self.assertEqual("W:0601",r) 
    r= self.cmd.exec("222", "W:0601> ") # hack: error text has "<data> must", so contains "> "
    self.assertEqual("ERROR: <data> must be 00..FF, not '222', ignored\r\n",r) 
    r= self.cmd.exec("33 44 55", "> ")
    self.assertEqual("W:0604",r) 
    r= self.cmd.exec("")
    self.assertEqual("",r) 
    r= self.cmd.exec("read 0600 07") 
    self.assertEqual("0600: 22 33 44 55 5A 5A 5A\r\n",r) 

# ##########################################################################
# ### Xxx
# ##########################################################################
# 
# class Test_help(unittest.TestCase):
#   def setUp(self):
#     self.cmd= cmd.Cmd(port)
#     
#   def tearDown(self):
#     self.cmd.close()
#     self.cmd= None
# 
#   # Commands and arguments may be shortened.
#   def test_shorten(self):
#     r= self.cmd.exec("xxx y") # y is shorthand for yyy
#     self.assertIn("...xxx...yyy...",r) 
# 
#   # The long help gives details
#   def test_longhelp(self):
#     r= self.cmd.exec("help xxx")
#     # Check if all sections are there
#     self.assertIn("SYNTAX: xxx",r) 
#     self.assertIn("SYNTAX: xxx <cmd>",r) 
#     self.assertIn("NOTES:",r) 
#     # Check notes
#     self.assertIn(">>",r) 
#     self.assertIn("//",r) 
#     self.assertIn("@",r) 
# 
#   # Erroneous args
#   def test_errargs(self):
#     r= self.cmd.exec("xxx OOPS")
#     self.assertIn("ERROR: command not found (try 'help')\r\n",r) 
#     r= self.cmd.exec("xxx FOO BAR OOPS")
#     self.assertIn("ERROR: too many arguments\r\n",r) 
# 
#   ## Test for main features ##############################################
#   
#   # The ....
#   def test_list(self):
#     r= self.cmd.exec("xxx")
#     self.assertIn("...xxx...",r) 
# 
#   # The sub command ...
#   def test_sub(self):
#     r= self.cmd.exec("xxx yyy")
#     self.assertIn("...yyy...",r) 

if __name__ == '__main__':
  unittest.main()
