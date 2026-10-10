#include "stack.h"

struct AsmFilesInfo {
    char * readFrom = NULL;
    char * writeTo  = NULL;
};


//! Flag for user's filename of asm source
const char * CMD_FLAG_ASM = "--asmsource";

//! Default filename for writing a byte code 
const char * PROG_BYTECODE_FILE = "pbytecode.txt";

//! Assembler's files for calculating hash values of commands

const char * COMMANDS_SOURCE    = "comsrc.txt"; //! File with commands for assembler
const char * HASH_COMMANDS_FILE = "commands.h"; //! Auto-generated file with enum of hash values

//! Max command length for commands (bad)
const int COMMAND_LEN = 10;

//! -------------------------------------------------------------------------
//! Functions for ASSEMBLER
//! -------------------------------------------------------------------------

err_t GetAsmFilenameStd(int argc, char * argv[], AsmFilesInfo * fileInfo);

err_t Assemble(const char * asmSourceName, const char * byteCodeName);

err_t GetCommandCode(const char * command, size_t commandSize, int * commandCode);

//! Write commands to the file with byte code
err_t WriteHashCom(const char * source, const char * commandsFile);

err_t WriteArguments(FILE * fileSource, int args, int newCommandCode, int newElem);
err_t PrepareAsmSource(FileStat * fAsmSource, const char * asmSourceName, const char * byteCodeName);
err_t GetCodeArguments(FileStat * fAsmSource, int i, int * nArgs, char * command,
                                                     int * newElem, int * newCommandCode);

err_t FGetInfoBuffer(const char * fileName, int ** infoBuffer, FileStat * txtFile);

int main(const int argc, char * argv[]) {

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
    printf("File with bytecode: <%s>\n", asmInfo.writeTo);
    
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

    //! Get hash values of using commands
    int hashStat = WriteHashCom(COMMANDS_SOURCE, HASH_COMMANDS_FILE);
    if (hashStat != ERR_OK) {
        // error description
        printf(RED "Error of getting hash values of commands\n" RESET);
        return ERR_ASM;
    }

    FileStat fAsmSource = {};

    err_t prepareAsmStat = PrepareAsmSource(&fAsmSource, asmSourceName, byteCodeName);
    if (prepareAsmStat != ERR_OK) {
        printf("Error of reading asm source\n");
        return ERR_ASM;
    }

    
    FILE * byteCodeFile  = fopen(byteCodeName, "w");

    for (size_t i = 0; i < fAsmSource.nLines - 1; i++) {

        char command[COMMAND_LEN] = "";
        int newElem = 0;
        int nArgs = 0;

        int newCommandCode = 0;

        err_t getArgStat = GetCodeArguments(&fAsmSource, i, &nArgs, command, &newElem, &newCommandCode);
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

err_t WriteHashCom(const char * source, const char * commandsFile) {

    FileStat sourceFile = {};

    int readStat = ReadTxtFile(source, &sourceFile);
    if (!readStat) {
        printf(BOLD_RED "Error of reading file <%s>\n" RESET);
        return ERR_FILEOPEN;
    }

    FILE * fileForWriting = fopen(commandsFile, "w");

    //! Making a enum with hash values of all commands in the "commandsFile"

    fprintf(fileForWriting, "// There's all hash values of all commands\n");
    fprintf(fileForWriting, "#ifndef HASH_COMMANDS\n#define HASH_COMMANDS\n\n");
    fprintf(fileForWriting, "enum HASH_COMMANDS {\n");

    for (size_t i = 0; i < sourceFile.nLines - 1; i++) {

        char * newCommand = sourceFile.index[i];
        if (newCommand == NULL) {
            printf("NULL-pointer to newCommand.\n");
            return ERR_WRITEHASH;
        }

        size_t commandHash = DjbHash((const void *)newCommand, strlen(newCommand));

        fprintf(fileForWriting, "\tHASH_%s = %zu,\n", newCommand, commandHash);

    }

    fprintf(fileForWriting, "\n};\n");
    fprintf(fileForWriting, "#endif");

    fclose(fileForWriting);

    return ERR_OK;
}

err_t WriteArguments(FILE * fileSource, int args, int newCommandCode, int newElem) {

    ASSERT(fileSource);

    if (args == 1) {  // real command without argument

        fprintf(fileSource, "%d\n", newCommandCode);
        return ERR_OK;

    }

    if (args == 2) {  // real command with one argument

        fprintf(fileSource, "%d\t%d\n", newCommandCode, newElem);
        return ERR_OK;

    }

    return ERR_INTPR;

}

err_t GetCodeArguments(FileStat * fAsmSource, int i, int * nArgs, char * command,
                                                     int * newElem, int * newCommandCode) {

    *nArgs = sscanf(fAsmSource->index[i], "%s %d", command, newElem);
    
    int interpreterStat = GetCommandCode(command, strlen(command), newCommandCode);

    if (interpreterStat != ERR_OK) {
    
        printf(BOLD_RED "Translation error: file <%s>, line %d\n"
                        "Incorrect command\n" RESET, fAsmSource->readFrom, i + 1);
        return ERR_INTPR;
    
    }

    return ERR_OK;

}

err_t FGetInfoBuffer(const char * fileName, int ** infoBuffer, FileStat * txtFile) {

    ASSERT(fileName);
    ASSERT(infoBuffer);

    txtFile->readFrom = (char *)fileName;

    if (!ReadTxtFile(fileName, txtFile)) ASSERT(0);

    int * newBuffer = (int *)calloc(txtFile->nLines * 2, sizeof(int));

    if (!newBuffer) {
        printf("ERR_CALLOC: newBuffer error\n");
        return ERR_CALLOC;
    }

    *infoBuffer = newBuffer;

    return ERR_OK;

}

err_t GetCommandCode(const char * command, size_t commandSize, int * commandCode) {

    size_t hashCommand = DjbHash((const void *)command, commandSize);

    switch(hashCommand) {

    case (HASH_PUSH):
        if (strcmp(command, "PUSH") != 0) return ERR_INTPR;
        *commandCode = PUSH_C;
        return ERR_OK;
    
    case (HASH_ADD):
        if (strcmp(command, "ADD") != 0) return ERR_INTPR;
        *commandCode = ADD_C;
        return ERR_OK;

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

    default:
        return ERR_INTPR;
    }

    return ERR_INTPR;
}