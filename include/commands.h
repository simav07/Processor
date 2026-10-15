#ifndef COMMANDS
#define COMMANDS

//! Commands for virtual machine
enum COMMANDS {
    PUSH_C    = 1,
    ADD_C     = 2,
    SUB_C     = 3,
    DIV_C     = 4,
    MUL_C     = 5,
    PUSH_R    = 6,
    POP_R     = 7,
    SKIP_STEP = 8,
    JUMP_C    = 9,
    JA_C      = 10,
    JAE_C     = 11,
    JB_C      = 12,
    JBE_C     = 13,
    JE_C      = 14,
    JNE_C     = 15,
    IN_C      = 16,
    CALL_C    = 17,
    RET_C     = 18,
    OUT_C,
    HLT_C,
};

#endif