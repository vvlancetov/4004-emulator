# Emulator of Intel 4004 processor and Busicom calculator 141-PF (with original ROMs)

The emulator is designed for educational purposes. The original firmware from the Busicom 141-PF (attached) is used. I also attached a file with the Pi number calculation program (16 signs), the result of the calculation wiil be placed in the RAM registers.

ROM file format (plain text) : (command in decimal) # (comment)

Example:
```
64 # jump to start
0
209 # ld 1
```

It is better to start the emulator from IDE to immediately specify the necessary parameters.

Operand will not be displayed into the console immediately (like the original calculator). The operand will be displayed after pressing the operator’s key ( +, -, etc.) or the Enter key.

Keys for command string:
```
4004_emulator.exe -f BusiCom.txt -ru -list -step -log

 -f <filename>   - txt file with program
 
 -ru             - russian localization
 
 -list           - list program before run
 
 -step           - step by step execution (<Space> to run next command, press <P> to disable/enable it, <TAB> to list all registers)
 
 -log            - log commands to console
```
 
Attached programs:

BusiCom.txt - original ROMs from Busicom calculator

Prog_PI.txt - calculating of PI

Memtest.txt - fill RAM with numbers

