#include "include/stack.h"
#include "include/io.h"
#include "include/commands.h"
#include "hashcommands.h"

// mother fucker naming 
//     VirtualMachine
struct VM_resource {

    stack_t stack = {};
    
    int * byteCode = NULL;
    size_t byteCodeLen = 0;
    
    size_t pc = 0;
    bool pcChanged = false;

    stack_t funcStack = {};

    //! Registers
    stackElem_t regstr[5] = {};
};

//! Flag for byte code filename
static const char * CMD_FLAG_BYTECODE = "--bytecode";

//! #define VMDump for debug

//! File for StackDump, there we can see what happening with stack
static const char * FILE_STACKDUMP = "dump.txt";

void VmDump(FILE * stream, VM_resource * virtualMachine, size_t upCode);

err_t GetBCfilenameStd(int argc, char * argv[], char * readFromFile);

//! Main virtual machine
err_t RunVirtualMachine(VM_resource * virtualMachine);
err_t PrepareByteCode(VM_resource * virtualMachine, const int argc, char * argv[]);

//! Fill byte code buffer from txt
size_t GetByteCode(int ** byteCode, const char * byteCodeFilename);

//! Interpreter: run commands
err_t Interpreter(VM_resource * virtualMachine, size_t * upCode);

//! Commands for virtual machine -------------------------------------
err_t PushCom(VM_resource * virtualMachine, size_t * upCode);

err_t GetTwoPop(stack_t * stack, stackElem_t * a, stackElem_t * b);

err_t PopR(VM_resource * virtualMachine, size_t * upCode);
err_t PushR(VM_resource * virtualMachine, size_t * upCode); // change to enum

err_t Jump(VM_resource * virtualMachine, size_t jumpIndex);
err_t JA(VM_resource * virtualMachine, size_t * upCode);
err_t JAE(VM_resource * virtualMachine, size_t * upCode);
err_t JB(VM_resource * virtualMachine, size_t * upCode);
err_t JBE(VM_resource * virtualMachine, size_t * upCode);
err_t JE(VM_resource * virtualMachine, size_t * upCode);
err_t JNE(VM_resource * virtualMachine, size_t * upCode);

err_t IN(VM_resource * virtualMachine, size_t * upCode);

err_t AddCom(VM_resource * virtualMachine, size_t * upCode);
err_t SubCom(VM_resource * virtualMachine, size_t * upCode);
err_t DivCom(VM_resource * virtualMachine, size_t * upCode);
err_t MulCom(VM_resource * virtualMachine, size_t * upCode);

err_t Call(VM_resource * virtualMachine, size_t * upCode);
err_t Ret(VM_resource * virtualMachine, size_t * upCode);

err_t OutCom(VM_resource * virtualMachine, size_t * upCode);
// -------------------------------------------------------------------

int main(const int argc, /*const?*/ char * argv[]) {

    printf("\nFile <%s>, [%s]\n", __FILE__, __TIME__);

    VM_resource virtualMachine = {};
    
    // constructor de facto 
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

//TODO ////////////////////// destructor mother fucker//////////////////////////

    return true;
}

err_t PrepareByteCode(VM_resource * virtualMachine, const int argc, char * argv[]) {

    ASSERT(virtualMachine);
    ASSERT(argv);

    char byteCodeFile[100] = "";// why =(

    int filenamesStat = GetBCfilenameStd(argc, argv, byteCodeFile);
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

err_t RunVirtualMachine(VM_resource * virtualMachine) {

    ASSERT(virtualMachine);
    ASSERT(virtualMachine->byteCode);

    virtualMachine->pc = 0; // mother fucker what is it

    int ctorStat1 = StackCtor(&(virtualMachine->stack), 10); // mother fucker its constructor
    if (ctorStat1 != ERR_OK) return ERR_CTOR;

    int ctorStat2 = StackCtor(&(virtualMachine->funcStack), 10); // mother fucker its constructor
    if (ctorStat2 != ERR_OK) return ERR_CTOR;

    FILE * file_p = fopen(FILE_STACKDUMP, "w"); // mother fucker its constructor
    if (!file_p) return ERR_FILEOPEN;

    while (true) {

        if (virtualMachine->pc >= virtualMachine->byteCodeLen) {

            printf(BOLD_RED "In asm file no HLT command\n" RESET); // destructor
            
            fclose(file_p);
            
            StackDtor(&(virtualMachine->funcStack));
            StackDtor(&(virtualMachine->stack));
            return ERR_PROCESSING;
        }

        if (virtualMachine->byteCode[virtualMachine->pc] == HLT_C) break;

        virtualMachine->pcChanged = false;
        size_t upCode = 1;
        err_t interpreterStat = Interpreter(virtualMachine, &upCode);

        if (interpreterStat != ERR_OK) {
            printf(RED "ERROR_PROCESSING: command at pc=%zu, error=%d\n" RESET,
                   virtualMachine->pc, interpreterStat);
            fclose(file_p);
            StackDtor(&(virtualMachine->funcStack));
            StackDtor(&(virtualMachine->stack));
            return interpreterStat;
        }

        #ifdef VMDump
        VmDump(stdout, virtualMachine, upCode);
        getchar();
        #endif

        if (virtualMachine->pcChanged == false) {
            virtualMachine->pc = virtualMachine->pc + upCode;
        }
    }

    fclose(file_p); // mother fucker its destructor

    StackDtor(&(virtualMachine->funcStack)); // mother fucker its destructor
    StackDtor(&(virtualMachine->stack)); // mother fucker its destructor

    return ERR_OK;
}

err_t Interpreter(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    *upCode = 1;

    if (virtualMachine->pc >= virtualMachine->byteCodeLen) return ERR_PROCESSING;

    switch (virtualMachine->byteCode[virtualMachine->pc])
    {
    // PUSH
    case (PUSH_C):
        if (virtualMachine->pc + 1 >= virtualMachine->byteCodeLen) return ERR_PROCESSING;
        *upCode = 2;
        return PushCom(virtualMachine, upCode);

    case (POP_R):
        if (virtualMachine->pc + 1 >= virtualMachine->byteCodeLen) return ERR_PROCESSING;
        *upCode = 2;
        return PopR(virtualMachine, upCode);

    case (PUSH_R):
        if (virtualMachine->pc + 1 >= virtualMachine->byteCodeLen) return ERR_PROCESSING;
        *upCode = 2;
        return PushR(virtualMachine, upCode);

    case (JUMP_C):
        if (virtualMachine->pc + 1 >= virtualMachine->byteCodeLen) return ERR_PROCESSING;
        *upCode = 2;
        return Jump(virtualMachine, (size_t)virtualMachine->byteCode[virtualMachine->pc + 1]);

    case (JA_C):
        if (virtualMachine->pc + 1 >= virtualMachine->byteCodeLen) return ERR_PROCESSING;
        *upCode = 2;
        return JA(virtualMachine, upCode);

    case (JAE_C):
        if (virtualMachine->pc + 1 >= virtualMachine->byteCodeLen) return ERR_PROCESSING;
        *upCode = 2;
        return JAE(virtualMachine, upCode);

    case (JB_C):
        if (virtualMachine->pc + 1 >= virtualMachine->byteCodeLen) return ERR_PROCESSING;
        *upCode = 2;
        return JB(virtualMachine, upCode);

    case (JBE_C):
        if (virtualMachine->pc + 1 >= virtualMachine->byteCodeLen) return ERR_PROCESSING;
        *upCode = 2;
        return JBE(virtualMachine, upCode);

    case (JE_C):
        if (virtualMachine->pc + 1 >= virtualMachine->byteCodeLen) return ERR_PROCESSING;
        *upCode = 2;
        return JE(virtualMachine, upCode);

    case (JNE_C):
        if (virtualMachine->pc + 1 >= virtualMachine->byteCodeLen) return ERR_PROCESSING;
        *upCode = 2;
        return JNE(virtualMachine, upCode);

    case (IN_C):
        return IN(virtualMachine, upCode);

    case (RET_C):
        return Ret(virtualMachine, upCode);

    case (CALL_C):
        if (virtualMachine->pc + 1 >= virtualMachine->byteCodeLen) return ERR_PROCESSING;
        *upCode = 2;
        return Call(virtualMachine, upCode);

    // ADD (+)
    case (ADD_C):
        return AddCom(virtualMachine, upCode);

    // SUB (-)
    case (SUB_C):
        return SubCom(virtualMachine, upCode);

    // DIV (/)
    case (DIV_C):
        return DivCom(virtualMachine, upCode);

    // MUL (*)
    case (MUL_C):
        return MulCom(virtualMachine, upCode);

    // OUT
    case (OUT_C):
        return OutCom(virtualMachine, upCode);

    case (HLT_C):
        return ERR_OK;

    default:
        return ERR_COMMAND;
    }

}

//------------------------------------------------------------------
// Functions for processor
//------------------------------------------------------------------

err_t PushCom(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    err_t pushStat = StackPush(&(virtualMachine->stack), virtualMachine->byteCode[virtualMachine->pc + 1]);

    if (pushStat != ERR_OK) return ERR_COMMAND;

    *upCode = 2;
    return ERR_OK;

}

err_t PopR(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    int registerIndex = virtualMachine->byteCode[virtualMachine->pc + 1];
    if (registerIndex < 0 || registerIndex >= 5) return ERR_COMMAND;

    err_t popStat = 0;
    stackElem_t var = StackPop(&(virtualMachine->stack), &popStat);

    if (popStat != ERR_OK) return ERR_COMMAND;

    virtualMachine->regstr[registerIndex] = var;

    *upCode = 2;
    return ERR_OK;
}

err_t PushR(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    int registerIndex = virtualMachine->byteCode[virtualMachine->pc + 1];
    if (registerIndex < 0 || registerIndex >= 5) return ERR_COMMAND;

    err_t pushStat = StackPush(&(virtualMachine->stack), virtualMachine->regstr[registerIndex]);

    if (pushStat != ERR_OK) return ERR_PUSH_COM;

    *upCode = 2;
    return ERR_OK;
}

err_t Jump(VM_resource * virtualMachine, size_t jumpIndex) {

    ASSERT(virtualMachine);

    if (jumpIndex >= virtualMachine->byteCodeLen) return ERR_PROCESSING;

    virtualMachine->pc = jumpIndex;
    virtualMachine->pcChanged = true;

    return ERR_OK;

}

err_t JA(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    err_t popStat1 = ERR_OK;
    err_t popStat2 = ERR_OK;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    if (popStat1 != ERR_OK) return ERR_COMMAND;

    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);
    if (popStat2 != ERR_OK) return ERR_COMMAND;

    *upCode = 2;
    if (a > b) return Jump(virtualMachine, (size_t)virtualMachine->byteCode[virtualMachine->pc + 1]);

    return ERR_OK;
}

err_t JAE(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    err_t popStat1 = ERR_OK;
    err_t popStat2 = ERR_OK;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    if (popStat1 != ERR_OK) return ERR_COMMAND;

    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);
    if (popStat2 != ERR_OK) return ERR_COMMAND;

    *upCode = 2;
    if (a >= b) return Jump(virtualMachine, (size_t)virtualMachine->byteCode[virtualMachine->pc + 1]);

    return ERR_OK;
}

err_t JB(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    err_t popStat1 = ERR_OK;
    err_t popStat2 = ERR_OK;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    if (popStat1 != ERR_OK) return ERR_COMMAND;

    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);
    if (popStat2 != ERR_OK) return ERR_COMMAND;

    *upCode = 2;
    if (a < b) return Jump(virtualMachine, (size_t)virtualMachine->byteCode[virtualMachine->pc + 1]);

    return ERR_OK;
}

err_t JBE(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    err_t popStat1 = ERR_OK;
    err_t popStat2 = ERR_OK;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    if (popStat1 != ERR_OK) return ERR_COMMAND;

    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);
    if (popStat2 != ERR_OK) return ERR_COMMAND;

    *upCode = 2;
    if (a <= b) return Jump(virtualMachine, (size_t)virtualMachine->byteCode[virtualMachine->pc + 1]);

    return ERR_OK;
}

err_t JE(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    err_t popStat1 = ERR_OK;
    err_t popStat2 = ERR_OK;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    if (popStat1 != ERR_OK) return ERR_COMMAND;

    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);
    if (popStat2 != ERR_OK) return ERR_COMMAND;

    *upCode = 2;
    if (a == b) return Jump(virtualMachine, (size_t)virtualMachine->byteCode[virtualMachine->pc + 1]);

    return ERR_OK;
}

err_t JNE(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    err_t popStat1 = ERR_OK;
    err_t popStat2 = ERR_OK;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    if (popStat1 != ERR_OK) return ERR_COMMAND;

    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);
    if (popStat2 != ERR_OK) return ERR_COMMAND;

    *upCode = 2;
    if (a != b) return Jump(virtualMachine, (size_t)virtualMachine->byteCode[virtualMachine->pc + 1]);

    return ERR_OK;
}

err_t IN(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    stackElem_t newElem = 0;

    if (scanf("%lg", &newElem) == 1) {

        err_t pushStat = StackPush(&(virtualMachine->stack), newElem);
        if (pushStat != ERR_OK) return ERR_PROCESSING;

        *upCode = 1;
        return ERR_OK;

    }

    printf("Incorrect input\n");
    return ERR_PROCESSING;
}

err_t Call(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    err_t pushStat = StackPush(&(virtualMachine->funcStack), virtualMachine->pc);
    if (pushStat != ERR_OK) return ERR_CALL;

    *upCode = 2;
    return Jump(virtualMachine, (size_t)virtualMachine->byteCode[virtualMachine->pc + 1]);
}

err_t Ret(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    err_t popStat = ERR_OK;
    size_t backIndex = (size_t)StackPop(&(virtualMachine->funcStack), &popStat);
    if (popStat != ERR_OK) return ERR_RET;

    *upCode = 1;
    return Jump(virtualMachine, backIndex + 2);
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

err_t AddCom(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    stackElem_t a = 0;
    stackElem_t b = 0;

    if (GetTwoPop(&(virtualMachine->stack), &a, &b) != ERR_OK) {
        printf("Can't get a and b\n");
        return ERR_COMMAND;
    }

    int pushStat = StackPush(&(virtualMachine->stack), (stackElem_t)(a + b));
    if (pushStat != ERR_OK) return ERR_COMMAND;

    *upCode = 1;
    return ERR_OK;
}

err_t SubCom(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    stackElem_t a = 0, b = 0;

    if (GetTwoPop(&(virtualMachine->stack), &a, &b) != ERR_OK) return ERR_COMMAND;

    int pushStat = StackPush(&(virtualMachine->stack), (stackElem_t)(b - a));
    if (pushStat != ERR_OK) return ERR_COMMAND;

    *upCode = 1;
    return ERR_OK;
}

err_t DivCom(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    stackElem_t a = 0, b = 0;

    if (GetTwoPop(&(virtualMachine->stack), &a, &b) != ERR_OK) return ERR_COMMAND;
    if (a == 0) return ERR_COMMAND;

    int pushStat = StackPush(&(virtualMachine->stack), (stackElem_t)((double)b / (double)a));
    if (pushStat != ERR_OK) return ERR_COMMAND;

    *upCode = 1;
    return ERR_OK;
}

err_t MulCom(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    stackElem_t a = 0, b = 0;

    if (GetTwoPop(&(virtualMachine->stack), &a, &b) != ERR_OK) return ERR_COMMAND;

    int pushStat = StackPush(&(virtualMachine->stack), (stackElem_t)(b * a));
    if (pushStat != ERR_OK) return ERR_COMMAND;

    *upCode = 1;
    return ERR_OK;
}

err_t OutCom(VM_resource * virtualMachine, size_t * upCode) {

    ASSERT(virtualMachine);
    ASSERT(upCode);

    int outStat = ERR_OK;
    stackElem_t out = StackPop(&(virtualMachine->stack), &outStat);

    if (outStat != ERR_OK) return ERR_OUT_PROCESSING;

    *upCode = 1;

    printf(BOLD_GREEN "Output: %lg" RESET "\n", (double)out);
    
    return ERR_OK;
}
//------------------------------------------------------------------

size_t GetByteCode(int ** byteCode, const char * byteCodeFilename) {

    ASSERT(byteCode);
    ASSERT(byteCodeFilename);

    FileStat binFile = {}; // rename

    //if (!GetAsmFilenameStd(argc, argv, &txtFile)) ASSERT(0);
    if (!ReadTxtFile(byteCodeFilename, "rb", &binFile)) ASSERT(0);

    *byteCode = (int *)binFile.text_ptr;
    
    return (int)(binFile.sizeInBytes / sizeof(int));

}

//-----------------------------------------------------------------
// DEBUG
//-----------------------------------------------------------------

void VmDump(FILE * stream, VM_resource * virtualMachine, size_t upCode) {

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

        if (i >= virtualMachine->pc && i < virtualMachine->pc + upCode) {

            printf(BOLD_RED "[%2d] " RESET, i);

        }

        if (i >= virtualMachine->pc + upCode) {
            printf(" %2d  ", i);
        }

    }
    printf("\n");

    for (size_t i = 0; i < virtualMachine->byteCodeLen; i++) {

        if (i < virtualMachine->pc) {
            printf("[%2d] ", virtualMachine->byteCode[i]);
        }

        if (i >= virtualMachine->pc && i < virtualMachine->pc + upCode) {

            printf(BOLD_RED "[%2d] " RESET, virtualMachine->byteCode[i]);

        }

        if (i >= virtualMachine->pc + upCode) {
            printf("[%2d] ", virtualMachine->byteCode[i]);
        }
    }
    printf("\n");
    printf(YELLOW "\n-------------------------------------------------------------------------\n" RESET);
    printf("\n");

}

// IO functions ----------------------------------------------------
err_t GetBCfilenameStd(int argc, char * argv[], char * readFromFile) { // argv parser 

    ASSERT(argv);
    ASSERT(readFromFile);

    if ((argc == 3) && (!strcmp(argv[1], CMD_FLAG_BYTECODE))) {

        if (!argv[2]) return ERR_FILEOPEN;

        //! User put bytecode filename, so processor must use this bytecode
        size_t len = strlen(argv[2]);
        memcpy(readFromFile, argv[2], len + 1);  // +1 for '\0' // extra action 

        printf(YELLOW "Executable file: <%s>\n" RESET, argv[2]);

        return ERR_OK;
    }

    printf("\nError of reading stdin\n");
    return ERR_FILEOPEN;
}