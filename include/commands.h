#ifndef COMMANDS
#define COMMANDS

#define CODE_MASKS uint8_t constMask = 0x20; \
                   uint8_t regMask   = 0x40; \
                   uint8_t ramMask   = 0x80  \

//! Commands for virtual machine
enum COMMANDS {
    PUSH_C    = 1,
    ADD_C     = 2,
    SUB_C     = 3,
    DIV_C     = 4,
    MUL_C     = 5,
    PUSH_R    = 6,
    POP_C     = 7,  // POP_R
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