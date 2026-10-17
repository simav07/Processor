typedef int err_t;

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>

#include "include/errors.h"
#include "include/colors.h"
#include "include/io.h"
#include "include/commands.h"
#include "hashcommands.h"

struct Marks {
    int Label[20] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
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

/**
 * Values near the command like PUSH 20, POP [1], PUSH [BX+30].
 * There is 2 (BX = 2) and 30 is first value and second value
 * Maximum two values near the command
 */
struct ArgValues {
    int32_t value1;
    int32_t value2;
    uint8_t usedArgs; //!< 0, 1 or 2 used arguments in current command
};

//! Flag for user's filename of asm source
const char * CMD_FLAG_ASM = "--asmsource";
//! Flag for user's filename of byte code
const char * CMD_FLAG_BC  = "--bytecode";

//! Default filename for writing a byte code 
const char * PROG_BYTECODE_FILE = "bcode.bin";

char * BYTECODE_FILENAME = NULL;

//! Max command length for commands
const int MAX_COMMAND_LEN = 20;

const int POISON_VALUE = INT32_MAX;

//! -------------------------------------------------------------------------
//! Functions for ASSEMBLER
//! -------------------------------------------------------------------------

err_t AsmCtor(const int argc, const char * argv[], Assembler * assembler);

err_t ArgvParser(const int argc, const char * argv[], Assembler * fileInfo);

err_t Assemble(Assembler * assembler);

err_t GetCommandCode(Marks * label, Command * command, uint8_t * commandCode,
                                  Argument * argument, ArgValues * newValues);
int GetRegCode(Argument * arg2);

err_t GetPushPopCode(uint8_t defaultValue, Command * command, Argument * arg,
                                           uint8_t * commandCode, ArgValues * argValues);

err_t WriteJumpMark(Marks * label, const char * arg, int * newValue, uint8_t * commandCode);
err_t MakeMark(Marks * label, const char * arg1, const size_t currIndex);
err_t WriteMarkIndex(Marks * label, const char * arg2, int * newValue);

//! Write commands to the file with byte code
size_t DjbHash(const void * data, size_t size);

err_t WriteArguments(FILE * fileSource, int args, uint8_t newCommandCode, ArgValues * newValues);
err_t PrepareAsmSource(FileStat * fAsmSource, const char * asmSourceName, const char * byteCodeName);
err_t GetCodeArguments(Assembler * assembler, size_t * count, int i, int * nArgs,
                                              ArgValues * newValues, uint8_t * newCommandCode);

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

    if (!ArgvParser(argc, argv, assembler)) return ERR_ASM_CTOR;

    err_t prepareAsmStat = PrepareAsmSource(&(assembler->fAsmSource), assembler->readFrom, assembler->writeTo);
    if (prepareAsmStat != ERR_OK) {
        printf("Error of reading asm source\n");
        return ERR_ASM_CTOR;
    }

    return ERR_OK;

}

err_t ArgvParser(const int argc, const char * argv[], Assembler * fileInfo) {

    ASSERT(argv);
    ASSERT(*argv);
    ASSERT(fileInfo);

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
        
        ArgValues newValues = {};

        int nArgs = 0;

        uint8_t newCommandCode = 0;

        err_t getArgStat = GetCodeArguments(assembler, &count, i, &nArgs, &newValues, &newCommandCode);
        if (getArgStat != ERR_OK) {
            printf("Error of getting args\n");
            return ERR_ASM;
        }

        err_t writeArgsStat = WriteArguments(byteCodeFile, nArgs, newCommandCode, &newValues);
        if (writeArgsStat != ERR_OK) {
            printf("Error of writing arguments to the byte code file\n");
            return ERR_ASM;
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

err_t WriteArguments(FILE * fileSource, int args, uint8_t newCommandCode, ArgValues * newValues) {

    ASSERT(newValues);
    ASSERT(fileSource);

    if (newCommandCode == SKIP_STEP || args <= 0) return ERR_OK;

    if (args != 1 && args != 2) {
        return ERR_ASM;
    }

    uint8_t argCount = (args == 2) ? 1 : 0;

    // 1st byte: quantity of arguments
    // if (fwrite(&argCount, sizeof(uint8_t), 1, fileSource) != 1) return ERR_ASM;

    //! 1st byte: code of the command
    if (fwrite(&newCommandCode, sizeof(uint8_t), 1, fileSource) != 1) return ERR_ASM;

    //! Next 4 bytes: argument (if its need)
    if (newValues->usedArgs >= 1) {

        if (fwrite(&newValues->value1, sizeof(int32_t), 1, fileSource) != 1) return ERR_ASM;

    }

    if (newValues->usedArgs == 2) {

        if (fwrite(&newValues->value2, sizeof(int32_t), 1, fileSource) != 1) return ERR_ASM;

    }

    return ERR_OK;

}

err_t GetCodeArguments(Assembler * assembler, size_t * count, int i, int * nArgs,
                                              ArgValues * newValues, uint8_t * newCommandCode) {
    ASSERT(assembler);
    ASSERT(count);

    // buffer overflow protect
    if (assembler->fAsmSource.index->length > MAX_COMMAND_LEN) return ERR_OVERFLOW;

    char arg1[MAX_COMMAND_LEN] = "";
    char arg2[MAX_COMMAND_LEN] = "";

    *nArgs = sscanf(assembler->fAsmSource.index[i].index, "%s %s", arg1, arg2);

    if (*nArgs <= 0) return ERR_OK;

    if (*nArgs != 1 && *nArgs != 2) return ERR_COMMAND;

    //! Comments
    if (arg1[0] == ';') {
        *newCommandCode = SKIP_STEP;
        return ERR_OK;
    }

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

    // *count += 2 + ((*nArgs == 2) ? sizeof(int32_t) : 0); // !!!

    Command command   = {.commandPtr = arg1, .commandSize = strlen(arg1)};
    Argument argument = {.argPtr = arg2, .argSize = strlen(arg2)};

    err_t interpreterStat = GetCommandCode(&(assembler->label), &command, newCommandCode, &argument, newValues);

    if (interpreterStat != ERR_OK) {
    
        printf(BOLD_RED "Translation error: file <%s>, line %d\n"
                        "Incorrect command\n" RESET, assembler->fAsmSource.readFrom, i + 1);
        return ERR_INTPR;
    
    }

    //! Count in bytes
    *count += 1 + (newValues->usedArgs * sizeof(int32_t));

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

    if (nMark < 0 || nMark >= 20) return ERR_MARK;

    label->nMark = nMark;
    label->Label[nMark] = (int)currIndex;

    return ERR_OK;

}

err_t WriteMarkIndex(Marks * label, const char * arg2, int * newValue) {

    // -------------- TODO ------------- GetLabel, SetLabel

    ASSERT(label);
    ASSERT(arg2);
    ASSERT(newValue);

    int index = -1; // number of mark
    int indexStat = sscanf(arg2, ":%d", &index);

    if (indexStat != 1) return ERR_SYNTAX;

    if (index < 0 || index >= 20) return ERR_MARK;

    int newIndex = label->Label[index];
    if (newIndex < 0) {
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

err_t GetCommandCode(Marks * label, Command * command, uint8_t * commandCode, 
                                    Argument * argument, ArgValues * newValues) {

    ASSERT(label);
    ASSERT(command);
    ASSERT(commandCode);
    ASSERT(argument);
    ASSERT(newValues);

    size_t hashCommand = DjbHash((const void *)(command->commandPtr), command->commandSize);

    switch(hashCommand) {

    case (HASH_PUSH): {
        if (strcmp(command->commandPtr, "PUSH") != 0) return ERR_INTPR;

        return GetPushPopCode(PUSH_C, command, argument, commandCode, newValues);
        
    }

    case (HASH_POP): {
        if (strcmp(command->commandPtr, "POP") != 0) return ERR_INTPR;

        return GetPushPopCode(POP_C, command, argument, commandCode, newValues);
    }

    case (HASH_CALL): {

        if (strcmp(command->commandPtr, "CALL") != 0) return ERR_INTPR;

        err_t markStat = WriteMarkIndex(label, argument->argPtr, &(newValues->value1));

        if (markStat == ERR_SKIP_ARG) {
            newValues->value1 = 0;
        }
        else if (markStat != ERR_OK) {
            return ERR_MARK;
        }

        *commandCode = CALL_C | 0x20;
        newValues->usedArgs = 1;

        return ERR_OK;

    }

    case (HASH_RET): {

        if (strcmp(command->commandPtr, "RET") != 0) return ERR_INTPR;

        *commandCode = RET_C;

        return ERR_OK;

    }

    case (HASH_JUMP): {

        if (strcmp(command->commandPtr, "JUMP") != 0) return ERR_INTPR;

        err_t markStat = WriteMarkIndex(label, argument->argPtr, &(newValues->value1));

        if (markStat == ERR_SKIP_ARG) {
            *commandCode = SKIP_STEP;
            return ERR_OK;
        }

        if (markStat != ERR_OK) return ERR_MARK;

        *commandCode = JUMP_C | 0x20;
        newValues->usedArgs = 1;

        return ERR_OK;
    }

    case (HASH_JA): {
        if (strcmp(command->commandPtr, "JA") != 0) return ERR_INTPR;

        *commandCode = JA_C | 0x20;
        newValues->usedArgs = 1;

        return WriteJumpMark(label, argument->argPtr, &newValues->value1, commandCode);

    }

    case (HASH_JAE): {
        if (strcmp(command->commandPtr, "JAE") != 0) return ERR_INTPR;

        *commandCode = JAE_C | 0x20;
        newValues->usedArgs = 1;

        return WriteJumpMark(label, argument->argPtr, &newValues->value1, commandCode);

    }

    case (HASH_JB): {
        if (strcmp(command->commandPtr, "JB") != 0) return ERR_INTPR;

        newValues->usedArgs = 1;
        *commandCode = JB_C | 0x20;
        return WriteJumpMark(label, argument->argPtr, &newValues->value1, commandCode);

    }

    case (HASH_JBE): {
        if (strcmp(command->commandPtr, "JBE") != 0) return ERR_INTPR;

        newValues->usedArgs = 1;
        *commandCode = JBE_C | 0x20;
        return WriteJumpMark(label, argument->argPtr, &newValues->value1, commandCode);

    }

    case (HASH_JE): {
        if (strcmp(command->commandPtr, "JE") != 0) return ERR_INTPR;

        newValues->usedArgs = 1;
        *commandCode = JE_C | 0x20;
        return WriteJumpMark(label, argument->argPtr, &newValues->value1, commandCode);

    }

    case (HASH_JNE): {
        if (strcmp(command->commandPtr, "JNE") != 0) return ERR_INTPR;

        newValues->usedArgs = 1;
        *commandCode = JNE_C | 0x20;
        return WriteJumpMark(label, argument->argPtr, &newValues->value1, commandCode);

    }

    case (HASH_IN): {
        if (strcmp(command->commandPtr, "IN") != 0) return ERR_INTPR;

        newValues->usedArgs = 0;
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

err_t GetPushPopCode(uint8_t defaultValue, Command * command, Argument * arg,
                                           uint8_t * commandCode, ArgValues * argValues) {

    ASSERT(command);
    ASSERT(arg);
    ASSERT(commandCode);
    ASSERT(argValues);
    ASSERT(arg->argPtr);

    uint8_t defaultMask = defaultValue;

    uint8_t constMask = 0x20;
    uint8_t regMask   = 0x40;
    uint8_t ramMask   = 0x80;

    // push 10
    // push AX
    // push AX+10
    // push [10]
    // push [AX+10]

    int bytesArg = 0;
    int32_t newConst = 0;

    // Constant
    if (defaultValue != POP_C && sscanf(arg->argPtr, "%d%n", &newConst, &bytesArg) == 1
                               && arg->argPtr[bytesArg] == '\0') {

        *commandCode = (defaultMask | constMask);
        argValues->value1 = newConst;
        argValues->usedArgs = 1;

        return ERR_OK;
    }

    //! For registers -----------------------------------------
    char regName[3] = "";
    bytesArg = 0;

    if (sscanf(arg->argPtr, "%2[A-Z]%n", regName, &bytesArg) == 1 && bytesArg == 2) {
        if (arg->argPtr[bytesArg] == '\0') {

            Argument regArg = {.argPtr = regName, .argSize = strlen(regName)};
            int regCode = GetRegCode(&regArg);
            if (regCode == ERR_REGISTER) return ERR_REGISTER;

            *commandCode = (defaultMask | regMask);

            argValues->value1 = regCode;
            argValues->usedArgs = 1;

            return ERR_OK;
        }
    }
    //! --------------------------------------------------------------

    //! For ram ------------------------------------------------------
    bytesArg = 0;
    if (sscanf(arg->argPtr, "[%d]%n", &newConst, &bytesArg) == 1
                            && arg->argPtr[bytesArg] == '\0') {

        *commandCode = (defaultMask | ramMask | constMask);
        argValues->value1 = newConst;
        argValues->usedArgs = 1;

        return ERR_OK;
    }
    // ---------------------------------------------------------------

    //! For register + const  ----------------------------------------
    bytesArg = 0;
    regName[0] = '\0';
    if (sscanf(arg->argPtr, "%2[A-Z]+%d%n", regName, &newConst, &bytesArg) == 2
                            && arg->argPtr[bytesArg] == '\0') {

        Argument regArg = {.argPtr = regName, .argSize = strlen(regName)};
        int regCode = GetRegCode(&regArg);
        if (regCode == ERR_REGISTER) return ERR_REGISTER;

        *commandCode = (defaultMask | regMask | constMask);
        
        argValues->value1 = regCode;
        argValues->value2 = newConst;
        argValues->usedArgs = 2;

        return ERR_OK;
    }
    // ---------------------------------------------------------------

    //! For RAM, register and const
    bytesArg = 0;
    regName[0] = '\0';
    if (sscanf(arg->argPtr, "[%2[A-Z]+%d]%n", regName, &newConst, &bytesArg) == 2
                            && arg->argPtr[bytesArg] == '\0') {

        Argument regArg = {.argPtr = regName, .argSize = strlen(regName)};
        int regCode = GetRegCode(&regArg);
        if (regCode == ERR_REGISTER) return ERR_REGISTER;

        *commandCode = (defaultMask | regMask | constMask | ramMask);
        argValues->value1 = regCode;
        argValues->value2 = newConst;
        argValues->usedArgs = 2;

        return ERR_OK;
    }

    return ERR_ASM;

}

err_t WriteJumpMark(Marks * label, const char * arg, int * newValue, uint8_t * commandCode) {

    err_t markStat = WriteMarkIndex(label, arg, newValue);

    if (markStat == ERR_SKIP_ARG) {
        *newValue = 0;
    }
    else if (markStat != ERR_OK) {
        return ERR_MARK;
    }

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