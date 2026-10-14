#include "include/stack.h"
#include "include/io.h"
#include "include/commands.h"
#include "hashcommands.h"

struct VM_resource {
    stack_t stack = {};
    int * byteCode = NULL;
    size_t byteCodeLen = 0;
    
    size_t pc = 0;
    bool pcChanged = false;

    //! Registers
    stackElem_t regstr[5] = {};
};

//! Flag for byte code filename
const char * CMD_FLAG_BYTECODE = "--bytecode";

//! #define VMDump for debug

//! File for StackDump, there we can see what happening with stack
const char * FILE_STACKDUMP = "dump.txt";

void VmDump(FILE * stream, VM_resource * virtualMachine, int goOn);

err_t GetBCfilenameStd(int argc, char * argv[], char * readFromFile);

//! Main virtual machine
err_t RunVirtualMachine(VM_resource * virtualMachine);
err_t PrepareByteCode(VM_resource * virtualMachine, const int argc, char * argv[]);

//! Fill byte code buffer from txt

size_t GetByteCode(int ** byteCode, const char * byteCodeFilename);

size_t ReadByteCode(const FileStat * txtFile, int * codeBuffer);

//! Interpreter: run commands
err_t Interpreter(VM_resource * virtualMachine, int index);

//! Commands for virtual machine -------------------------------------
err_t PushCom(int * byteCode, stack_t * stack, int index);

err_t GetTwoPop(stack_t * stack, stackElem_t * a, stackElem_t * b);

err_t PopR(VM_resource * virtualMachine); // pushRegister
err_t PushR(VM_resource * virtualMachine, int nRegister);

err_t Jump(VM_resource * virtualMachine, int index);
err_t JA(VM_resource * virtualMachine, int index);
err_t JAE(VM_resource * virtualMachine, int index);
err_t JB(VM_resource * virtualMachine, int index);
err_t JBE(VM_resource * virtualMachine, int index);
err_t JE(VM_resource * virtualMachine, int index);
err_t JNE(VM_resource * virtualMachine, int index);

err_t AddCom(stack_t * stack);
err_t SubCom(stack_t * stack);
err_t DivCom(stack_t * stack);
err_t MulCom(stack_t * stack);

stackElem_t OutCom(stack_t * stack);
// -------------------------------------------------------------------

int main(const int argc, char * argv[]) {

    printf("\nFile <%s>, [%s]\n", __FILE__, __TIME__);

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

    ASSERT(virtualMachine);
    ASSERT(argv);

    char byteCodeFile[100] = "";

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

    virtualMachine->pc = 0;

    int ctorStat = StackCtor(&(virtualMachine->stack), 10);
    if (ctorStat != ERR_OK) return ERR_CTOR;

    FILE * file_p = fopen(FILE_STACKDUMP, "w");

    stackElem_t outResult = 0;

    while (virtualMachine->byteCode[virtualMachine->pc] != HLT_C) {

        if (virtualMachine->pc > virtualMachine->byteCodeLen) {
            printf(BOLD_RED "In asm file no HLT command\n" RESET);
            return ERR_PROCESSING;
        }

        if (virtualMachine->byteCode[virtualMachine->pc] == OUT_C) {
            stackElem_t out = OutCom(&(virtualMachine->stack));

            if (out == ERR_COMMAND) return ERR_OUT_PROCESSING;

            outResult = out;
            virtualMachine->pc++;

            printf(BOLD_GREEN "Output: %lg" RESET "\n", outResult);
            
            continue;

        }

        virtualMachine->pcChanged = false;

        int goOn = Interpreter(virtualMachine, virtualMachine->pc);

        if (goOn == ERR_COMMAND) {
            printf(RED "ERROR_PROCESSING\n" RESET);
            return ERR_PROCESSING;
        }

        #ifdef VMDump
        VmDump(stdout, virtualMachine, goOn);
        getchar();
        #endif

        if (virtualMachine->pcChanged == false) {
            virtualMachine->pc = virtualMachine->pc + goOn;
        }
    }
    fclose(file_p);

    return ERR_OK;
}

err_t Interpreter(VM_resource * virtualMachine, int index) {

    ASSERT(virtualMachine);

    switch (virtualMachine->byteCode[index])
    {
    // PUSH
    case (PUSH_C):
        return PushCom(virtualMachine->byteCode, &(virtualMachine->stack), index);

    case (POP_R):
        return PopR(virtualMachine);

    case (PUSH_R):
        return PushR(virtualMachine, virtualMachine->byteCode[index + 1]);

    case (JUMP_C):
        return Jump(virtualMachine, index);

    case (JA_C):
        return JA(virtualMachine, index);

    case (JAE_C):
        return JAE(virtualMachine, index);

    case (JB_C):
        return JB(virtualMachine, index);

    case (JBE_C):
        return JBE(virtualMachine, index);

    case (JE_C):
        return JE(virtualMachine, index);
        
    case (JNE_C):
        return JNE(virtualMachine, index);
    
    // ADD (+)
    case (ADD_C):
        return AddCom(&(virtualMachine->stack));

    // SUB (-)
    case (SUB_C):
        return SubCom(&(virtualMachine->stack));

    // DIV (/)
    case (DIV_C):
        return DivCom(&(virtualMachine->stack));

    // MUL (*)
    case (MUL_C):
        return MulCom(&(virtualMachine->stack));

    // OUT
    case (OUT_C):
        return OutCom(&(virtualMachine->stack));

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

err_t PopR(VM_resource * virtualMachine) {

    // ASSERT(virtualMachine);
    // ASSERT(virtualMachine->byteCode);
    // ASSERT(virtualMachine->regstr);
    // ASSERT(virtualMachine->stack);

    err_t popStat = 0;
    stackElem_t var = StackPop(&(virtualMachine->stack), &popStat);

    if (popStat != ERR_OK) return ERR_COMMAND;

    virtualMachine->regstr[virtualMachine->byteCode[virtualMachine->pc + 1]] = var;

    return 2;
}

err_t PushR(VM_resource * virtualMachine, int nRegister) {

    ASSERT(virtualMachine);

    err_t pushStat = StackPush(&(virtualMachine->stack), virtualMachine->regstr[nRegister]);

    if (pushStat != ERR_OK) return ERR_PUSH_COM;

    return 2;
}

err_t Jump(VM_resource * virtualMachine, int index) {

    int jumpIndex = virtualMachine->byteCode[index + 1];

    virtualMachine->pc = jumpIndex;
    virtualMachine->pcChanged = true;

    return 2;

}

err_t JA(VM_resource * virtualMachine, int index) {

    err_t popStat1 = 0;
    err_t popStat2 = 0;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);

    if (a > b) {
        return Jump(virtualMachine, index);
    }

    return 2;
}

err_t JAE(VM_resource * virtualMachine, int index) {

    err_t popStat1 = 0;
    err_t popStat2 = 0;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);

    if (a >= b) {
        return Jump(virtualMachine, index);
    }

    return 2;
}

err_t JB(VM_resource * virtualMachine, int index) {

    err_t popStat1 = 0;
    err_t popStat2 = 0;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);

    if (a < b) {
        return Jump(virtualMachine, index);
    }

    return 2;
}

err_t JBE(VM_resource * virtualMachine, int index) {

    err_t popStat1 = 0;
    err_t popStat2 = 0;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);

    if (a <= b) {
        return Jump(virtualMachine, index);
    }

    return 2;
}

err_t JE(VM_resource * virtualMachine, int index) {

    err_t popStat1 = 0;
    err_t popStat2 = 0;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);

    if (a == b) {
        return Jump(virtualMachine, index);
    }

    return 2;
}

err_t JNE(VM_resource * virtualMachine, int index) {

    err_t popStat1 = 0;
    err_t popStat2 = 0;

    stackElem_t b = StackPop(&(virtualMachine->stack), &popStat1);
    stackElem_t a = StackPop(&(virtualMachine->stack), &popStat2);

    if (a != b) {
        return Jump(virtualMachine, index);
    }

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

err_t MulCom(stack_t * stack) {

    stackElem_t a = 0, b = 0;

    if (GetTwoPop(stack, &a, &b) != ERR_OK) return ERR_COMMAND;

    int pushStat = StackPush(stack, (stackElem_t)(b * a));

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

size_t GetByteCode(int ** byteCode, const char * byteCodeFilename) {

    ASSERT(byteCode);

    FileStat txtFile = {};

    //if (!GetAsmFilenameStd(argc, argv, &txtFile)) ASSERT(0);
    if (!ReadTxtFile(byteCodeFilename, "rb", &txtFile)) ASSERT(0);

    int * newBuffer = (int *)calloc(txtFile.nLines * 2, sizeof(int));

    if (!newBuffer) {
        printf("ERR_CALLOC: newBuffer error\n");
        return ERR_BYTECODE;
    }

    *byteCode = newBuffer;

    size_t codeBytesLen = ReadByteCode(&txtFile, *byteCode);

    if (codeBytesLen == ERR_BYTECODE) {
        printf("ERR_BYTECODE\n");
        return ERR_BYTECODE;
    }

    return codeBytesLen;

}

size_t ReadByteCode(const FileStat * txtFile, int * codeBuffer) {

    ASSERT(txtFile);
    ASSERT(codeBuffer);

    // Count of elements that are writing to codeBuffer
    size_t count = 0;

    for (size_t i = 0; i < txtFile->nLines; i++) {

        int command = -1; // -1 using only for init, there's no real commands with code that equals -1
        int newElem = 0;

        int args = sscanf(txtFile->index[i].index, "%d %d", &command, &newElem);

        if (args == EOF || args == 0) {
            // if theres an empty string
            continue; 
        }

        if ((args == 1) && (command >= 0)) {  // real command without argument
            codeBuffer[count] = command;
            count++;
        }

        else if ((args == 2) && (command >= 0)) {  // real command with one argument
            codeBuffer[count++] = command;
            codeBuffer[count++] = newElem;
        }

        else return ERR_BYTECODE;

    }
    
    return count;
    
}

//-----------------------------------------------------------------
// DEBUG
//-----------------------------------------------------------------

void VmDump(FILE * stream, VM_resource * virtualMachine, int goOn) {

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

        if (i >= virtualMachine->pc && i < virtualMachine->pc + goOn) {

            printf(BOLD_RED "[%2d] " RESET, i);

        }

        if (i >= virtualMachine->pc + goOn) {
            printf(" %2d  ", i);
        }

    }
    printf("\n");

    for (size_t i = 0; i < virtualMachine->byteCodeLen; i++) {

        if (i < virtualMachine->pc) {
            printf("[%2d] ", virtualMachine->byteCode[i]);
        }

        if (i >= virtualMachine->pc && i < virtualMachine->pc + goOn) {

            printf(BOLD_RED "[%2d] " RESET, virtualMachine->byteCode[i]);

        }

        if (i >= virtualMachine->pc + goOn) {
            printf("[%2d] ", virtualMachine->byteCode[i]);
        }
    }
    printf("\n");
    printf(YELLOW "\n-------------------------------------------------------------------------\n" RESET);
    printf("\n");

}

// IO functions ----------------------------------------------------
err_t GetBCfilenameStd(int argc, char * argv[], char * readFromFile) {
    ASSERT(argv);
    ASSERT(readFromFile);

    if ((argc == 3) && (!strcmp(argv[1], CMD_FLAG_BYTECODE))) {

        if (!argv[2]) return ERR_FILEOPEN;

        //! User put bytecode filename, so processor must use this bytecode
        size_t len = strlen(argv[2]);
        memcpy(readFromFile, argv[2], len + 1);  // +1 for '\0'

        printf(YELLOW "Executable file: <%s>\n" RESET, argv[2]);

        return ERR_OK;
    }

    printf("\nError of reading stdin\n");
    return ERR_FILEOPEN;
}