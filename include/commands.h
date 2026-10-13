#ifndef COMMANDS
#define COMMANDS

//! Commands for virtual machine
enum COMMANDS {
    PUSH_C = 1,
    ADD_C  = 2,
    SUB_C  = 3,
    DIV_C  = 4,
    MUL_C  = 5,
    PUSH_R = 6,
    POP_R  = 7,
    SKIP_STEP = 8,
    JUMP_C = 9,
    JA_C = 10,
    OUT_C = 11,
    HLT_C = 12,
};

#endif