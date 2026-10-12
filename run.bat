@echo off
g++ genhash.cpp io.cpp -I.\include -DMYDEBUG -o run_hash
.\run_hash
g++ assembler.cpp io.cpp  -I.\include -DMYDEBUG -DLOG_MODE -o run_asm
.\run_asm --asmsource ucode.asm
g++ processor.cpp stack.cpp io.cpp -I.\include  -DMYDEBUG -DLOG_MODE -DCANARY -o run_vm
.\run_vm --bytecode bcode.bin