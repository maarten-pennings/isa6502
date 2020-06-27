# This file contains the test cases for the isa6502prog example

import cmd
import time
import unittest

class cmd_test(unittest.TestCase):
    def setUp(self):
        self.cmd= cmd.Cmd("COM32")
        
    def tearDown(self):
        self.cmd.close()
        self.cmd= None

    ##########################################################################
    ### cmd iteself
    ##########################################################################
    
    # Each character received by the command interpreter is appended to its input buffer.
    def test_cmd_buffered(self):
        self.cmd.serial.write(b"he") # directly send some chars (should be buffered)
        r= self.cmd.exec("lp")
        self.assertIn("Available commands",r)

    # The command interpreter echos each received character, so local echo is not needed.
    def test_cmd_echo(self):
        r= self.cmd.exec("echo enable")
        self.assertEqual(r, "echo: enabled")
        r= self.cmd.exec("echo disable")
        self.assertEqual(r, "echo disable\r\necho: disabled") # echo was on, so we get our command back

    # FIX
    # When the input buffer is full or an illegal char is received BEL is echo’ed.
    #def test_cmd_illchar(self):
    #    r= self.cmd.exec("\x11") # illegal char
    #    self.assertEqual(r, "\x07") # bell is \a

    # Backspace deletes the last character from the input buffer (echo’s BS SPC BS).
    def test_cmd_bs(self):
        self.cmd.exec("echo enable")
        r= self.cmd.exec("#A\b")
        self.assertIn("#A\b \b",r) # A is erased with BS SPC BS
        self.cmd.exec("echo disable")

    # All characters from // till end-of-line are considered comment.
    def test_cmd_comment(self):
        r= self.cmd.exec("help help xxx")
        self.assertEqual(r,"ERROR: too many arguments")
        r= self.cmd.exec("help help // xxx",">> ") # FIX \r\n
        self.assertIn("SYNTAX: help",r)

    # Commands and arguments may be shortened (eg help, hel, he, and h, all work, and so does h h as a shorthand for help help).
    def test_cmd_shorten(self):
        r= self.cmd.exec("help",">> ") # FIX \r\n
        self.assertIn("help -",r) 
        r= self.cmd.exec("hel",">> ") # FIX \r\n
        self.assertIn("help -",r) 
        r= self.cmd.exec("he",">> ") # FIX \r\n
        self.assertIn("help -",r) 
        r= self.cmd.exec("h",">> ") # FIX \r\n
        self.assertIn("help -",r) 
        r= self.cmd.exec("help help",">> ") # FIX \r\n
        self.assertIn("SYNTAX: help",r)
        r= self.cmd.exec("help hel",">> ") # FIX \r\n
        self.assertIn("SYNTAX: help",r)
        r= self.cmd.exec("help he",">> ") # FIX \r\n
        self.assertIn("SYNTAX: help",r)
        r= self.cmd.exec("help h",">> ") # FIX \r\n

    # FIX: when buffer full, backspace is not accepted
    # The input buffer size is 128 characters (max number of characters until end-of-line).
    def test_cmd_buf1024(self):
        # First check that 127 chars fit
        self.cmd.serial.write(b"echo ")
        for line in range(0,(128-5-1)//16): 
            self.cmd.serial.write(b"0123456789abcdef")
            time.sleep(0.1) # Prevent "characters lost"
        r= self.cmd.exec("0123456PQR")
        self.assertNotIn("\b",r) # no BEL is send back
        self.assertIn("PQR",r)
        # Now check that char nr 128 does not fit
        self.cmd.serial.write(b"echo ")
        for line in range(0,(128-5-1)//16): 
            self.cmd.serial.write(b"0123456789abcdef")
            time.sleep(0.1) # Prevent "characters lost"
        r= self.cmd.exec("0123456pqrs")
        self.assertIn("pqr",r)
        self.assertNotIn("pqrs",r)

    # The empty string is a valid command.
    def test_cmd_empty(self):
        r= self.cmd.exec("",">> ") # Empty command has empty reply, so no output ending in \r\n
        self.assertEqual(r, "") 

    # When a command name is not in the list of supported commands, an error is given    
    def test_cmd_unknown(self):
        r= self.cmd.exec("foo")
        self.assertEqual(r, "ERROR: command not found (try help)" )

    ##########################################################################
    ### Help
    ##########################################################################

    # Check if all commands are there
    def test_help(self):
        r= self.cmd.exec("help")
        self.assertIn("asm -",r) 
        self.assertIn("dasm -",r) 
        self.assertIn("echo -",r) 
        self.assertIn("help -",r) 
        self.assertIn("man -",r) 
        self.assertIn("read -",r) 
        self.assertIn("write -",r) 
        # self.assertIn("prog -",r) FIX

    # help help
    def test_help_help(self):
        r= self.cmd.exec("help help",">> ") # FIX
        # Check if all sections are there
        self.assertIn("SYNTAX: help",r) 
        self.assertIn("SYNTAX: help <cmd>",r) 
        self.assertIn("NOTES:",r) 
        # Check notes
        self.assertIn("shortened",r) 
        self.assertIn(">>",r) 
        self.assertIn("//",r) 
        self.assertIn("@",r) 

    # superfluous args
    def test_help_supargs(self):
        r= self.cmd.exec("help OOPS")
        self.assertIn("ERROR",r) 
        r= self.cmd.exec("help help OOPS")
        self.assertIn("ERROR",r) 

    ##########################################################################
    ### echo
    ##########################################################################

    # enable/disable and status
    def test_echo_enabledisable(self):
        r= self.cmd.exec("echo")
        self.assertEqual("echo: disabled",r) 
        r= self.cmd.exec("echo enable")
        self.assertEqual("echo: enabled",r) 
        r= self.cmd.exec("echo disable")
        self.assertEqual("echo disable\r\necho: disabled",r) 
        # short-hands
        r= self.cmd.exec("e en") # e = errors
        self.assertEqual("echo: enabled",r) 
        r= self.cmd.exec("e d")
        self.assertEqual("e d\r\necho: disabled",r) 
        # no output
        r= self.cmd.exec("@e enable", ">> ") # empty reply
        self.assertEqual("",r) 
        r= self.cmd.exec("@e d", ">> ") # empty reply
        self.assertEqual("@e d\r\n",r) 

    # errors subcommand
    def test_echo_errors(self):
        r= self.cmd.exec("echo errors") # clear counter
        r= self.cmd.exec("echo errors")
        self.assertEqual("echo: errors: 0",r) 
        r= self.cmd.exec("@echo errors", ">> ") # empty reply
        self.assertEqual("",r) 

    # line subcommand
    def test_echo_line(self):
        r= self.cmd.exec("echo Hello, world!")
        self.assertIn("Hello, world!",r) 
        r= self.cmd.exec("echo line enable")
        self.assertEqual("enable",r) 
        r= self.cmd.exec("echo line")
        self.assertEqual("",r) 

    # help
    def test_echo_help(self):
        r= self.cmd.exec("help echo",">> ") # FIX
        # Check if all sections are there
        self.assertIn("SYNTAX: echo [line] <word>...",r) 
        self.assertIn("SYNTAX: [@]echo errors",r) 
        self.assertIn("SYNTAX: [@]echo [ enable | disable ]",r) 
        self.assertIn("NOTES:",r) 

if __name__ == '__main__':
    unittest.main()
