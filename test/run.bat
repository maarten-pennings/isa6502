@ECHO off
IF "(env) " neq "%PROMPT:~0,6%" ECHO Please run setup.bat first && EXIT /b

REM python -m unittest cmd_test


python  cmd_test.py  Test_cmd  Test_help  Test_echo  Test_man  Test_read  Test_write  Test_dasm  Test_asm
rem python  cmd_test.py  Test_prog




