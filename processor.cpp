#include "include/stack.h"
#include "include/io.h"
#include "include/commands.h"
#include "hashcommands.h"
#include <string.h>

struct vmResource {

    stack_t stack = {};

    FILE * fileDump = NULL;
    
    uint8_t * byteCode = NULL;
    size_t byteCodeLen = 0;
    
    size_t pc = 0;
    bool pcChanged = false;

    stack_t funcStack = {};

    //! Registers
    stackElem_t regstr[5] = {};

    stackElem_t * RAM;
};

const size_t ramSize = 200;

//! Flag for byte code filename
static const char * CMD_FLAG_BYTECODE = "--bytecode";

//! #define VMDump for debug

//! File for StackDump, there we can see what happening with stack
static const char * FILE_STACKDUMP = "dump.txt";

void VmDump(FILE * stream, vmResource * virtualMachine);

err_t VmCtor(const int argc, const char * argv[], vmResource * virtualMachine);
void VmDtor(vmResource * virtualMachine);

err_t ArgvParser(const int argc, const char * argv[], char * readFromFile);

//! Main virtual machine
err_t RunVirtualMachine(vmResource * virtualMachine);
err_t PrepareByteCode(vmResource * virtualMachine, const int argc, const char * argv[]);

//! Fill byte code buffer from txt
size_t GetByteCode(uint8_t ** byteCode, const char * byteCodeFilename);

size_t GetInstructionSize(vmResource * virtualMachine);
int32_t GetInstructionArgument(vmResource * virtualMachine);


//! RunCommands: run commands
err_t RunCommands(vmResource * virtualMachine);
err_t Interpreter(vmResource * virtualMachine);

//! Commands for virtual machine -------------------------------------
err_t PushCom(vmResource * virtualMachine);

err_t GetTwoPop(stack_t * stack, stackElem_t * a, stackElem_t * b);

err_t Pop(vmResource * virtualMachine, uint8_t flags);
err_t PushR(vmResource * virtualMachine);

err_t Jump(vmResource * virtualMachine, size_t jumpIndex);
err_t JA(vmResource * virtualMachine);
err_t JAE(vmResource * virtualMachine);
err_t JB(vmResource * virtualMachine);
err_t JBE(vmResource * virtualMachine);
err_t JE(vmResource * virtualMachine);
err_t JNE(vmResource * virtualMachine);

err_t IN(vmResource * virtualMachine);

err_t AddCom(vmResource * virtualMachine);
err_t SubCom(vmResource * virtualMachine);
err_t DivCom(vmResource * virtualMachine);
err_t MulCom(vmResource * virtualMachine);

err_t Call(vmResource * virtualMachine);
err_t Ret(vmResource * virtualMachine);

err_t OutCom(vmResource * virtualMachine);
// -------------------------------------------------------------------

int main(const int argc, const char * argv[]) {

    printf("\nFile <%s>, [%s]\n", __FILE__, __TIME__);

    vmResource virtualMachine = {};
    
    // constructor de facto 
    err_t ctorStat = VmCtor(argc, argv, &virtualMachine);
    if (ctorStat != ERR_OK) {
        // error description
        printf("ERROR OF MAKING BYTE CODE\n");
        return false;
    }

    err_t processingStat = RunVirtualMachine(&virtualMachine);

    if (processingStat != ERR_OK) {
        // GetErrorDescription
        printf("ERROR OF VIRTUAL MACHINE\n");
        return false;
    }

    VmDtor(&virtualMachine);

    return true;
}

err_t VmCtor(const int argc, const char * argv[], vmResource * virtualMachine) {

    ASSERT(argv);
    ASSERT(virtualMachine);

    virtualMachine->pc = 0;
 
    err_t preparingStat = PrepareByteCode(virtualMachine, argc, argv);

    if (preparingStat != ERR_OK) {
        // error description
        printf("ERROR OF MAKING BYTE CODE\n");
        return ERR_VM_CTOR;
    }

    virtualMachine->pc = 0;

    stackElem_t * newMemPtr = (stackElem_t *)calloc(ramSize, sizeof(stackElem_t));
    if (!newMemPtr) return ERR_CTOR;
    virtualMachine->RAM = newMemPtr;

    int ctorStat1 = StackCtor(&(virtualMachine->stack), 10);
    if (ctorStat1 != ERR_OK) return ERR_VM_CTOR;

    int ctorStat2 = StackCtor(&(virtualMachine->funcStack), 10);
    if (ctorStat2 != ERR_OK) return ERR_VM_CTOR;

    virtualMachine->fileDump = fopen(FILE_STACKDUMP, "w");
    if (!virtualMachine->fileDump) return ERR_FILEOPEN;

    return ERR_OK;

}

void VmDtor(vmResource * virtualMachine) {

    ASSERT(virtualMachine);
    ASSERT(&virtualMachine->funcStack);
    ASSERT(&virtualMachine->stack);

    fclose(virtualMachine->fileDump);

    StackDtor(&(virtualMachine->funcStack));
    StackDtor(&(virtualMachine->stack));

}

err_t PrepareByteCode(vmResource * virtualMachine, const int argc, const char * argv[]) {

    ASSERT(virtualMachine);
    ASSERT(argv);

    char byteCodeFile[100] = "";

    int filenamesStat = ArgvParser(argc, argv, byteCodeFile);
    if (filenamesStat != ERR_OK) {
        printf("Error of reading bytecode filename\n");
        return ERR_ASM;
    }

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

        if ((virtualMachine->byteCode[virtualMachine->pc] & 0x1F) == HLT_C) return ERR_OK;

        virtualMachine->pcChanged = false;
        size_t instrSize = GetInstructionSize(virtualMachine);

        if (instrSize == 0 || instrSize > virtualMachine->byteCodeLen - virtualMachine->pc) {
            printf(BOLD_RED "Incorrect instruction at pc=%zu\n" RESET, virtualMachine->pc);
            return ERR_PROCESSING;
        }

        err_t interpreterStat = Interpreter(virtualMachine);

        if (interpreterStat != ERR_OK) {
            printf(RED "ERROR_PROCESSING: command at pc=%zu, error=%d\n" RESET,
                                                virtualMachine->pc, interpreterStat);
            return interpreterStat;
        }

        #ifdef VMDump
        VmDump(stdout, virtualMachine);
        getchar();
        #endif

        if (!virtualMachine->pcChanged) virtualMachine->pc += instrSize;
    }

    printf(BOLD_RED "In asm file no HLT command\n" RESET);
    return ERR_PROCESSING;
}

err_t Interpreter(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    uint8_t command = virtualMachine->byteCode[virtualMachine->pc] & 0x1F;

    uint8_t flags = virtualMachine->byteCode[virtualMachine->pc];
    
    stackElem_t result = 0;
    size_t argIndex = virtualMachine->pc + 1;
    int32_t value = 0;

    if (flags & 0x40) {
        
        int32_t regIndex = 0;

        if (argIndex + sizeof(int32_t) > virtualMachine->byteCodeLen) return ERR_PROCESSING;

        memcpy(&regIndex, virtualMachine->byteCode + argIndex, sizeof(regIndex));
        
        if (regIndex < 0 || regIndex >= 5) return ERR_COMMAND;
        
        result += virtualMachine->regstr[regIndex];
        argIndex += sizeof(int32_t);
    }

    if (flags & 0x20) {
        if (argIndex + sizeof(int32_t) > virtualMachine->byteCodeLen) return ERR_PROCESSING;

        memcpy(&value, virtualMachine->byteCode + argIndex, sizeof(value));

        result += value;
        argIndex += sizeof(int32_t);
    }

    if (flags & 0x80) {
        if (result < 0 || result >= (stackElem_t)ramSize) return ERR_COMMAND;
        
        if (command == POP_C) {
            printf("Pop command\n");
            err_t popStat = ERR_OK;
            stackElem_t var = StackPop(&(virtualMachine->stack), &popStat);
            if (popStat != ERR_OK) return ERR_COMMAND;
            virtualMachine->RAM[(size_t)result] = var;
            return ERR_OK;
        }
        result = virtualMachine->RAM[(size_t)result];
    }

    switch (command) {

        case PUSH_C: {
            if (!(flags & 0xE0)) return ERR_COMMAND;
            if (flags & 0x80) return StackPush(&(virtualMachine->stack), result);
            
            return StackPush(&(virtualMachine->stack), result);
        }

        case POP_C: {
            if (flags & 0x80) return ERR_OK;
            // if (!(flags & 0x40) || (flags & 0x20)) return ERR_COMMAND;
            
            int32_t regIndex = 0;
            memcpy(&regIndex, virtualMachine->byteCode + virtualMachine->pc + 1, sizeof(regIndex));

            if (regIndex < 0 || regIndex >= 5) return ERR_COMMAND;
            err_t popStat = ERR_OK;
            stackElem_t var = StackPop(&(virtualMachine->stack), &popStat);
            
            if (popStat != ERR_OK) return ERR_COMMAND;
            
            virtualMachine->regstr[regIndex] = var;
            return ERR_OK;
        }

        case JUMP_C: return Jump(virtualMachine, (size_t)GetInstructionArgument(virtualMachine));
        case JA_C:   return JA(virtualMachine);
        case JAE_C:  return JAE(virtualMachine);
        case JB_C:   return JB(virtualMachine);
        case JBE_C:  return JBE(virtualMachine);
        case JE_C:   return JE(virtualMachine);
        case JNE_C:  return JNE(virtualMachine);
        case IN_C:   return IN(virtualMachine);
        case RET_C:  return Ret(virtualMachine);
        case CALL_C: return Call(virtualMachine);
        case ADD_C:  return AddCom(virtualMachine);
        case SUB_C:  return SubCom(virtualMachine);
        case DIV_C:  return DivCom(virtualMachine);
        case MUL_C:  return MulCom(virtualMachine);
        case OUT_C:  return OutCom(virtualMachine);
        case HLT_C:  return ERR_OK;

        default:     return ERR_COMMAND;
    }
}

err_t RunCommands(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    if (virtualMachine->pc >= virtualMachine->byteCodeLen) return ERR_PROCESSING;

    return Interpreter(virtualMachine);

}

//------------------------------------------------------------------
// Functions for processor
//------------------------------------------------------------------

err_t PushCom(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    err_t pushStat = StackPush(&(virtualMachine->stack), GetInstructionArgument(virtualMachine));

    if (pushStat != ERR_OK) return ERR_COMMAND;
    return ERR_OK;

}

err_t Pop(vmResource * virtualMachine, uint8_t flags) {

    ASSERT(virtualMachine);

    if (flags & 0x80) return ERR_OK;

    if (!(flags & 0x40) || (flags & 0x20)) return ERR_COMMAND;
    
    int32_t regIndex = 0;
    memcpy(&regIndex, virtualMachine->byteCode + virtualMachine->pc + 1, sizeof(regIndex));

    if (regIndex < 0 || regIndex >= 5) return ERR_COMMAND;
    err_t popStat = ERR_OK;
    stackElem_t var = StackPop(&(virtualMachine->stack), &popStat);
    
    if (popStat != ERR_OK) return ERR_COMMAND;
    
    virtualMachine->regstr[regIndex] = var;
    return ERR_OK;
}

err_t PushR(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    int32_t registerIndex = GetInstructionArgument(virtualMachine);
    if (registerIndex < 0 || registerIndex >= 5) return ERR_COMMAND;

    err_t pushStat = StackPush(&(virtualMachine->stack), virtualMachine->regstr[registerIndex]);

    if (pushStat != ERR_OK) return ERR_PUSH_COM;
    return ERR_OK;
}

err_t Jump(vmResource * virtualMachine, size_t jumpIndex) {

    ASSERT(virtualMachine);

    if (jumpIndex >= virtualMachine->byteCodeLen) return ERR_PROCESSING;

    virtualMachine->pc = jumpIndex;
    virtualMachine->pcChanged = true;

    return ERR_OK;

}

err_t JA(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    err_t popStat1 = ERR_OK;
    err_t popStat2 = ERR_OK;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    if (popStat1 != ERR_OK) return ERR_COMMAND;

    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);

    if (popStat2 != ERR_OK) return ERR_COMMAND;
    
    if (a > b) return Jump(virtualMachine, (size_t)GetInstructionArgument(virtualMachine));

    return ERR_OK;
}

err_t JAE(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    err_t popStat1 = ERR_OK;
    err_t popStat2 = ERR_OK;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    if (popStat1 != ERR_OK) return ERR_COMMAND;

    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);
    
    if (popStat2 != ERR_OK) return ERR_COMMAND;

    if (a >= b) return Jump(virtualMachine, (size_t)GetInstructionArgument(virtualMachine));

    return ERR_OK;
}

err_t JB(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    err_t popStat1 = ERR_OK;
    err_t popStat2 = ERR_OK;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    if (popStat1 != ERR_OK) return ERR_COMMAND;

    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);
    
    if (popStat2 != ERR_OK) return ERR_COMMAND;
    
    if (a < b) return Jump(virtualMachine, (size_t)GetInstructionArgument(virtualMachine));

    return ERR_OK;
}

err_t JBE(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    err_t popStat1 = ERR_OK;
    err_t popStat2 = ERR_OK;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    if (popStat1 != ERR_OK) return ERR_COMMAND;

    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);
    if (popStat2 != ERR_OK) return ERR_COMMAND;
    
    if (a <= b) return Jump(virtualMachine, (size_t)GetInstructionArgument(virtualMachine));

    return ERR_OK;
}

err_t JE(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    err_t popStat1 = ERR_OK;
    err_t popStat2 = ERR_OK;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    if (popStat1 != ERR_OK) return ERR_COMMAND;

    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);
    if (popStat2 != ERR_OK) return ERR_COMMAND;
    
    if (a == b) return Jump(virtualMachine, (size_t)GetInstructionArgument(virtualMachine));

    return ERR_OK;
}

err_t JNE(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    err_t popStat1 = ERR_OK;
    err_t popStat2 = ERR_OK;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    if (popStat1 != ERR_OK) return ERR_COMMAND;

    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);
    if (popStat2 != ERR_OK) return ERR_COMMAND;
    
    if (a != b) return Jump(virtualMachine, (size_t)GetInstructionArgument(virtualMachine));

    return ERR_OK;
}

err_t IN(vmResource * virtualMachine) {

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

err_t AddCom(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    stackElem_t a = 0;
    stackElem_t b = 0;

    if (GetTwoPop(&(virtualMachine->stack), &a, &b) != ERR_OK) {
        printf("Can't get a and b\n");
        return ERR_COMMAND;
    }

    int pushStat = StackPush(&(virtualMachine->stack), (stackElem_t)(a + b));
    
    if (pushStat != ERR_OK) return ERR_COMMAND;
    
    return ERR_OK;
}

err_t SubCom(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    stackElem_t a = 0, b = 0;

    if (GetTwoPop(&(virtualMachine->stack), &a, &b) != ERR_OK) return ERR_COMMAND;

    int pushStat = StackPush(&(virtualMachine->stack), (stackElem_t)(b - a));
    
    if (pushStat != ERR_OK) return ERR_COMMAND;
    
    return ERR_OK;
}

err_t DivCom(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    stackElem_t a = 0, b = 0;

    if (GetTwoPop(&(virtualMachine->stack), &a, &b) != ERR_OK) return ERR_COMMAND;
    if (a == 0) return ERR_COMMAND;

    int pushStat = StackPush(&(virtualMachine->stack), (stackElem_t)((double)b / (double)a));
    if (pushStat != ERR_OK) return ERR_COMMAND;
    
    return ERR_OK;
}

err_t MulCom(vmResource * virtualMachine) {

    ASSERT(virtualMachine);

    stackElem_t a = 0, b = 0;

    if (GetTwoPop(&(virtualMachine->stack), &a, &b) != ERR_OK) return ERR_COMMAND;

    int pushStat = StackPush(&(virtualMachine->stack), (stackElem_t)(b * a));
    if (pushStat != ERR_OK) return ERR_COMMAND;
    
    return ERR_OK;
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

    FileStat binFile = {}; // rename

    //if (!GetAsmFilenameStd(argc, argv, &txtFile)) ASSERT(0);
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

    if ((command & 0x1F) == JUMP_C || (command & 0x1F) == JA_C ||
        (command & 0x1F) == JAE_C || (command & 0x1F) == JB_C ||
        (command & 0x1F) == JBE_C || (command & 0x1F) == JE_C ||
        (command & 0x1F) == JNE_C || (command & 0x1F) == CALL_C) argCount = 1;

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

    for (size_t i = 0; i < 5; i++) {
        printf("[%1s%d] ", "R", i);
    }
    printf("\n");

    for (size_t i = 0; i < 5; i++) {

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
err_t ArgvParser(const int argc, const char * argv[], char * readFromFile) {

    ASSERT(argv);

    if ((argc == 3) && (!strcmp(argv[1], CMD_FLAG_BYTECODE))) {

        ASSERT(argv[2]);

        //! User put bytecode filename, so processor must use this bytecode
        size_t len = strlen(argv[2]);
        memcpy(readFromFile, argv[2], len + 1);

        printf(YELLOW "Executable file: <%s>\n" RESET, argv[2]);

        return ERR_OK;
    }

    printf("\nError of reading stdin\n");
    return ERR_FILEOPEN;
}