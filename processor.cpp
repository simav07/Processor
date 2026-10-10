#include "stack.h"

//! Filenames for writing bytecode or asm source

const char * PROG_BYTECODE_FILE   = "pbytecode.txt";
const char * PROG_ASM_SOURCE_FILE = "pcode.asm";
const char * USER_BYTECODE_FILE   = "ubytecode.txt";
const char * USER_ASM_SOURCE_FILE = "ucode.asm";

//! User commands from stdin
const char * CMD_FLAG_ASM      = "--asmsource";
const char * CMD_FLAG_BYTECODE = "--bytecode";

//! File for StackDump, there we can see what happening with stack
const char * FILE_STACKDUMP = "dump.txt";

//! Main virtual machine
err_t RunVirtualMachine(VM_resource * virtualMachine);
err_t PrepareByteCode(VM_resource * virtualMachine, const int argc, char * argv[]);

err_t GetFilenameStd(int argc, char * argv[], AsmFilesInfo * fileInfo);

//! Interpreter: run commands
err_t Interpreter(int * byteCode, stack_t * stack, int commandCode);

//! Commands for virtual machine -------------------------------------
err_t PushCom(int * byteCode, stack_t * stack, int index);

err_t GetTwoPop(stack_t * stack, stackElem_t * a, stackElem_t * b);

err_t AddCom(stack_t * stack);
err_t SubCom(stack_t * stack);
err_t DivCom(stack_t * stack);

stackElem_t OutCom(stack_t * stack);
// -------------------------------------------------------------------

int main(const int argc, char * argv[]) {

    VM_resource virtualMachine = {};
    
    err_t preparingStat = PrepareByteCode(&virtualMachine, argc, argv);

    if (preparingStat != ERR_OK) {
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

    return true;
}

err_t PrepareByteCode(VM_resource * virtualMachine, const int argc, char * argv[]) {

    AsmFilesInfo asmInfo = {};

    int filenamesStat = GetFilenameStd(argc, argv, &asmInfo);

    //! Asm source -> byte code
    int asmStat = Assemble(asmInfo.readFrom, asmInfo.writeTo);

    if (asmStat != ERR_OK) {
        // error description
        printf("ERR_ASM\n");
        return ERR_ASM;
    }

    if (strcmp(asmInfo.readFrom, USER_ASM_SOURCE_FILE) != 0) ASSERT(0);

    size_t byteCodeLen = GetByteCode(&(virtualMachine->byteCode), &asmInfo);

    if (byteCodeLen == ERR_BYTECODE) {
        printf(RED "ERR_BYTECODE\n" RESET);
        return ERR_BYTECODE;
    }

    virtualMachine->byteCodeLen = byteCodeLen;

    return ERR_OK;

}

err_t RunVirtualMachine(VM_resource * virtualMachine) {

    ASSERT(virtualMachine);
    ASSERT(virtualMachine->byteCode);

    size_t pc = 0; // in struct

    int ctorStat = StackCtor(&(virtualMachine->stack), 10);
    if (ctorStat != ERR_OK) return ERR_CTOR;

    FILE * file_p = fopen(FILE_STACKDUMP, "w");

    stackElem_t outResult = 0;

    while (virtualMachine->byteCode[pc] != HLT_C) {

        StackDump(file_p, &(virtualMachine->stack));

        if (pc > virtualMachine->byteCodeLen) {
            printf(BOLD_RED "In asm file no HLT command\n" RESET);
            return ERR_PROCESSING;
        }

        if (virtualMachine->byteCode[pc] == OUT_C) {
            stackElem_t out = OutCom(&(virtualMachine->stack));

            if (out == ERR_COMMAND) return ERR_OUT_PROCESSING;

            outResult = out;
            pc++;
            continue;

        }

        int goOn = Interpreter(virtualMachine->byteCode, &(virtualMachine->stack), pc);

        if (goOn == ERR_COMMAND) {
            printf(RED "ERROR_PROCESSING\n" RESET);
            return ERR_PROCESSING;
        }
        pc = pc + goOn;

    }
    fclose(file_p);

    printf(BOLD_GREEN "Output: %lg" RESET, outResult);

    return ERR_OK;
}

err_t Interpreter(int * byteCode, stack_t * stack, int index) {

    ASSERT(stack);
    ASSERT(byteCode);

    switch (byteCode[index])
    {
    // PUSH
    case (PUSH_C):
        return PushCom(byteCode, stack, index);
    
    // ADD (+)
    case (ADD_C):
        return AddCom(stack);

    // SUB (-)
    case (SUB_C):
        return SubCom(stack);

    // DIV (/)
    case (DIV_C):
        return DivCom(stack);

    // OUT
    case (OUT_C):
        return OutCom(stack);

    case (HLT_C):
        return 0;
    
    default:
        return ERR_COMMAND;
    }

}

//------------------------------------------------------------------
// Functions for processor
//------------------------------------------------------------------

err_t PushCom(int * byteCode, stack_t * stack, int index) {

    ASSERT(byteCode);
    ASSERT(stack);

    err_t pushStat = StackPush(stack, byteCode[index + 1]);

    if (pushStat != ERR_OK) return ERR_COMMAND;

    return 2;

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

err_t AddCom(stack_t * stack) {

    ASSERT(stack);

    double a = 0;
    double b = 0;

    if (GetTwoPop(stack, &a, &b) != ERR_OK) {
        printf("Can't get a and b\n");
        return ERR_COMMAND;
    }

    int pushStat = StackPush(stack, (stackElem_t)(a + b));
    if (pushStat != ERR_OK) return ERR_COMMAND;

    return 1;

}

err_t SubCom(stack_t * stack) {

    ASSERT(stack);

    stackElem_t a = 0, b = 0;

    if (GetTwoPop(stack, &a, &b) != ERR_OK) return ERR_COMMAND;

    int pushStat = StackPush(stack, (stackElem_t)(b - a));

    if (pushStat != ERR_OK) return ERR_COMMAND;

    return 1;

}

err_t DivCom(stack_t * stack) {

    ASSERT(stack);

    stackElem_t a = 0, b = 0;

    if (GetTwoPop(stack, &a, &b) != ERR_OK) return ERR_COMMAND;

    int pushStat = StackPush(stack, (stackElem_t)((double)b / (double)a));

    if (pushStat != ERR_OK) return ERR_COMMAND;

    return 1;

}

stackElem_t OutCom(stack_t * stack) {

    ASSERT(stack);

    int outStat = ERR_OK;
    stackElem_t out = StackPop(stack, &outStat);

    if (outStat != ERR_OK) return ERR_COMMAND;

    return out;

}
//------------------------------------------------------------------

// IO functions ----------------------------------------------------
err_t GetFilenameStd(int argc, char * argv[], AsmFilesInfo * fileInfo) {

    ASSERT(argv);
    ASSERT(*argv);

    // add more if
    if ((argc == 3) && (!strcmp(argv[1], CMD_FLAG_ASM) || (!strcmp(argv[1], CMD_FLAG_BYTECODE)))) {

        if (!argv[2]) return false;

        //! User put asm filename, so processor must use assembler and get a bytecode
        if (!strcmp(argv[1], CMD_FLAG_ASM)) {

            (*fileInfo).readFrom = argv[2];
            (*fileInfo).writeTo  = (char *)PROG_BYTECODE_FILE;

            return true;
        }

        //! User put bytecode filename, so processor must use this bytecode and write a retranslation to asm
        if (!strcmp(argv[1], CMD_FLAG_BYTECODE)) {

            (*fileInfo).readFrom = argv[2];
            (*fileInfo).writeTo  = (char *)PROG_ASM_SOURCE_FILE;

            return true;
        }

        return false;
    }

    printf("\nError of reading stdin\n");
    return false;
}