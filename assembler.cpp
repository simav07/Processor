typedef int err_t;

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>

#include "./include/errors.h"
#include "./include/colors.h"
#include "./include/io.h"
#include "./include/commands.h"
#include "hashcommands.h"

struct Marks {
    int Label[20] = {0};
    size_t nMark  = 0;
};

struct Assembler {
    char * readFrom = NULL;
    char * writeTo  = NULL;

    FileStat fAsmSource = {};

    Marks label = {};

};

struct Command {
    char * commandPtr;
    size_t commandSize;
};

struct Argument {
    char * argPtr;
    size_t argSize;
};

/*
struct AsmFilesInfo {
    char * readFrom = NULL;
    char * writeTo  = NULL;
};

struct Marks {
    int Label[1000] = {0};
    size_t nMark  = 0;
}; */

// struct Assembler

//! Flag for user's filename of asm source
const char * CMD_FLAG_ASM = "--asmsource";
//! Flag for user's filename of byte code
const char * CMD_FLAG_BC  = "--bytecode";

//! Default filename for writing a byte code 
const char * PROG_BYTECODE_FILE = "bcode.bin";

char * BYTECODE_FILENAME = NULL;

//! Max command length for commands (bad)
const int MAX_COMMAND_LEN = 20;

const int POISON_VALUE = INT32_MAX;

//! -------------------------------------------------------------------------
//! Functions for ASSEMBLER
//! -------------------------------------------------------------------------

err_t AsmCtor(const int argc, const char * argv[], Assembler * assembler);

err_t GetAsmFilenameStd(const int argc, const char * argv[], Assembler * fileInfo);

err_t Assemble(Assembler * assembler);
                                    // mother fucker struct <...>
err_t GetCommandCode(Marks * label, Command * command, int32_t * commandCode,
                                  Argument * argument, int32_t * newValue);
int GetRegCode(Argument * arg2);

err_t WriteJumpMark(Marks * label, const char * arg, int * newValue, int32_t * commandCode);
err_t MakeMark(Marks * label, const char * arg1, const size_t currIndex);
err_t WriteMarkIndex(Marks * label, const char * arg2, int * newValue);

//! Write commands to the file with byte code
size_t DjbHash(const void * data, size_t size);

err_t WriteArguments(FILE * fileSource, int args, int32_t newCommandCode, int newElem);
err_t PrepareAsmSource(FileStat * fAsmSource, const char * asmSourceName, const char * byteCodeName);
err_t GetCodeArguments(Assembler * assembler, size_t * count, int i, int * nArgs,
                                              int32_t * newElem, int32_t * newCommandCode);

err_t FGetInfoBuffer(const char * fileName, int ** infoBuffer, FileStat * txtFile);

int main(const int argc, const  char * argv[]) {

    printf("\nFile <%s>, [%s]\n", __FILE__, __TIME__);

    FileStat fAsmSource = {};

    Assembler assembler = {};

    int ctorStat = AsmCtor(argc, argv, &assembler);
    if (ctorStat != ERR_OK) {
        // err description
        printf("Error from constructor\n");
        return false;
    }

    //! Asm source -> byte code
    int asmStat = Assemble(&assembler);

    if (asmStat != ERR_OK) {
        // error description
        printf("ERR_ASM\n");
        return ERR_ASM;
    }

    printf(BOLD_GREEN "Successful assembling.\n" RESET);
    printf(YELLOW "File with bytecode: <%s>\n" RESET, assembler.writeTo);
    
    return true;
}

err_t AsmCtor(const int argc, const char * argv[], Assembler * assembler) {

    ASSERT(argv);
    ASSERT(assembler);

    if (!GetAsmFilenameStd(argc, argv, assembler)) return ERR_ASM_CTOR;

    // FileStat fAsmSource = {};

    err_t prepareAsmStat = PrepareAsmSource(&(assembler->fAsmSource), assembler->readFrom, assembler->writeTo);
    if (prepareAsmStat != ERR_OK) {
        printf("Error of reading asm source\n");
        return ERR_ASM_CTOR;
    }

    return ERR_OK;

}

err_t GetAsmFilenameStd(const int argc, const char * argv[], Assembler * fileInfo) {

    ASSERT(argv);
    ASSERT(*argv);
    ASSERT(fileInfo);

    // add more if
    if (((argc == 3) || (argc == 5)) && (!strcmp(argv[1], CMD_FLAG_ASM))) {

        if (!argv[2]) return false;

        //! User put asm filename, so assembler must make a bytecode

        (*fileInfo).readFrom = (char *)argv[2];

        //! User's filename
        if ((argc == 5) && !strcmp(argv[3], CMD_FLAG_BC)) {
            (*fileInfo).writeTo = (char *)argv[4];
            return true;
        }

        //! Else - default filename
        (*fileInfo).writeTo  = (char *)PROG_BYTECODE_FILE;

        return true;
    }

    printf("\nError of reading stdin\n");
    return false;
}

err_t Assemble(Assembler * assembler) {

    ASSERT(assembler);
    
    for (int i = 0; i < 2; i++) {

    FILE * byteCodeFile  = fopen(assembler->writeTo, "wb");

    printf(YELLOW "%d step of assembling...\n" RESET, i + 1);

    size_t count = 0;

    for (size_t i = 0; i < (assembler->fAsmSource).nLines; i++) {

        if ((assembler->fAsmSource).index->length > MAX_COMMAND_LEN) return ERR_OVERFLOW; //! overflow protect
        
        char command[MAX_COMMAND_LEN] = "";
        int32_t newElem = 0;

        int nArgs = 0;

        int32_t newCommandCode = 0; // uint8_t

        err_t getArgStat = GetCodeArguments(assembler, &count, i, &nArgs, &newElem, &newCommandCode);
        if (getArgStat != ERR_OK) {
            printf("Error of getting args\n");
            return ERR_INTPR;
        }

        err_t writeArgsStat = WriteArguments(byteCodeFile, nArgs, newCommandCode, newElem);
        if (writeArgsStat != ERR_OK) {
            printf("Error of writing arguments to the byte code file\n");
            return ERR_INTPR;
        }

    }

    #ifdef MARKDUMP
    printf("\nLABEL WITH MARKS: \n");
    for (size_t i = 0; i <= Label.nMark; i++) {
        printf("[%d] ", Label.Label[i]);
    }
    printf("\n");
    #endif

    fclose(byteCodeFile);
    }


    return ERR_OK;
}

err_t PrepareAsmSource(FileStat * fAsmSource, const char * asmSourceName, const char * byteCodeName) {

    ASSERT(fAsmSource);
    ASSERT(asmSourceName);
    ASSERT(byteCodeName);

    int * asmSource = NULL;
    int getAsmStat = FGetInfoBuffer(asmSourceName, &asmSource, fAsmSource);

    if (getAsmStat != ERR_OK) {
        return ERR_ASM;
    }
    ASSERT(asmSource);

    return ERR_OK;
}

err_t WriteArguments(FILE * fileSource, int args, int32_t newCommandCode, int newElem) {

    ASSERT(fileSource);

    if (newCommandCode == SKIP_STEP) return ERR_OK;

    if ((args == 1) || (args == 2 && newElem == POISON_VALUE)) {  // real command without argument

        // fprintf(fileSource, "%d\n", newCommandCode);

        if (fwrite(&newCommandCode, sizeof(int32_t), 1, fileSource) != 1) return ERR_ASM;
        
        return ERR_OK;

    }

    if (args == 2) {  // real command with one argument

        if (fwrite(&newCommandCode, sizeof(int32_t), 1, fileSource) != 1) return ERR_ASM;

        int32_t argument = (int32_t)newElem;

        if (fwrite(&argument, sizeof(int32_t), 1, fileSource) != 1) return ERR_ASM;

        // fprintf(fileSource, "%d\t%d\n", newCommandCode, newElem);
        return ERR_OK;

    }

    return ERR_INTPR;

}

err_t GetCodeArguments(Assembler * assembler, size_t * count, int i, int * nArgs,
                                              int32_t * newElem, int32_t * newCommandCode) {
    ASSERT(assembler);
    ASSERT(count);

    // buffer overflow protect
    if (assembler->fAsmSource.index->length > MAX_COMMAND_LEN) return ERR_OVERFLOW;

    char arg1[MAX_COMMAND_LEN] = "";
    char arg2[MAX_COMMAND_LEN] = "";

    *nArgs = sscanf(assembler->fAsmSource.index[i].index, "%s %s", arg1, arg2);

    if (*nArgs != 1 && *nArgs != 2) return ERR_COMMAND;

    // Write a mark to the label
    if (arg1[0] == ':') {

        err_t makeMarkStat = MakeMark(&(assembler->label), arg1, *count);
        if (makeMarkStat != ERR_OK) {
            printf("Make mark error\n");
            return ERR_ASM;
        }

        *newCommandCode = SKIP_STEP;

        return ERR_OK;
    }

    *count += *nArgs;

    Command command   = {.commandPtr = arg1, .commandSize = strlen(arg1)};
    Argument argument = {.argPtr = arg2, .argSize = strlen(arg2)};

    err_t interpreterStat = GetCommandCode(&(assembler->label), &command, newCommandCode, &argument, newElem);

    if (interpreterStat != ERR_OK) {
    
        printf(BOLD_RED "Translation error: file <%s>, line %d\n"
                        "Incorrect command\n" RESET, assembler->fAsmSource.readFrom, i + 1);
        return ERR_INTPR;
    
    }

    return ERR_OK;

}

int GetRegCode(Argument * arg2) {

    ASSERT(arg2);

    size_t hashReg = DjbHash(arg2->argPtr, arg2->argSize);

    switch (hashReg)
    {
    case (HASH_OX):
        return 0;

    case (HASH_AX):
        return 1;

    case (HASH_BX):
        return 2;

    case (HASH_CX):
        return 3;

    case (HASH_DX):
        return 4;
    
    default:
        return ERR_REGISTER;
        break;
    }

}

err_t MakeMark(Marks * label, const char * arg1, const size_t currIndex) {

    ASSERT(label);
    ASSERT(arg1);

    int nMark = -1;

    int scanStat = sscanf(arg1, ":%d", &nMark);

    if (scanStat != 1) {
        printf("ERROR OF MARK NUMBER READING\n");
        return ERR_MARK;
    }

    label->nMark = nMark;
    label->Label[nMark] = currIndex;

    return ERR_OK;

}

err_t WriteMarkIndex(Marks * label, const char * arg2, int * newValue) {

    ASSERT(label);
    ASSERT(arg2);
    ASSERT(newValue);

    int index = -1; // number of mark
    int indexStat = sscanf(arg2, ":%d", &index);

    if (indexStat != 1) return ERR_SYNTAX;

    int newIndex = label->Label[index];
    if (newIndex == 0) {
        return ERR_SKIP_ARG;
    }

    //! Write to the JUMP's argument value of this mark index in byteCode
    *newValue = newIndex;

    return ERR_OK;

}

err_t FGetInfoBuffer(const char * fileName, int ** infoBuffer, FileStat * txtFile) {

    ASSERT(fileName);
    ASSERT(infoBuffer);

    txtFile->readFrom = (char *)fileName;

    if (!ReadTxtFile(fileName, "r", txtFile)) ASSERT(0);

    int * newBuffer = (int *)calloc(txtFile->nLines * 2, sizeof(int));

    if (!newBuffer) {
        printf("ERR_CALLOC: newBuffer error\n");
        return ERR_CALLOC;
    }

    *infoBuffer = newBuffer;

    return ERR_OK;

}

err_t GetCommandCode(Marks * label, Command * command, int32_t * commandCode, 
                                    Argument * argument, int32_t * newValue) {

    ASSERT(label);
    ASSERT(command);
    ASSERT(commandCode);
    ASSERT(argument);
    ASSERT(newValue);

    size_t hashCommand = DjbHash((const void *)(command->commandPtr), command->commandSize);

    switch(hashCommand) {

    case (HASH_PUSH): {
        if (strcmp(command->commandPtr, "PUSH") != 0) return ERR_INTPR;
        
        if (!isalpha(argument->argPtr[0])) {
            *commandCode = PUSH_C;
            char * checkPtr;
            *newValue = strtod(argument->argPtr, &checkPtr);
            if (argument->argPtr == checkPtr) {
                printf("Error of reading a number\n");
                return ERR_SYNTAX;
            }
            return ERR_OK;
        }

        int getRegCodeStat = GetRegCode(argument);

        *commandCode = PUSH_R;
        
        if (getRegCodeStat == ERR_REGISTER) return ERR_REGISTER;

        *newValue = getRegCodeStat; // write a number of register to new Value

        return ERR_OK;
    }

    case (HASH_POP): {
        if (strcmp(command->commandPtr, "POP") != 0) return ERR_INTPR;

        int getRegCodeStat = GetRegCode(argument);

        *commandCode = POP_R;
        
        if (getRegCodeStat == ERR_REGISTER) return ERR_REGISTER;

        *newValue = getRegCodeStat; // write a number of register to new Value

        return ERR_OK;
    }

    case (HASH_CALL): {

        if (strcmp(command->commandPtr, "CALL") != 0) return ERR_INTPR;

        err_t markStat = WriteMarkIndex(label, argument->argPtr, newValue);

        if (markStat == ERR_SKIP_ARG) {
            *commandCode = SKIP_STEP;
            return ERR_OK;
        }

        if (markStat != ERR_OK) return ERR_MARK;

        *commandCode = CALL_C;

        return ERR_OK;

    }

    case (HASH_RET): {

        if (strcmp(command->commandPtr, "RET") != 0) return ERR_INTPR;

        *commandCode = RET_C;

        return ERR_OK;

    }

    case (HASH_JUMP): {

        if (strcmp(command->commandPtr, "JUMP") != 0) return ERR_INTPR;

        err_t markStat = WriteMarkIndex(label, argument->argPtr, newValue);

        if (markStat == ERR_SKIP_ARG) {
            *commandCode = SKIP_STEP;
            return ERR_OK;
        }

        if (markStat != ERR_OK) return ERR_MARK;

        *commandCode = JUMP_C;

        return ERR_OK;
    }

    case (HASH_JA): {
        if (strcmp(command->commandPtr, "JA") != 0) return ERR_INTPR;

        *commandCode = JA_C;

        return WriteJumpMark(label, argument->argPtr, newValue, commandCode);

    }

    case (HASH_JAE): {
        if (strcmp(command->commandPtr, "JAE") != 0) return ERR_INTPR;

        *commandCode = JAE_C;
        return WriteJumpMark(label, argument->argPtr, newValue, commandCode);

    }

    case (HASH_JB): {
        if (strcmp(command->commandPtr, "JB") != 0) return ERR_INTPR;

        *commandCode = JB_C;
        return WriteJumpMark(label, argument->argPtr, newValue, commandCode);

    }

    case (HASH_JBE): {
        if (strcmp(command->commandPtr, "JBE") != 0) return ERR_INTPR;

        *commandCode = JBE_C;
        return WriteJumpMark(label, argument->argPtr, newValue, commandCode);

    }

    case (HASH_JE): {
        if (strcmp(command->commandPtr, "JE") != 0) return ERR_INTPR;

        *commandCode = JE_C;
        return WriteJumpMark(label, argument->argPtr, newValue, commandCode);

    }

    case (HASH_JNE): {
        if (strcmp(command->commandPtr, "JNE") != 0) return ERR_INTPR;

        *commandCode = JNE_C;
        return WriteJumpMark(label, argument->argPtr, newValue, commandCode);

    }

    case (HASH_IN): {
        if (strcmp(command->commandPtr, "IN") != 0) return ERR_INTPR;

        *commandCode = IN_C;
        return ERR_OK;
    }

    case (HASH_ADD): {
        if (strcmp(command->commandPtr, "ADD") != 0) return ERR_INTPR;
        *commandCode = ADD_C;
        return ERR_OK;
    }
    case (HASH_SUB):
        if (strcmp(command->commandPtr, "SUB") != 0) return ERR_INTPR;
        *commandCode = SUB_C;
        return ERR_OK;

    case (HASH_DIV):
        if (strcmp(command->commandPtr, "DIV") != 0) return ERR_INTPR;
        *commandCode = DIV_C;
        return ERR_OK;

    case (HASH_OUT):
        if (strcmp(command->commandPtr, "OUT") != 0) return ERR_INTPR;
        *commandCode = OUT_C;
        return ERR_OK;
    
    case (HASH_HLT):
        if (strcmp(command->commandPtr, "HLT") != 0) return ERR_INTPR;
        *commandCode = HLT_C;
        return ERR_OK;

    case (HASH_MUL):
        if (strcmp(command->commandPtr, "MUL") != 0) return ERR_INTPR;
        *commandCode = MUL_C;
        return ERR_OK;

    default:
        return ERR_INTPR;
    }

    return ERR_INTPR;
}

err_t WriteJumpMark(Marks * label, const char * arg, int * newValue, int32_t * commandCode) {

    err_t markStat = WriteMarkIndex(label, arg, newValue);

    if (markStat == ERR_SKIP_ARG) {
        *commandCode = SKIP_STEP;
        return ERR_OK;
    }

    if (markStat != ERR_OK) return ERR_MARK;

    return ERR_OK;

}

size_t DjbHash(const void *data, size_t size) {

    ASSERT(data);

    const unsigned char *ptr = (const unsigned char *)data;
    ASSERT(ptr);

    size_t hash = 5381;

    for (size_t i = 0; i < size; i++)
        
        hash = hash * 33 + ptr[i];

    return hash;
}