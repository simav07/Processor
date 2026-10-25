typedef double ramElem_t;
typedef double cmdRes_t;

#include "include/stack.h"
#include "include/io.h"
#include "include/commands.h"
#include "hashcommands.h"
#include <windows.h>

const size_t nRegisters = 5;

/**
 * Struct for virtual machine that includes stack for all variables,
 * buffer with bytecode, current pc, pc status (changed / not changed),
 * another stack for collect all info about function's calls, and correct returning.
 * 5 registers for quick working with memory.
 * And its own RAM, so we can draw memory contents into the console
 */

struct vmResource {

    stack_t stack = {};

    FILE * fileDump = NULL;
    
    uint8_t * byteCode = NULL;
    size_t byteCodeLen = 0;
    
    size_t pc = 0;
    bool pcChanged = false;

    stack_t funcStack = {};

    //! Registers
    stackElem_t regstr[nRegisters] = {};

    ramElem_t * RAM = NULL;
};

//! Size of memory in bytes
const size_t ramSize = 7000;

/**
 * Information about frame for drawing in the console
 * picLength - horizontal size in elements,
 * picWidth  - vertical high in elements
 */
const size_t picLength = 100;
const size_t picWidth  = 50;

//! Flag for byte code filename
const char * const CMD_FLAG_BYTECODE = "--bytecode";

//! Special ram cell for bad apple video
const size_t RAM_CELL_BAD_APPLE = 6999;

//! #define VMDump for debug

const char FILE_STACKDUMP[] = "dump.txt";

void VmDump(FILE * stream, vmResource * virtualMachine);

void DrawRam(const vmResource * virtualMachine);

err_t VmCtor(vmResource * virtualMachine, char * byteCodeFile);
err_t PrepareByteCode(vmResource * virtualMachine, char * byteCodeFile);
void VmDtor(vmResource * virtualMachine);

err_t ArgvParser(const int argc, const char * const argv[], char ** readFromFile);

//! Main virtual machine
err_t RunVirtualMachine(vmResource * virtualMachine);

//! Fill byte code buffer from txt
size_t GetByteCode(uint8_t ** byteCode, const char * byteCodeFilename);

size_t  GetInstructionSize(vmResource * virtualMachine);
int32_t GetInstructionArgument(vmResource * virtualMachine);


//! Reading command code: analysis of flags, args
err_t GetCommand(vmResource * virtualMachine, size_t * instrSize, uint8_t * command, stackElem_t ** result);
//! Run commands
err_t RunCommand(vmResource * virtualMachine, uint8_t command, stackElem_t * result);

//! Commands for virtual machine -------------------------------------

//! Get the last two elemets from stack
err_t GetTwoPop(stack_t * stack, stackElem_t * a, stackElem_t * b);

err_t Pop(vmResource * virtualMachine, stackElem_t * result);
err_t Push(vmResource * virtualMachine, stackElem_t * result);

err_t Jump(vmResource * virtualMachine, size_t jumpIndex);
err_t JA(vmResource * virtualMachine);
err_t JAE(vmResource * virtualMachine);
err_t JB(vmResource * virtualMachine);
err_t JBE(vmResource * virtualMachine);
err_t JE(vmResource * virtualMachine);
err_t JNE(vmResource * virtualMachine);

//! Input from the keyboard
err_t In(vmResource * virtualMachine);

//! Arithmetical operations
err_t AddCom(vmResource * virtualMachine);
err_t SubCom(vmResource * virtualMachine);
err_t DivCom(vmResource * virtualMachine);
err_t MulCom(vmResource * virtualMachine);
err_t Sqrt(vmResource * virtualMachine);

//! Call a function by name written after ":"
err_t Call(vmResource * virtualMachine);
//! Return to the last call's pc
err_t Ret(vmResource * virtualMachine);

//! Print the result of some operations. Get the last elem from stack
err_t OutCom(vmResource * virtualMachine);
// -------------------------------------------------------------------

int main(int argc, const char * const * argv) {

    vmResource virtualMachine = {};
    
    char * byteCodeFile = NULL;

    int filenamesStat = ArgvParser(argc, argv, &byteCodeFile);
    if (filenamesStat != ERR_OK) {
        printf("Error of reading bytecode filename\n");
        return ERR_ASM;
    }

    err_t ctorStat = VmCtor(&virtualMachine, byteCodeFile);
    if (ctorStat != ERR_OK) {
        printf("ERROR OF MAKING BYTE CODE\n");
        return false;
    }

    err_t processingStat = RunVirtualMachine(&virtualMachine);

    if (processingStat != ERR_OK) {
        printf("ERROR OF VIRTUAL MACHINE\n");
        return false;
    }

    // DrawRam(&virtualMachine);

    VmDtor(&virtualMachine);

    return 0;
}

err_t VmCtor(vmResource * virtualMachine, char * byteCodeFile) {

    ASSERT(byteCodeFile);
    ASSERT(virtualMachine);

    virtualMachine->pc = 0;
 
    err_t preparingStat = PrepareByteCode(virtualMachine, byteCodeFile);

    if (preparingStat != ERR_OK) return ERR_VM_CTOR;

    virtualMachine->pc = 0;

    stackElem_t * newMemPtr = (stackElem_t *)calloc(ramSize, sizeof(stackElem_t));
    if (!newMemPtr) return ERR_CTOR;
    virtualMachine->RAM = newMemPtr;
    for (size_t i = 0; i < ramSize; i++) {
        virtualMachine->RAM[i] = 0;
    }

    int ctorStat1 = StackCtor(&(virtualMachine->stack), 10);
    if (ctorStat1 != ERR_OK) {
        free(virtualMachine->RAM);
        return ERR_VM_CTOR;
    }
    int ctorStat2 = StackCtor(&(virtualMachine->funcStack), 10);
    if (ctorStat2 != ERR_OK) {
        free(virtualMachine->RAM);
        StackDtor(&virtualMachine->stack);
        return ERR_VM_CTOR;
    }
    virtualMachine->fileDump = fopen(FILE_STACKDUMP, "w");
    if (!virtualMachine->fileDump) return ERR_FILEOPEN;

    return ERR_OK;

}

void VmDtor(vmResource * virtualMachine) {

    fclose(virtualMachine->fileDump);

    StackDtor(&(virtualMachine->funcStack));
    StackDtor(&(virtualMachine->stack));

}

err_t PrepareByteCode(vmResource * virtualMachine, char * byteCodeFile) {

    ASSERT(virtualMachine);
    ASSERT(byteCodeFile);

    size_t byteCodeLen = GetByteCode(&(virtualMachine->byteCode), byteCodeFile);

    if (byteCodeLen == ERR_BYTECODE) {
        printf(RED "ERR_BYTECODE\n" RESET);
        return ERR_BYTECODE;
    }

    virtualMachine->byteCodeLen = byteCodeLen;

    return ERR_OK;

}

err_t RunVirtualMachine(vmResource * virtualMachine) {

    ASSERT(virtualMachine);
    ASSERT(virtualMachine->byteCode);

    while (virtualMachine->pc < virtualMachine->byteCodeLen) {

        uint8_t command = virtualMachine->byteCode[virtualMachine->pc] & 0x1F;

        cmdRes_t resultValue = 0;
        cmdRes_t * result = &resultValue;

        if (command == HLT_C) return ERR_OK;

        virtualMachine->pcChanged = false;
        size_t instrSize = 0;

        err_t readingStat = GetCommand(virtualMachine, &instrSize, &command, &result);
        if (readingStat != ERR_OK) return readingStat;

        err_t runStat = RunCommand(virtualMachine, command, result);
        if (runStat != ERR_OK) return ERR_PROCESSING;

        #ifdef VMDump
        VmDump(stdout, virtualMachine);
        getchar();
        #endif

        if (!virtualMachine->pcChanged) virtualMachine->pc += instrSize;
    }

    printf(BOLD_RED "In asm file no HLT command\n" RESET);
    return ERR_PROCESSING;
}

err_t GetCommand(vmResource * virtualMachine, size_t * instrSize,
                                               uint8_t * command, stackElem_t ** result) {

    ASSERT(virtualMachine);

    *instrSize = GetInstructionSize(virtualMachine);

    cmdRes_t currResult = 0;

    uint8_t cmdFlags = virtualMachine->byteCode[virtualMachine->pc];
    
    size_t argIndex = virtualMachine->pc + 1;
    int32_t value = 0;

    if (cmdFlags & constMask) {
        if (argIndex + sizeof(int32_t) > virtualMachine->byteCodeLen) return ERR_PROCESSING;

        memcpy(&value, virtualMachine->byteCode + argIndex, sizeof(value));

        currResult += value;
        argIndex += sizeof(int32_t);
    }

    if (cmdFlags & regMask) {
        
        int32_t regIndex = 0;

        if (argIndex + sizeof(int32_t) > virtualMachine->byteCodeLen) return ERR_PROCESSING;

        memcpy(&regIndex, virtualMachine->byteCode + argIndex, sizeof(regIndex));
        
        if (regIndex < 0 || regIndex >= nRegisters) return ERR_COMMAND;
        
        currResult += virtualMachine->regstr[regIndex];

        argIndex += sizeof(int32_t);

        if (!(cmdFlags & ramMask)) {
            *result = &virtualMachine->regstr[regIndex];
            return ERR_OK;
        }
    }

    if (cmdFlags & ramMask) {
        if (currResult < 0 || currResult >= ramSize) return ERR_COMMAND;

        *result = &virtualMachine->RAM[(size_t)currResult];

        return ERR_OK;
    }

    **result = currResult;

    return ERR_OK;

}

stackElem_t * GetArgResult(vmResource * virtualMachine, uint8_t cmdFlags,
                           stackElem_t result) {

    ASSERT(virtualMachine);
    ASSERT(virtualMachine->RAM);

    return &virtualMachine->RAM[(size_t)result];

}

err_t RunCommand(vmResource * virtualMachine, uint8_t command, stackElem_t * result) {

    ASSERT(virtualMachine);

    if (virtualMachine->pc >= virtualMachine->byteCodeLen) return ERR_PROCESSING;

    uint8_t flags = virtualMachine->byteCode[virtualMachine->pc];

    switch (command) {

        case PUSH_C: return Push   (virtualMachine, result);
        case POP_C:  return Pop    (virtualMachine, result);

        // TODO to align
        case JUMP_C: return Jump   (virtualMachine, (size_t)GetInstructionArgument(virtualMachine));
        case JA_C:   return JA     (virtualMachine);
        case JAE_C:  return JAE    (virtualMachine);
        case JB_C:   return JB     (virtualMachine);
        case JBE_C:  return JBE    (virtualMachine);
        case JE_C:   return JE     (virtualMachine);
        case JNE_C:  return JNE    (virtualMachine);
        case IN_C:   return In     (virtualMachine);
        case RET_C:  return Ret    (virtualMachine);
        case CALL_C: return Call   (virtualMachine);
        case ADD_C:  return AddCom (virtualMachine);
        case SUB_C:  return SubCom (virtualMachine);
        case DIV_C:  return DivCom (virtualMachine);
        case MUL_C:  return MulCom (virtualMachine);
        case SQRT_C: return Sqrt   (virtualMachine);
        case OUT_C:  return OutCom (virtualMachine);
        case HLT_C:  return ERR_OK;

        default:     return ERR_COMMAND;
    }

}

//------------------------------------------------------------------
// Functions for processor
//------------------------------------------------------------------

#define JUMP_FUNCTION(JMP_NAME, OPER) \
    err_t JMP_NAME (vmResource * virtualMachine) {\
        \
        ASSERT(virtualMachine);                                          \
                                                                         \
        err_t popStat1 = ERR_OK;                                         \
        err_t popStat2 = ERR_OK;                                         \
                                                                         \
        stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);   \
        if (popStat1 != ERR_OK) return ERR_COMMAND;                      \
                                                                         \
        stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);   \
        if (popStat2 != ERR_OK) return ERR_COMMAND;                      \
                                                                         \
        if (a OPER b) return Jump(virtualMachine,                        \
                        (size_t)GetInstructionArgument(virtualMachine)); \
                                                                         \
        return ERR_OK;                                                   \
    }

JUMP_FUNCTION(JA, >)

JUMP_FUNCTION(JAE, >=)

JUMP_FUNCTION(JB, <)

JUMP_FUNCTION(JBE, <=)

JUMP_FUNCTION(JE, ==)

JUMP_FUNCTION(JNE, !=)

#define ARITHMETIC_FUNCTION(OPER_NAME, OPER) \
    err_t OPER_NAME (vmResource * virtualMachine) {                                 \
                                                                                    \
        ASSERT(virtualMachine);                                                     \
                                                                                    \
        stackElem_t a = 0;                                                          \
        stackElem_t b = 0;                                                          \
                                                                                    \
        if (GetTwoPop(&(virtualMachine->stack), &a, &b) != ERR_OK) {                \
            return ERR_COMMAND;                                                     \
        }                                                                           \
                                                                                    \
        int pushStat = StackPush(&(virtualMachine->stack), (stackElem_t)(b OPER a));\
                                                                                    \
        if (pushStat != ERR_OK) return ERR_COMMAND;                                 \
                                                                                    \
        return ERR_OK;                                                              \
                                                                                    \
    }

ARITHMETIC_FUNCTION(AddCom, +)

ARITHMETIC_FUNCTION(SubCom, -)

ARITHMETIC_FUNCTION(DivCom, /)

ARITHMETIC_FUNCTION(MulCom, *)

// Get 2 last elems from stack
err_t GetTwoPop(stack_t * stack, stackElem_t * a, stackElem_t * b) {

    ASSERT(stack);
    ASSERT(a);
    ASSERT(b);

    err_t popStat = ERR_OK;

    *a = StackPop(stack, &popStat);
    if (popStat != ERR_OK) return ERR_COMMAND;

    *b = StackPop(stack, &popStat);
    if (popStat != ERR_OK) return ERR_COMMAND;

    return ERR_OK;
}

err_t Pop(vmResource * virtualMachine, stackElem_t * result) {

    ASSERT(virtualMachine);
    ASSERT(result);
        
    err_t popStat = ERR_OK;

    popStat = ERR_OK;
    stackElem_t var = StackPop(&(virtualMachine->stack), &popStat);
    
    if (popStat != ERR_OK) return ERR_COMMAND;
    
    *result = var;

    //! Special memory cell using as signal to draw a shot
    if (*result == RAM_CELL_BAD_APPLE) {
            DrawRam(virtualMachine); 
            Sleep(33);  // 1000 / 30 fps
    }

    return ERR_OK;
}

err_t Push(vmResource * virtualMachine, stackElem_t * result) {

    ASSERT(virtualMachine);
    ASSERT(result);

    return StackPush(&(virtualMachine->stack), *result);
}

err_t Jump(vmResource * virtualMachine, size_t jumpIndex) {

    ASSERT(virtualMachine);

    if (jumpIndex >= virtualMachine->byteCodeLen) return ERR_PROCESSING;

    virtualMachine->pc = jumpIndex;
    virtualMachine->pcChanged = true;

    return ERR_OK;
}

err_t Sqrt(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    err_t popStat = ERR_OK;

    stackElem_t number = StackPop(&virtualMachine->stack, &popStat);
    if (popStat != ERR_OK) return ERR_COMMAND;

    err_t pushStat = StackPush(&(virtualMachine->stack), (stackElem_t)(pow(number, 0.5)));
    if (pushStat != ERR_OK) return ERR_COMMAND;

    return ERR_OK;
}

err_t Call(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    err_t pushStat = StackPush(&(virtualMachine->funcStack),
                                virtualMachine->pc + GetInstructionSize(virtualMachine));
    if (pushStat != ERR_OK) return ERR_CALL;
    
    return Jump(virtualMachine, (size_t)GetInstructionArgument(virtualMachine));
}

err_t Ret(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    err_t popStat = ERR_OK;
    size_t backIndex = (size_t)StackPop(&(virtualMachine->funcStack), &popStat);
    if (popStat != ERR_OK) return ERR_RET;
    
    return Jump(virtualMachine, backIndex);
}

err_t In(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    stackElem_t newElem = 0;

    if (scanf("%lg", &newElem) == 1) {

        err_t pushStat = StackPush(&(virtualMachine->stack), newElem);
        if (pushStat != ERR_OK) return ERR_PROCESSING;
        return ERR_OK;

    }

    printf("Incorrect input\n");
    return ERR_PROCESSING;
}

err_t OutCom(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    int outStat = ERR_OK;
    stackElem_t out = StackPop(&(virtualMachine->stack), &outStat);

    if (outStat != ERR_OK) return ERR_OUT_PROCESSING;

    printf(BOLD_GREEN "Output: %lg" RESET "\n", (double)out);
    
    return ERR_OK;
}
//------------------------------------------------------------------

size_t GetByteCode(uint8_t ** byteCode, const char * byteCodeFilename) {

    ASSERT(byteCode);
    ASSERT(byteCodeFilename);

    FileStat binFile = {};

    if (!ReadTxtFile(byteCodeFilename, "rb", &binFile)) ASSERT(0);

    *byteCode = (uint8_t *)binFile.text_ptr;
    
    return binFile.sizeInBytes;

}

size_t GetInstructionSize(vmResource * virtualMachine) {

    ASSERT(virtualMachine);
    ASSERT(virtualMachine->byteCode);

    if (virtualMachine->pc >= virtualMachine->byteCodeLen) return 0;

    uint8_t command = virtualMachine->byteCode[virtualMachine->pc];
    size_t argCount = 0;

    if (command & 0x20) argCount++;
    if (command & 0x40) argCount++;

    size_t instrSize = 1 + argCount * sizeof(int32_t);
    if (instrSize > virtualMachine->byteCodeLen - virtualMachine->pc) return 0;

    return instrSize;
}

int32_t GetInstructionArgument(vmResource * virtualMachine) {

    ASSERT(virtualMachine);
    ASSERT(virtualMachine->byteCode);

    int32_t argument = 0;
    if (virtualMachine->pc + 1 + sizeof(argument) > virtualMachine->byteCodeLen) return -1;

    memcpy(&argument, virtualMachine->byteCode + virtualMachine->pc + 1, sizeof(argument));

    return argument;
}


//-----------------------------------------------------------------
// DEBUG
//-----------------------------------------------------------------

void VmDump(FILE * stream, vmResource * virtualMachine) {

    size_t instrSize = GetInstructionSize(virtualMachine);

    printf("\n");
    printf(YELLOW "-------------------------------------------------------------------------\n" RESET);
    printf(YELLOW "                    <<<<<< VM DUMP >>>>>>                                \n" RESET);
    printf(YELLOW "-------------------------------------------------------------------------\n" RESET);

    StackDump(stream, &(virtualMachine->stack));

    printf("virtual machine: [%p]\n", virtualMachine);
    printf("Registers:\n");

    for (size_t i = 0; i < nRegisters; i++) {
        printf("[%1s%d] ", "R", i);
    }
    printf("\n");

    for (size_t i = 0; i < nRegisters; i++) {

        if (virtualMachine->regstr[i] != 0) {
            printf(BOLD_GREEN "[%2lg] " RESET, virtualMachine->regstr[i]);
            continue;
        }

        printf("[%2lg] ", virtualMachine->regstr[i]);
    }
    printf("\n");

    printf("<<<<Byte code>>>>\n");
    for (size_t i = 0; i < virtualMachine->byteCodeLen; i++) {

        if (i < virtualMachine->pc) {
            printf(" %2d  ", i);
        }

        if (i >= virtualMachine->pc && i < virtualMachine->pc + instrSize) {

            printf(BOLD_RED "[%2d] " RESET, i);

        }

        if (i >= virtualMachine->pc + instrSize) {
            printf(" %2d  ", i);
        }

    }
    printf("\n");

    for (size_t i = 0; i < virtualMachine->byteCodeLen; i++) {

        if (i < virtualMachine->pc) {
            printf("[%2d] ", virtualMachine->byteCode[i]);
        }

        if (i >= virtualMachine->pc && i < virtualMachine->pc + instrSize) {

            printf(BOLD_RED "[%2d] " RESET, virtualMachine->byteCode[i]);

        }

        if (i >= virtualMachine->pc + instrSize) {
            printf("[%2d] ", virtualMachine->byteCode[i]);
        }
    }
    printf("\n");
    printf(YELLOW "\n-------------------------------------------------------------------------\n" RESET);
    printf("\n");

}

// IO functions ----------------------------------------------------
err_t ArgvParser(const int argc, const char * const argv[], char ** readFromFile) {

    ASSERT(argv);

    if ((argc == 3) && (!strcmp(argv[1], CMD_FLAG_BYTECODE))) {

        ASSERT(argv[2]);

        char * newPtr = strdup(argv[2]);
        if (newPtr == NULL) return ERR_CTOR;

        *readFromFile = newPtr;

        return ERR_OK;
    }

    printf("\nError of reading stdin\n");
    return ERR_FILEOPEN;
}

void DrawRam(const vmResource * virtualMachine) {

    ASSERT(virtualMachine);
    
    char screenBuffer[picLength * picWidth + picWidth + 1] = "";
    size_t bufIndex = 0;

    for (size_t i = 0; i < picWidth; i++) {

        for (size_t j = 0; j < picLength; j++) {

            int elem = (int)(virtualMachine->RAM[i*picLength + j]);

            if (elem == 0) {
                screenBuffer[bufIndex++] = ' ';
                
                continue;
            }

            screenBuffer[bufIndex++] = '&';

        }

        screenBuffer[bufIndex++] = '\n';

    }
    
    screenBuffer[bufIndex] = '\0';
    fwrite(screenBuffer, sizeof(char), bufIndex, stdout);
    fflush(stdout);
}