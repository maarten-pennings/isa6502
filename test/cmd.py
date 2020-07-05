# cmd - Driver for the cmd library

# INSTALL
#   - run 'pip freeze' to confirm that pyserial is installed
#   - if not, run 'python -m pip install pyserial'
#   - in case of errors, you might want to uninstall _both_ serial and pyserial, and reinstall pyserial.
#     'python -m pip uninstall pyserial' and 'python -m pip uninstall serial'

import re
import sys
import glob
import serial
import datetime


class CmdException(Exception):
    def __init__(self,*args,**kwargs):
        Exception.__init__(self,*args,**kwargs)


# def hexx(v,b=0):
#     """Same as hex() but formats v as b bytes, hexx(0x1F,2)='0x001f'."""
#     h = hex(v)
#     if len(h)%2==1: h="0x0"+h[2:]
#     missing= 2*b-(len(h)-2)
#     if missing>0: h="0x"+("0"*missing)+h[2:]
#     return h
# 
#     
# def hexs2int_be(hexs):
#     """Converts a hexs string (eg '12 34') to int (0x1234) assuming big endian."""
#     i = 0
#     for h in hexs.split():
#         i= i*256 + int(h,16)
#     return i
# 
#     
# def hexs2int_le(hexs):
#     """Converts a hexs string (eg '12 34') to int (0x3412) assuming little endian."""
#     i,p = 0,1
#     for h in hexs.split():
#         i= i + p*int(h,16)
#         p= p*256
#     return i
# 
# 
# def int2hexs_be(v,b):
#     """Converts an integer v (eg 0x1234) of 'b' bytes (eg 3) to a hexs string ('00 12 34') assuming big endian."""
#     s= hexx(v,b)
#     h= ""
#     for i in range(2,len(s),2) :
#         h= h+s[i:i+2]+' '
#     return h[:-1]
#     
# 
# def int2hexs_le(v,b):
#     """Converts an integer v (eg 0x1234) of 'b' bytes (eg 3) to a hexs string ('34 12 00') assuming little endian."""
#     s= hexx(v,b)
#     h= ""
#     for i in range(2,len(s),2) :
#         h= s[i:i+2]+' '+h
#     return h[:-1]

    
class Cmd:
    """A driver for the cmd library, offering the low-level exec() command command."""
    def __init__(self, port=None):
        """Creates a serial port object. If port is passed, open() is called."""
        self.serial= None
        self.logfile= None
        if not port is None: self.open(port)
    def logstart(self,filename="__cmd.log",filemode="w",msg=None):
        """Starts logging to file 'filename' (for append) of all commands and responses to and from the command interpreter."""
        self.logfile= open(filename,filemode)
        self.log('log start '+(msg if msg else filename))
    def logstop(self):
        if self.logfile!=None:
            """Stops logging."""
            self.log('Log stop')
            self.logfile.close()
            self.logfile= None
    def log(self,msg,tag="!"):
        """Add message 'msg' to the log, tagging the message with character 'tag'."""
        # Prepends time stamp and tag: '2016-12-08 13:40:20.879844 ! '
        indent= f"                           {tag} "
        # Make CR/LF visible by rendering them as \r and \n
        msg= msg.replace("\r\n","\\r\\n«")
        msg= msg.replace("\n","\\n«")
        msg= msg.replace("\r","\\r«")
        msg= msg.replace("«","\n"+indent)
        if msg.endswith(indent): msg= msg[0:len(msg)-len(indent)-1] # remove last indent if it is just the indent
        self.logfile.write("{} {} {}\n".format(datetime.datetime.today(),tag,msg))    
    def open(self,port):
        """Opens serial port 'port' towards the command interpreter."""
        if self.logfile!=None: self.log('open port '+port)
        if self.serial!=None: raise CmdException("open(): COM port already open - Tip: either call e=Xxx();e.open(\"COM4\") or e=Xxx(\"COM4\").")
        self.serial= serial.Serial(port=port,baudrate=115200,timeout=0.01)
        self.rxbuf=b"" # Clear the receive buffer.
        res=self.exec("") # Clear any pending command (or startup banner).
        # Explicitly sync.
        res=self.exec("echo disable","echo: disabled\r\n>> ")
    def close(self, nice=True):
        """Closes the serial port. When 'nice', first re-enables echoing."""
        if nice : res=self.exec("echo enable\n") # Switch echo back on
        if self.logfile!=None: 
            self.log("close port")
        self.serial.close()
        self.serial= None
        if self.logfile!=None: 
            self.log("log stop")
            self.logfile.write("\n")
            self.logfile.close()
    def isopen(self):
        """Returns true if the serial port to the dongle is open."""
        return not self.serial is None
    def exec(self,icmd,isync=">> "):
        """Send command 'icmd' to the command interpreter; waiting for dongle response to have 'isync'. Returns response up to 'isync'."""
        cmd= icmd.encode(); sync= isync.encode() # Convert strings to bytes
        self.serial.write(cmd+b"\r") # Send bytes with <CR> to command interpreter
        if self.logfile!=None: self.log(icmd+"\r",">") # Log the command that was send
        pos,tries = -1,0
        while (pos<0) and (tries<100):
            self.rxbuf+= self.serial.read(1000)
            pos= self.rxbuf.find(sync)
            tries= tries+1
        if pos<0 : 
            raise CmdException("exec(): sync not received ["+self.rxbuf.decode()+"]")
        if self.logfile!=None: self.log(self.rxbuf[:pos+len(sync)].decode(),"<")
        res=self.rxbuf[:pos]
        self.rxbuf=self.rxbuf[pos+len(sync):]
        return res.decode() # Convert bytes to strings


def findports():
    """Returns an array of the serial ports that have a command interpreter."""
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(255)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # this excludes your current terminal "/dev/tty"
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Unsupported platform')
    result = []
    for port in ports:
        try:
            cmd= Cmd()
            cmd.open(port)
            cmd.close(True)
            result.append(port)
        except (OSError, serial.SerialException, CmdException) as error:
            # print(error)
            pass
    return result


def TestCmd():
    print("--- scanning ---")
    print("scanning... (wait)")
    ports= findports()
    print(ports)
    if len(ports)>0:
        port=ports[0]
        print("--- opening ---")
        print(port)
        d=Cmd()
        d.open(port)
        print(d.exec("echo Hello, World!"),end="")
        print(d.exec("help"),end="")
        print(d.exec("echo Switching logging on"),end="")
        d.logstart()
        print(d.exec("echo Logging is on"),end="")
        d.exec("help")
        d.exec("man lda")
        d.log("Manual entry\nDoes that\rWork?")
        d.logstop()
        print("--- closing ---")
        d.close()
    print("--- end ---")


if __name__ == "__main__":
    print("NOTE: a test is now running; this script is not intended to be run alone.")
    TestCmd()
        
