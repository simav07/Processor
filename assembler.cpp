typedef int err_t;

#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <getopt.h>

#include "include/errors.h"
#include "include/colors.h"
#include "include/io.h"
#include "include/commands.h"
#include "hashcommands.h"

struct LabelEntry {
    size_t labelHash;
    ssize_t address;
};

struct Label {
    LabelEntry * label;
    size_t nMark;
    size_t capacity;
};

struct Marks {
    int * Label;
    size_t nMark  = 0;
};

struct Assembler {
    char * readFrom = NULL;
    char * writeTo  = NULL;

    FileStat fAsmSource = {};

    Label asmLabel = {};

    FILE * listFile;
    bool isListing;
};

struct Command {
    char * commandPtr;
    size_t commandSize;
};

struct Argument {
    char * argPtr;
    size_t argSize;
};

//! Max command length for commands
const int MAX_COMMAND_LEN = 256;

/**
 * Values near the command like PUSH 20, POP [1], PUSH [BX+30].
 * There is 2 (BX = 2) and 30 is the first value and the second value
 * Maximum two arguments near the command
 */
struct ArgValues {

    char arg1[MAX_COMMAND_LEN];
    char arg2[MAX_COMMAND_LEN];

    int8_t nArgs;

    int32_t value1;
    int32_t value2;
    uint8_t usedArgs; //!< 0, 1 or 2 used arguments in current command
};

//! Flag for user's filename of asm source
const char * CMD_FLAG_ASM = "--asmsource";
//! Flag for user's filename of byte code
const char * CMD_FLAG_BC  = "--bytecode";

//! Default filename for writing a byte code 
static const char * PROG_BYTECODE_FILE = "bcode.bin";

//! Default filename for writing a code listing
static const char * ASM_LIST_FILE = "listing.asm";

char * BYTECODE_FILENAME = NULL;

const size_t LABEL_SIZE = 100;

//! -------------------------------------------------------------------------
//! Functions for ASSEMBLER
//! -------------------------------------------------------------------------

err_t AsmCtor(Assembler * assembler);
err_t AsmDtor(Assembler * assembler);

err_t ArgvParser(int argc, char * argv[], Assembler * assembler);

err_t Assemble(Assembler * assembler);

err_t GetCommandCode(Label * asmLabel, Command * command, uint8_t * commandCode, 
                                    ArgValues * newValues);

err_t ConstCode(uint8_t defaultMask, uint8_t * commandCode, ArgValues * argValues, int32_t newConst);
err_t RegCode(uint8_t defaultMask, uint8_t * commandCode, ArgValues * argValues, char * regName);
err_t MemConstCode(uint8_t defaultMask, uint8_t * commandCode, ArgValues * argValues, int32_t * newConst);
err_t RegMemCode(uint8_t defaultMask, uint8_t * commandCode, ArgValues * argValues, char * regName);
err_t RegConstCode(uint8_t defaultMask, uint8_t * commandCode,
                    ArgValues * argValues, char * regName, int32_t * newConst);
err_t MemRegConstCode(uint8_t defaultMask, uint8_t * commandCode,
                    ArgValues * argValues, char * regName, int32_t * newConst);

int GetRegCode(Argument * arg2);

err_t CallCommand(Label * asmLabel, ArgValues * newValues, uint8_t * commandCode);

err_t GetMasksCode(uint8_t defaultValue, Command * command,
                                           uint8_t * commandCode, ArgValues * argValues);

err_t WriteJumpMark(Label * label, const char * arg, int * newValue, uint8_t * commandCode);
err_t GetLabel(Label * asmLabel, ArgValues * newValues);

err_t SetLabel(Label * label, const char * arg1, const size_t currIndex);
err_t ReadMarkIndex(const char * arg2, size_t * newLabelHash);

//! Write commands to the file with byte code
size_t DjbHash(const void * data, size_t size);

err_t WriteArguments(FILE * fileSource, uint8_t newCommandCode, ArgValues * newValues);
err_t PrepareAsmSource(FileStat * fAsmSource, const char * asmSourceName, const char * byteCodeName);
err_t GetCodeArguments(Assembler * assembler, size_t * count, size_t i,
                                              ArgValues * newValues, uint8_t * newCommandCode);

err_t FGetInfoBuffer(const char * fileName, int ** infoBuffer, FileStat * txtFile);

void BeginListing(Assembler * assembler);
void AsmListing(Assembler * assembler, ArgValues newValues, uint8_t newCommandCode, size_t count);
void EndListing(Assembler * assembler);

char * toBinary(uint8_t num, char * buffer, size_t bufSize);

int main(int argc, char * argv[]) {

    printf("\nFile <%s>, [%s]\n", __FILE__, __TIME__);

    FileStat fAsmSource = {};

    Assembler assembler = {};

    if (!ArgvParser(argc, argv, &assembler)) return ERR_ASM_CTOR;

    int ctorStat = AsmCtor(&assembler);
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

    AsmDtor(&assembler);
    
    return 0;
}

err_t AsmCtor(Assembler * assembler) {

    ASSERT(assembler);

    err_t prepareAsmStat = PrepareAsmSource(&(assembler->fAsmSource), assembler->readFrom, assembler->writeTo);
    if (prepareAsmStat != ERR_OK) {
        printf("Error of reading asm source\n");
        return ERR_ASM_CTOR;
    }

    LabelEntry * newLabel = (LabelEntry *)calloc(LABEL_SIZE, sizeof(LabelEntry));
    if (!newLabel) return ERR_CTOR;
    assembler->asmLabel.label = newLabel;

    for (size_t i = 0; i < LABEL_SIZE; i++) { assembler->asmLabel.label[i].address = -1; }

    if (assembler->isListing) BeginListing(assembler);
    
    return ERR_OK;

}

err_t ArgvParser(int argc, char * argv[], Assembler * assembler) {

    ASSERT(argv);
    ASSERT(*argv);
    ASSERT(assembler);

    int flag = 0;

    struct option longOptions[] = {
        {"asmsource", required_argument, 0, 'a'},
        {"bytecode",  required_argument, 0, 'b'},
        {"listing",   no_argument,       0, 'l'},
        {0, 0, 0, 0} 
    };

    int optIndex = 0;

    while ((flag = getopt_long(argc, argv, "", longOptions, &optIndex)) != -1) {

        switch (flag)
        {
        
        case ('a'): {
            assembler->readFrom = strdup(optarg);
            if (!assembler->readFrom) return false;
            break;
        }
        case ('b'): {
            assembler->writeTo = strdup(optarg);
            if (!assembler->writeTo) return false;
            break;
        }
        case ('l'):
            assembler->isListing = true;
            break;

        default: return false;

        }
    }

    return true;
}

err_t Assemble(Assembler * assembler) {

    ASSERT(assembler);
    
    for (int j = 0; j < 2; j++) {

    FILE * byteCodeFile  = fopen(assembler->writeTo, "wb");

    printf(YELLOW "%d step of assembling...\n" RESET, j + 1);

    //! Count in bytes
    size_t count = 0;

    for (size_t i = 0; i < (assembler->fAsmSource).nLines; i++) {
        
        ArgValues newValues = {};
        uint8_t newCommandCode = 0;

        err_t getArgStat = GetCodeArguments(assembler, &count, i, &newValues, &newCommandCode);
        if (getArgStat != ERR_OK) {
            printf("Error of getting args\n");
            return ERR_ASM;
        }

        if (assembler->isListing && j == 1) AsmListing(assembler, newValues, newCommandCode, count);
        
        err_t writeArgsStat = WriteArguments(byteCodeFile, newCommandCode, &newValues);
        if (writeArgsStat != ERR_OK) {
            printf("Error of writing arguments to the byte code file\n");
            return ERR_ASM;
        }


        if (newCommandCode == SKIP_STEP) continue;;

        count += 1 + (newValues.usedArgs * sizeof(int32_t));

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

err_t WriteArguments(FILE * fileSource, uint8_t newCommandCode, ArgValues * newValues) {

    ASSERT(newValues);
    ASSERT(fileSource);

    int8_t args = newValues->nArgs;

    if (newCommandCode == SKIP_STEP || args <= 0) return ERR_OK;

    if (args != 1 && args != 2) {
        return ERR_ASM;
    }

    uint8_t argCount = (args == 2) ? 1 : 0;

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

err_t GetCodeArguments(Assembler * assembler, size_t * count, size_t i,
                                              ArgValues * newValues, uint8_t * newCommandCode) {
    ASSERT(assembler);
    ASSERT(count);

    // buffer overflow protect
    if (assembler->fAsmSource.index->length > MAX_COMMAND_LEN) return ERR_OVERFLOW;

    newValues->nArgs = sscanf(assembler->fAsmSource.index[i].index, "%10s %10s",
                                                    newValues->arg1, newValues->arg2);
    
    //! Comments and empty strings
    if (newValues->arg1[0] == ';' || newValues->nArgs <= 0) {
        *newCommandCode = SKIP_STEP;
        return ERR_OK;
    }

    if (newValues->nArgs != 1 && newValues->nArgs != 2) return ERR_COMMAND;

    // Write a mark to the label
    if (newValues->arg1[0] == ':') {

        err_t makeMarkStat = SetLabel(&(assembler->asmLabel), newValues->arg1, *count);
        if (makeMarkStat != ERR_OK) {
            printf("Make mark error\n");
            return ERR_ASM;
        }

        *newCommandCode = SKIP_STEP;

        return ERR_OK;
    }

    Command command   = {.commandPtr = newValues->arg1, .commandSize = strlen(newValues->arg1)};
    // Argument argument = {.argPtr = newValues->arg2, .argSize = strlen(newValues->arg2)};

    err_t interpreterStat = GetCommandCode(&(assembler->asmLabel), &command, newCommandCode, newValues);

    if (interpreterStat != ERR_OK) {
    
        printf(BOLD_RED "Translation error: file <%s>, line %d\n"
                        "Incorrect command\n" RESET, assembler->fAsmSource.readFrom, i + 1);
        return ERR_ASM;
    
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

err_t SetLabel(Label * asmLabel, const char * arg1, const size_t currIndex) {

    ASSERT(asmLabel);
    ASSERT(arg1);

    size_t newLabelHash = 0;
    if (ReadMarkIndex(arg1, &newLabelHash) != ERR_OK) return ERR_SET_LABEL;

    if (asmLabel->nMark >= LABEL_SIZE) return ERR_MARK;

    asmLabel->label[asmLabel->nMark].labelHash = newLabelHash;
    asmLabel->label[asmLabel->nMark].address   = currIndex;

    if (asmLabel->nMark >= LABEL_SIZE) return ERR_MARK;

    asmLabel->nMark++;

    return ERR_OK;

}

err_t ReadMarkIndex(const char * arg2, size_t * newLabelHash) {

    ASSERT(newLabelHash);
    ASSERT(arg2);

    char newLabelVal[MAX_COMMAND_LEN] = "";

    int scanStat = sscanf(arg2, ":%30s", newLabelVal);

    if (scanStat != 1) {
        printf("ERROR OF MARK NUMBER READING\n");
        return ERR_MARK;
    }

    // if (label->nMark >= LABEL_SIZE) return ERR_MARK;

    *newLabelHash = DjbHash(newLabelVal, strlen(newLabelVal));

    return ERR_OK;
}

err_t GetLabel(Label * asmLabel, ArgValues * newValues) {

    ASSERT(asmLabel);
    ASSERT(newValues);

    size_t newLabelHash = 0;
    ssize_t newIndex = -1;
    if (ReadMarkIndex(newValues->arg2, &newLabelHash) != ERR_OK) return ERR_SET_LABEL;

    for (size_t i = 0; i < asmLabel->nMark; i++) {

        if (asmLabel->label[i].labelHash == newLabelHash) {
            newIndex = asmLabel->label[i].address;
        }

    }
    
    newValues->value1 = newIndex;
    
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

err_t GetCommandCode(Label * asmLabel, Command * command, uint8_t * commandCode, 
                                    ArgValues * newValues) {

    ASSERT(asmLabel);
    ASSERT(command);
    ASSERT(commandCode);
    ASSERT(newValues);

    size_t hashCommand = DjbHash((const void *)(command->commandPtr), command->commandSize);

    switch(hashCommand) {

    case (HASH_PUSH): {
        if (strcmp(command->commandPtr, "PUSH") != 0) return ERR_INTPR;

        return GetMasksCode(PUSH_C, command, commandCode, newValues);
        
    }

    case (HASH_POP): {
        if (strcmp(command->commandPtr, "POP") != 0) return ERR_INTPR;

        return GetMasksCode(POP_C, command, commandCode, newValues);
    }

    case (HASH_IN):
        if (strcmp(command->commandPtr, "IN") != 0) return ERR_INTPR;

        return GetMasksCode(IN_C, command, commandCode, newValues);

    case (HASH_ADD):
        if (strcmp(command->commandPtr, "ADD") != 0) return ERR_INTPR;
        return GetMasksCode(ADD_C, command, commandCode, newValues);

    case (HASH_SUB):
        if (strcmp(command->commandPtr, "SUB") != 0) return ERR_INTPR;
        return GetMasksCode(SUB_C, command, commandCode, newValues);

    case (HASH_DIV):
        if (strcmp(command->commandPtr, "DIV") != 0) return ERR_INTPR;
        return GetMasksCode(DIV_C, command, commandCode, newValues);

    case (HASH_SQRT):
        if (strcmp(command->commandPtr, "SQRT") != 0) return ERR_INTPR;
        return GetMasksCode(SQRT_C, command, commandCode, newValues);

    case (HASH_OUT):
        if (strcmp(command->commandPtr, "OUT") != 0) return ERR_INTPR;
        return GetMasksCode(OUT_C, command, commandCode, newValues);

    case (HASH_HLT):
        if (strcmp(command->commandPtr, "HLT") != 0) return ERR_INTPR;
        return GetMasksCode(HLT_C, command, commandCode, newValues);

    case (HASH_MUL):
        if (strcmp(command->commandPtr, "MUL") != 0) return ERR_INTPR;
        return GetMasksCode(MUL_C, command, commandCode, newValues);

    case (HASH_CALL): {
        if (strcmp(command->commandPtr, "CALL") != 0) return ERR_INTPR;

        return CallCommand(asmLabel, newValues, commandCode);

    }

    case (HASH_RET):
        if (strcmp(command->commandPtr, "RET") != 0) return ERR_INTPR;
        return GetMasksCode(RET_C, command, commandCode, newValues);

    case (HASH_JUMP): {

        if (strcmp(command->commandPtr, "JUMP") != 0) return ERR_INTPR;

        *commandCode = JUMP_C | 0x20;
        newValues->usedArgs = 1;

        return GetLabel(asmLabel, newValues);
    }

    case (HASH_JA): {
        if (strcmp(command->commandPtr, "JA") != 0) return ERR_INTPR;

        *commandCode = JA_C | 0x20;
        newValues->usedArgs = 1;

        return GetLabel(asmLabel, newValues);
    }

    case (HASH_JAE): {
        if (strcmp(command->commandPtr, "JAE") != 0) return ERR_INTPR;

        *commandCode = JAE_C | 0x20;
        newValues->usedArgs = 1;

        return GetLabel(asmLabel, newValues);

    }

    case (HASH_JB): {
        if (strcmp(command->commandPtr, "JB") != 0) return ERR_INTPR;

        newValues->usedArgs = 1;
        *commandCode = JB_C | 0x20;

        return GetLabel(asmLabel, newValues);

    }

    case (HASH_JBE): {
        if (strcmp(command->commandPtr, "JBE") != 0) return ERR_INTPR;

        newValues->usedArgs = 1;
        *commandCode = JBE_C | 0x20;

        return GetLabel(asmLabel, newValues);

    }

    case (HASH_JE): {
        if (strcmp(command->commandPtr, "JE") != 0) return ERR_INTPR;

        newValues->usedArgs = 1;
        *commandCode = JE_C | 0x20;

        return GetLabel(asmLabel, newValues);
    }

    case (HASH_JNE): {
        if (strcmp(command->commandPtr, "JNE") != 0) return ERR_INTPR;

        newValues->usedArgs = 1;
        *commandCode = JNE_C | 0x20;

        return GetLabel(asmLabel, newValues);
    }
    default:
        return ERR_INTPR;
    }

    return ERR_INTPR;
}

err_t GetMasksCode(uint8_t defaultValue, Command * command,
                                           uint8_t * commandCode, ArgValues * argValues) {

    ASSERT(command);
    ASSERT(commandCode);
    ASSERT(argValues);

    uint8_t defaultMask = defaultValue;

    CODE_MASKS;

    // No args
    if (defaultValue != PUSH_C && defaultValue != POP_C) {
        if (strlen(argValues->arg2) != 0) return ERR_COMMAND;

        *commandCode = defaultValue;
        argValues->usedArgs = 0;

        return ERR_OK;
    }

    // push 10      ConstCode();
    // push AX      RegCode();
    // push AX+10   RegConstCode();
    // push [10]    MemConstCode();
    // push [AX]    MemRegCode();
    // push [AX+10] MemRegConstCode();

    int bytesArg = 0;
    int32_t newConst = 0;

    // Constant
    if (defaultValue != POP_C && sscanf(argValues->arg2, "%d%n", &newConst, &bytesArg) == 1
                               && argValues->arg2[bytesArg] == '\0') {

        return ConstCode(defaultMask, commandCode, argValues, newConst);
    
    }

    //! For registers -----------------------------------------
    char regName[3] = "";
    bytesArg = 0;

    if (sscanf(argValues->arg2, "%2[A-ZX]%n", regName, &bytesArg) == 1
                    && bytesArg == 2 && argValues->arg2[bytesArg] == '\0') {
            
            return RegCode(defaultMask, commandCode, argValues, regName);
    }

    //! For ram ------------------------------------------------------
    bytesArg = 0;
    if (sscanf(argValues->arg2, "[%d]%n", &newConst, &bytesArg) == 1
                            && argValues->arg2[bytesArg] == '\0') {

        return MemConstCode(defaultMask, commandCode, argValues, &newConst);
    }

    //! For register and ram -----------------------------------------
    bytesArg = 0;
    regName[0] = '\0';
    
    if (sscanf(argValues->arg2, "[%2[A-ZX]]%n", regName, &bytesArg) == 1 
                                        && argValues->arg2[bytesArg] == '\0') {

        return RegMemCode(defaultMask, commandCode, argValues, regName);
    }

    //! For register + const  ----------------------------------------
    bytesArg = 0;
    regName[0] = '\0';
    sscanf(argValues->arg2, "%2[A-ZX]+%d%n", regName, &newConst, &bytesArg);
    if (argValues->arg2[bytesArg] == '\0') {

        return RegConstCode(defaultMask, commandCode, argValues, regName, &newConst);
    }

    //! For RAM, register and const
    bytesArg = 0;
    regName[0] = '\0';
    if (sscanf(argValues->arg2, "[%2[A-ZX]+%d]%n", regName, &newConst, &bytesArg) == 2
                            && argValues->arg2[bytesArg] == '\0') {

        return MemRegConstCode(defaultMask, commandCode, argValues, regName, &newConst);
    }

    return ERR_ASM;

}

err_t ConstCode(uint8_t defaultMask, uint8_t * commandCode, ArgValues * argValues, int32_t newConst) {

    ASSERT(commandCode);
    ASSERT(argValues);

    CODE_MASKS;

    *commandCode = (defaultMask | constMask);
    
    argValues->value1 = newConst;
    argValues->usedArgs = 1;

    return ERR_OK;
}

err_t RegCode(uint8_t defaultMask, uint8_t * commandCode, ArgValues * argValues, char * regName) {

    ASSERT(commandCode);
    ASSERT(argValues);
    ASSERT(regName);

    CODE_MASKS;

    Argument regArg = {.argPtr = regName, .argSize = strlen(regName)};

    int regCode = GetRegCode(&regArg);
    if (regCode == ERR_REGISTER) return ERR_REGISTER;

    *commandCode = (defaultMask | regMask);

    argValues->value1 = regCode;
    argValues->usedArgs = 1;
    
    return ERR_OK;

}

err_t MemConstCode(uint8_t defaultMask, uint8_t * commandCode, ArgValues * argValues, int32_t * newConst) {

    ASSERT(commandCode);
    ASSERT(argValues);
    ASSERT(newConst);

    CODE_MASKS;

    *commandCode = (defaultMask | ramMask | constMask);
    argValues->value1 = *newConst;
    argValues->usedArgs = 1;
    
    return ERR_OK;

}

err_t RegMemCode(uint8_t defaultMask, uint8_t * commandCode, ArgValues * argValues, char * regName) {

    ASSERT(commandCode);
    ASSERT(argValues);
    ASSERT(regName);
    
    CODE_MASKS;

    Argument regArg = {.argPtr = regName, .argSize = strlen(regName)};

    int regCode = GetRegCode(&regArg);
    
    if (regCode == ERR_REGISTER) return ERR_REGISTER;

    *commandCode = (defaultMask | ramMask | regMask);
    argValues->value1 = regCode;

    argValues->usedArgs = 1;
    
    return ERR_OK;

}

err_t RegConstCode(uint8_t defaultMask, uint8_t * commandCode,
                    ArgValues * argValues, char * regName, int32_t * newConst) {

    ASSERT(commandCode);
    ASSERT(argValues);
    ASSERT(newConst);
    ASSERT(regName);

    CODE_MASKS;

    Argument regArg = {.argPtr = regName, .argSize = strlen(regName)};
    int regCode = GetRegCode(&regArg);
    if (regCode == ERR_REGISTER) return ERR_REGISTER;
    *commandCode = (defaultMask | regMask | constMask);
    
    argValues->value1 = regCode;
    argValues->value2 = *newConst;
    argValues->usedArgs = 2;

    return ERR_OK;

}

err_t MemRegConstCode(uint8_t defaultMask, uint8_t * commandCode,
                    ArgValues * argValues, char * regName, int32_t * newConst) {

    ASSERT(commandCode);
    ASSERT(argValues);
    ASSERT(newConst);
    ASSERT(regName);
    
    CODE_MASKS;

    Argument regArg = {.argPtr = regName, .argSize = strlen(regName)};

    int regCode = GetRegCode(&regArg);
    if (regCode == ERR_REGISTER) return ERR_REGISTER;
    
    *commandCode = (defaultMask | regMask | constMask | ramMask);
    
    argValues->value1 = regCode;
    argValues->value2 = *newConst;
    argValues->usedArgs = 2;
    
    return ERR_OK;

}

err_t CallCommand(Label * asmLabel, ArgValues * newValues, uint8_t * commandCode) {

    size_t currHash = 0;

    err_t readStat = ReadMarkIndex(newValues->arg2, &currHash);
    if (readStat != ERR_OK) return ERR_COMMAND;

    err_t labelStat = GetLabel(asmLabel, newValues);

    if (labelStat == ERR_SKIP_ARG) {
        newValues->value1 = 0;
    }

    else if (labelStat != ERR_OK) {
        return ERR_MARK;
    }

    *commandCode = CALL_C | 0x20;
    newValues->usedArgs = 1;
    
    return ERR_OK;
}

void BeginListing(Assembler * assembler) {

    ASSERT(assembler);

    assembler->listFile = fopen(ASM_LIST_FILE, "w");
    if (assembler->listFile == NULL) {
        return;
    }

    fprintf(assembler->listFile,
        "------------------------------------------------------------------------\n");
    fprintf(assembler->listFile, 
        "--------------------  <<< ASSEMBLER LISTING >>> ------------------------\n");
    fprintf(assembler->listFile,
        "------------------------------------------------------------------------\n");
    
    fprintf(assembler->listFile,
        "| ADDRESS           | HEX CODE                | SOURCE LINE            |\n");

    fprintf(assembler->listFile,
        "------------------------------------------------------------------------\n");
    fflush(assembler->listFile);
    
}

void AsmListing(Assembler * assembler, ArgValues newValues, uint8_t newCommandCode, size_t count) {

    ASSERT(assembler);

    size_t addressStrLen = 20;
    size_t bitStrLen     = 26;
    size_t srcLineLen    = 26;

    char addressLine[addressStrLen] = "";
    snprintf(addressLine, addressStrLen, "%#06zx", count);
    fprintf(assembler->listFile, "| %-18s", addressLine);

    char bitCodeLine[bitStrLen] = "";
    char sourceLine[srcLineLen] = "";

    char cmdCodeBinary[bitStrLen] = "";

    switch (newValues.usedArgs)
    {
        case (0): 
            snprintf(bitCodeLine, bitStrLen, "%s", toBinary(newCommandCode, cmdCodeBinary, bitStrLen));
            snprintf(sourceLine, srcLineLen, "%s", newValues.arg1);
            break;
        case (1):
            snprintf(bitCodeLine, bitStrLen, "%s %d", 
                    toBinary(newCommandCode, cmdCodeBinary, bitStrLen), newValues.value1);
            snprintf(sourceLine, srcLineLen, "%s %s", newValues.arg1, newValues.arg2);
            break;
        case (2):
            snprintf(bitCodeLine, bitStrLen, "%s %d %d", 
                    toBinary(newCommandCode, cmdCodeBinary, bitStrLen), newValues.value1, newValues.value2);
            snprintf(sourceLine, srcLineLen, "%s %s", newValues.arg1, newValues.arg2);
            break;
        default: break;
    }
    fprintf(assembler->listFile, "| %-24s", bitCodeLine);
    fprintf(assembler->listFile, "| %-23s|", sourceLine);
    fprintf(assembler->listFile, "\n");
    fflush(assembler->listFile);
}

char * toBinary(uint8_t num, char * buffer, size_t bufSize) {

    if (bufSize == 0) return buffer;
    
    size_t bits = sizeof(num) * 8;
    buffer[bits] = '\0';
    
    for (int i = bits - 1; i >= 0; i--) {
        buffer[i] = (num & 1) ? '1' : '0';
        num >>= 1;
    }
    
    return buffer;
}

void EndListing(Assembler * assembler) {

    ASSERT(assembler);
    printf("END of listing\n");
    
    if (assembler->listFile == NULL) return;

    fprintf(assembler->listFile,
        "------------------------------------------------------------------------\n");
    fprintf(assembler->listFile,
        "----------      <<<< THE END OF THE CODE LISTING >>>>        -----------\n");
    fprintf(assembler->listFile,
        "------------------------------------------------------------------------\n");

    fclose(assembler->listFile);
}

err_t AsmDtor(Assembler * assembler) {

    ASSERT(assembler);

    if (assembler->isListing) EndListing(assembler);

    free(assembler->readFrom);
    free(assembler->writeTo);
    free(assembler->fAsmSource.text_ptr);
    free(assembler->fAsmSource.index->index);

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