Emulator of i4004 processor and Busicom calculator 141-PF (with original ROMs).
I also attached a file with the Pi number calculation program (16 signs).

Keys for command string:
4004_emulator.exe -f BusiCom.txt -ru -list -step -log
 -f <filename>   - txt file with program
 -ru             - russian localization
 -list           - list program before run
 -step           - step by step execution (<Space> to run next command, press <P> to disable/enable it, <TAB> to list all registers)
 -log            - log commands to console


Program file format : <command in decimal> # <comment>
Example:
>1  64 # jump to start
>2  0
>3  209 # ld 1


Attached programs:
BusiCom.txt - original ROMs from Busicom calculator
Prog_PI.txt - calculating of PI
Memtest.txt - fill RAM with numbers
