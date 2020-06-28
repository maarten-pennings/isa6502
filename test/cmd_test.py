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
  print(f"ERROR: no command interpreter on port {port}")
  pass


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


if __name__ == '__main__':
  unittest.main()
