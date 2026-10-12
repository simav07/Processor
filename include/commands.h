#ifndef COMMANDS
#define COMMANDS

//! Commands for virtual machine
enum COMMANDS {
    PUSH_C = 1,
    ADD_C  = 2,
    SUB_C  = 3,
    DIV_C  = 4,
    MUL_C  = 5,
    PUSH_R,
    POP_R,
    OUT_C,
    HLT_C,
};

#endif