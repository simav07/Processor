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

struct AsmFilesInfo {
    char * readFrom = NULL;
    char * writeTo  = NULL;
};

struct Marks {
    int Label[20] = {0};
    size_t nMark  = 0;
};


//! Flag for user's filename of asm source
const char * CMD_FLAG_ASM = "--asmsource";

//! Default filename for writing a byte code 
// const char * PROG_BYTECODE_FILE = "pbytecode.txt";
const char * PROG_BYTECODE_FILE = "bcode.bin";

//! Max command length for commands (bad)
const int MAX_COMMAND_LEN = 20;

const int POISON_VALUE = INT32_MAX;

//! -------------------------------------------------------------------------
//! Functions for ASSEMBLER
//! -------------------------------------------------------------------------

err_t GetAsmFilenameStd(int argc, char * argv[], AsmFilesInfo * fileInfo);

err_t Assemble(const char * asmSourceName, const char * byteCodeName);

err_t GetCommandCode(Marks * label, const char * command, size_t commandSize, uint8_t * commandCode, 
                                           const char * arg, size_t argSize, int * newValue);
int GetRegCode(const char * arg2, size_t arg2Len);

err_t MakeMark(Marks * label, const char * arg1, const size_t currIndex);
err_t WriteMarkIndex(Marks * label, const char * arg2, int * newValue);

//! Write commands to the file with byte code
err_t WriteHashCom(const char * source, const char * commandsFile);
size_t DjbHash(const void *data, size_t size);

err_t WriteArguments(FILE * fileSource, int args, uint8_t newCommandCode, int newElem);
err_t PrepareAsmSource(FileStat * fAsmSource, const char * asmSourceName, const char * byteCodeName);
err_t GetCodeArguments(FileStat * fAsmSource, int i, int * nArgs,
                                                     int * newElem, uint8_t * newCommandCode, Marks * label);
//int CheckSecondArg(const char * arg2, size_t arg2Len);

err_t FGetInfoBuffer(const char * fileName, int ** infoBuffer, FileStat * txtFile);

int main(const int argc, char * argv[]) {

    printf("\nFile <%s>, [%s]\n", __FILE__, __TIME__);

    AsmFilesInfo asmInfo = {};

    int filenamesStat = GetAsmFilenameStd(argc, argv, &asmInfo);

    //! Asm source -> byte code
    int asmStat = Assemble(asmInfo.readFrom, asmInfo.writeTo);

    if (asmStat != ERR_OK) {
        // error description
        printf("ERR_ASM\n");
        return ERR_ASM;
    }

    // if (strcmp(asmInfo.readFrom, USER_ASM_SOURCE_FILE) != 0) ASSERT(0);

    printf(BOLD_GREEN "Successful assembling.\n" RESET);
    printf(YELLOW "File with bytecode: <%s>\n" RESET, asmInfo.writeTo);
    
    return true;
}

err_t GetAsmFilenameStd(int argc, char * argv[], AsmFilesInfo * fileInfo) {

    ASSERT(argv);
    ASSERT(*argv);

    // add more if
    if ((argc == 3) && (!strcmp(argv[1], CMD_FLAG_ASM))) {

        if (!argv[2]) return false;

        //! User put asm filename, so assembler must make a bytecode

        (*fileInfo).readFrom = argv[2];
        (*fileInfo).writeTo  = (char *)PROG_BYTECODE_FILE;

        return true;
    }

    printf("\nError of reading stdin\n");
    return false;
}

err_t Assemble(const char * asmSourceName, const char * byteCodeName) {

    ASSERT(asmSourceName);
    ASSERT(byteCodeName);
    ASSERT((byteCodeName != asmSourceName));

    FileStat fAsmSource = {};

    Marks Label = {};

    err_t prepareAsmStat = PrepareAsmSource(&fAsmSource, asmSourceName, byteCodeName);
    if (prepareAsmStat != ERR_OK) {
        printf("Error of reading asm source\n");
        return ERR_ASM;
    }

    
    FILE * byteCodeFile  = fopen(byteCodeName, "w");

    for (size_t i = 0; i < fAsmSource.nLines; i++) {

        if (fAsmSource.index->length > MAX_COMMAND_LEN) return ERR_OVERFLOW; //! overflow protect
        char command[MAX_COMMAND_LEN] = "";
        int newElem = 0;
        int nArgs = 0;

        uint8_t newCommandCode = 0;

        err_t getArgStat = GetCodeArguments(&fAsmSource, i, &nArgs, &newElem, &newCommandCode, &Label);
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
    fclose(byteCodeFile);

    printf("\nLABEL WITH MARKS: \n");
    for (size_t i = 0; i <= Label.nMark; i++) {
        printf("[%d] ", Label.Label[i]);
    }
    printf("\n");

    return ERR_OK;
}

err_t PrepareAsmSource(FileStat * fAsmSource, const char * asmSourceName, const char * byteCodeName) {

    ASSERT(fAsmSource);

    int * asmSource = NULL;
    int getAsmStat = FGetInfoBuffer(asmSourceName, &asmSource, fAsmSource);

    if (getAsmStat != ERR_OK) {
        return ERR_ASM;
    }
    ASSERT(asmSource);

    return ERR_OK;
}

err_t WriteArguments(FILE * fileSource, int args, uint8_t newCommandCode, int newElem) {

    ASSERT(fileSource);

    if (newCommandCode == SKIP_STEP) return ERR_OK;

    if ((args == 1) || (args == 2 && newElem == POISON_VALUE)) {  // real command without argument

        fprintf(fileSource, "%d\n", newCommandCode);
        return ERR_OK;

    }

    if (args == 2) {  // real command with one argument

        fprintf(fileSource, "%d\t%d\n", newCommandCode, newElem);
        return ERR_OK;

    }

    return ERR_INTPR;

}

err_t GetCodeArguments(FileStat * fAsmSource, int i, int * nArgs,
                                                     int * newElem, uint8_t * newCommandCode, Marks * label) {

    // buffer overflow protect
    if (fAsmSource->index->length > MAX_COMMAND_LEN) return ERR_OVERFLOW;

    char arg1[MAX_COMMAND_LEN] = "";
    char arg2[MAX_COMMAND_LEN] = "";

    *nArgs = sscanf(fAsmSource->index[i].index, "%s %s", arg1, arg2);

    if (*nArgs != 1 && *nArgs != 2) return ERR_COMMAND;

    // Write a mark to the label
    if (arg1[0] == ':') {

        err_t makeMarkStat = MakeMark(label, arg1, i);
        if (makeMarkStat != ERR_OK) {
            return ERR_ASM;
        }

        *newCommandCode = SKIP_STEP;

        return ERR_OK;
    }

    err_t interpreterStat = GetCommandCode(label, arg1, strlen(arg1),
                            newCommandCode, arg2, strlen(arg2), newElem);

    if (interpreterStat != ERR_OK) {
    
        printf(BOLD_RED "Translation error: file <%s>, line %d\n"
                        "Incorrect command\n" RESET, fAsmSource->readFrom, i + 1);
        return ERR_INTPR;
    
    }

    return ERR_OK;

}

int GetRegCode(const char * arg2, size_t arg2Len) {

    size_t hashReg = DjbHash(arg2, arg2Len);

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

    //! Write to the JUMP's argument value of this mark index in byteCode
    *newValue = label->Label[index];

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

err_t GetCommandCode(Marks * label, const char * command, size_t commandSize, uint8_t * commandCode, 
                                           const char * arg, size_t argSize, int * newValue) {

    size_t hashCommand = DjbHash((const void *)command, commandSize);

    switch(hashCommand) {

    case (HASH_PUSH): {
        if (strcmp(command, "PUSH") != 0) return ERR_INTPR;
        
        if (!isalpha(arg[0])) {
            *commandCode = PUSH_C;
            char * checkPtr;
            *newValue = strtod(arg, &checkPtr);
            if (arg == checkPtr) {
                printf("Error of reading a number\n");
                return ERR_SYNTAX;
            }
            return ERR_OK;
        }

        int getRegCodeStat = GetRegCode(arg, argSize);

        *commandCode = PUSH_R;
        
        if (getRegCodeStat == ERR_REGISTER) return ERR_REGISTER;

        *newValue = getRegCodeStat; // write a number of register to new Value

        return ERR_OK;
    }

    case (HASH_POP): {
        if (strcmp(command, "POP") != 0) return ERR_INTPR;

        int getRegCodeStat = GetRegCode(arg, argSize);

        *commandCode = POP_R;
        
        if (getRegCodeStat == ERR_REGISTER) return ERR_REGISTER;

        *newValue = getRegCodeStat; // write a number of register to new Value

        return ERR_OK;
    }

    case (HASH_JUMP): {
        if (strcmp(command, "JUMP") != 0) return ERR_INTPR;

        err_t markStat = WriteMarkIndex(label, arg, newValue);
        if (markStat != ERR_OK) return ERR_MARK;

        *commandCode = JUMP_C;

        return ERR_OK;
    }
    
    case (HASH_ADD): {
        if (strcmp(command, "ADD") != 0) return ERR_INTPR;
        *commandCode = ADD_C;
        return ERR_OK;
    }
    case (HASH_SUB):
        if (strcmp(command, "SUB") != 0) return ERR_INTPR;
        *commandCode = SUB_C;
        return ERR_OK;

    case (HASH_DIV):
        if (strcmp(command, "DIV") != 0) return ERR_INTPR;
        *commandCode = DIV_C;
        return ERR_OK;

    case (HASH_OUT):
        if (strcmp(command, "OUT") != 0) return ERR_INTPR;
        *commandCode = OUT_C;
        return ERR_OK;
    
    case (HASH_HLT):
        if (strcmp(command, "HLT") != 0) return ERR_INTPR;
        *commandCode = HLT_C;
        return ERR_OK;

    case (HASH_MUL):
        if (strcmp(command, "MUL") != 0) return ERR_INTPR;
        *commandCode = MUL_C;
        return ERR_OK;

    default:
        return ERR_INTPR;
    }

    return ERR_INTPR;
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