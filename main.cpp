#include "stack.h"

//! All used file information
struct FileStat {
    char * text_ptr = NULL;
    char ** index = {};
    off_t sizeInBytes = 0;
    size_t nLines = 0;
    unsigned int sizeOfElem = sizeof(char);
    char * readFrom = NULL;
    char * printTo  = NULL;
};

struct AsmFilesInfo {
    char * readFrom = NULL;
    char * writeTo  = NULL;
};

struct VM_resource {
    stack_t stack = {};
    int * byteCode = NULL; // refact to stackElem_t *
    size_t byteCodeLen = 0;
};

enum COMMANDS {
    PUSH_C = 1,
    ADD_C  = 2,
    SUB_C  = 3,
    DIV_C  = 4,
    OUT_C  = 5,
    HLT_C  = 6,
};

const int COMMAND_LEN = 10;

//! Assembler's files for calculating hash values of commands

const char * COMMANDS_SOURCE    = "comsrc.txt"; //! File with commands for assembler
const char * HASH_COMMANDS_FILE = "commands.h"; //! Auto-generated file with enum of hash values

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

err_t GetFilenameStd(int argc, char * argv[], AsmFilesInfo * fileInfo);

//! Fill buffer from file
//! Text from the file is placed into buffer
err_t PrepareByteCode(VM_resource * virtualMachine, const int argc, char * argv[]);

err_t FGetInfoBuffer(const char * fileName, int ** infoBuffer, FileStat * txtFile);

//! Fill byte code buffer from txt
size_t ReadByteCode(const FileStat * txtFile, int * codeBuffer);

size_t GetByteCode(int ** byteCode, AsmFilesInfo * fileInfo);

//! Main virtual machine
err_t RunVirtualMachine(int * byteCode, int byteCodeLen);

err_t Assembler(const char * asmSourceName, const char * byteCodeName);

err_t WriteHashCom(const char * source, const char * commandsFile);

err_t GetCommandCode(const char * command, size_t commandSize, int * commandCode);

err_t Interpreter(int * byteCode, stack_t * stack, int commandCode);

//! Commands for virtual machine
err_t PushCom(int * byteCode, stack_t * stack, int index);
err_t GetTwoPop(stack_t * stack, stackElem_t * a, stackElem_t * b);
err_t AddCom(stack_t * stack);
err_t SubCom(stack_t * stack);
err_t DivCom(stack_t * stack);
stackElem_t OutCom(stack_t * stack);

//! IO funcs

err_t ReadTxtFile(const char filename[], FileStat * fileInfo);

size_t StringsParser(char * buffer, char diffElem, char ** index);

void FillIndexes(FileStat * fileInfo);

void PrintStrings(FILE * stream, char ** index_ptr, size_t nStrings);

size_t CountElems(const char * string, int Elem);

void PrintErrno(const char filename[], int err_inf);

int main(const int argc, char * argv[]) {

    VM_resource virtualMachine = {};
    
    err_t preparingStat = PrepareByteCode(&virtualMachine, argc, argv);

    if (preparingStat != ERR_OK) {
        // error description
        printf("ERROR OF MAKING BYTE CODE\n");
        return false;
    }

    err_t processingStat = RunVirtualMachine(virtualMachine.byteCode, virtualMachine.byteCodeLen);

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
    int asmStat = Assembler(asmInfo.readFrom, asmInfo.writeTo);

    if (asmStat != ERR_OK) {
        // error description
        printf("ERR_ASM\n");
        return ERR_ASM;
    }

    if (strcmp(asmInfo.readFrom, USER_ASM_SOURCE_FILE) != 0) ASSERT(0);

    //int * byteCode = NULL;

    size_t byteCodeLen = GetByteCode(&(virtualMachine->byteCode), &asmInfo);

    if (byteCodeLen == ERR_BYTECODE) {
        printf(RED "ERR_BYTECODE\n" RESET);
        return ERR_BYTECODE;
    }

    virtualMachine->byteCodeLen = byteCodeLen;

    return ERR_OK;

}

size_t GetByteCode(int ** byteCode, AsmFilesInfo * fileInfo) {

    ASSERT(byteCode);
    ASSERT(fileInfo);

    FileStat txtFile = {};

    //if (!GetFilenameStd(argc, argv, &txtFile)) ASSERT(0);
    if (!ReadTxtFile(fileInfo->writeTo, &txtFile)) ASSERT(0);

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

    for (int i = 0; i < codeBytesLen; i++) {

        printf("[%d] ", *(*byteCode + i));

    }

    return codeBytesLen;

}

err_t Assembler(const char * asmSourceName, const char * byteCodeName) {

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

    FILE * byteCode  = fopen(byteCodeName, "w");

    int * asmSource = NULL;
    int getAsmStat = FGetInfoBuffer(asmSourceName, &asmSource, &fAsmSource);

    if (getAsmStat != ERR_OK) {
        return ERR_ASM;
    }
    ASSERT(asmSource);

    // PrintStrings(byteCode, fAsmSource.index, fAsmSource.nLines);

    for (size_t i = 0; i < fAsmSource.nLines - 1; i++) {

        // new func
        char command[COMMAND_LEN] = "";
        int var = 0;

        int args = sscanf(fAsmSource.index[i], "%s %d", command, &var);
        printf(GREEN "Line = %d, args = %d\n" RESET, i, args);

        int newCommandCode = 0;
        int interpreterStat = GetCommandCode(command, strlen(command), &newCommandCode);

        if (interpreterStat != ERR_OK) {
        
            printf(BOLD_RED "Translation error: file <%s>, line %d\n"
                            "Incorrect command\n" RESET, fAsmSource.readFrom, i + 1);

            return ERR_INTPR;
        
        }
        printf(YELLOW "Command = %s, variable = %d\n" RESET, command, var);

        // new func check args
        if (args == 1) {  // real command without argument
            fprintf(byteCode, "%d\n", newCommandCode);
            continue;
        }

        else if (args == 2) {  // real command with one argument
            fprintf(byteCode, "%d\t%d\n", newCommandCode, var);
            continue;
        }

        else return ERR_INTPR;

    }
    fclose(byteCode);

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
    fprintf(fileForWriting, "#ifndef HASH_COMMANDS\n#define HASH_COMMANDS\n");
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

    fprintf(fileForWriting, "};\n");
    fprintf(fileForWriting, "#endif");

    fclose(fileForWriting);

    return ERR_OK;
}

err_t FGetInfoBuffer(const char * fileName, int ** infoBuffer, FileStat * txtFile) {

    ASSERT(fileName);
    ASSERT(infoBuffer);

    txtFile->readFrom = (char *)fileName;

    if (!ReadTxtFile(fileName, txtFile)) ASSERT(0);

    printf("File: <%s>, number of '0' = %zu\n", fileName, txtFile->nLines);

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
    printf("Hash value of <%s> in GetCommandCode: %zu\n", command, hashCommand);

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

void PrintStrings(FILE * stream, char ** index_ptr, size_t nStrings) {

    ASSERT(index_ptr);
    ASSERT(*index_ptr);

    for (size_t count = 0; count < nStrings; count++) {

        ASSERT(index_ptr[count]);
        fprintf(stream, "%s\n", index_ptr[count]);

    }
}

err_t RunVirtualMachine(int * byteCode, int byteCodeLen) {

    ASSERT(byteCode);

    size_t pc = 0;

    stack_t stack = {};
    StackCtor(&stack, 10);

    FILE * file_p = fopen(FILE_STACKDUMP, "w");

    stackElem_t outResult = 0;

    while ((pc <= byteCodeLen)) {

        StackDump(file_p, &stack);

        if (byteCode[pc] == HLT_C) {
            printf(BOLD_GREEN "Output: " STACK_ELEM_FORMAT "\n" RESET, outResult);
            return ERR_OK;
        }

        if (byteCode[pc] == OUT_C) {
            stackElem_t out = OutCom(&stack);

            if (out == ERR_COMMAND) return ERR_OUT_PROCESSING;

            outResult = out;
            pc++;
            continue;

        }

        int goOn = Interpreter(byteCode, &stack, pc);

        if (goOn == ERR_COMMAND) {
            printf(RED "ERROR_PROCESSING\n" RESET);
            return ERR_PROCESSING;
        }
        pc = pc + goOn;

    }
    fclose(file_p);
    
    printf(BOLD_RED "In asm file no HLT command\n" RESET);

    ASSERT(0);
    return false;
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

    printf("Getted: %d, %d\n", *a, *b);

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
    StackDump(stdout, stack);

    printf("ADD = %d\n", a+b);
    printf("PushStat = %d\n", pushStat);

    if (pushStat != ERR_OK) return ERR_COMMAND;

    return 1;

}

err_t SubCom(stack_t * stack) {

    ASSERT(stack);

    stackElem_t a = 0, b = 0;

    if (GetTwoPop(stack, &a, &b) != ERR_OK) return ERR_COMMAND;

    printf("Sub = %d\n", b - a);

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

err_t ReadTxtFile(const char filename[], FileStat * fileInfo) {

    ASSERT(filename);
    ASSERT(fileInfo);

    struct _stat fileStat = {};
    
    FILE * file_p = fopen(filename, "r");

    if (!file_p || (_stat(filename, &fileStat) == -1)) {
        PrintErrno((*fileInfo).readFrom, errno);
        return false;
    }

    (*fileInfo).sizeInBytes = fileStat.st_size;

    char * buffer = (char *)calloc((*fileInfo).sizeInBytes + (off_t)1, (*fileInfo).sizeOfElem);
    ASSERT(buffer);

    size_t lastIndex = fread((void *)buffer, (*fileInfo).sizeOfElem, (*fileInfo).sizeInBytes, file_p);
    buffer[lastIndex] = '\0';

    (*fileInfo).text_ptr = buffer;

    fclose(file_p);

    FillIndexes(fileInfo);

    return true;
}

//! Splits the file's strings from fileInfo.text_ptr at a specified char - "diffElem"
size_t StringsParser(char * buffer, char diffElem, char ** index) {
    
    ASSERT(buffer);
    ASSERT(index);

    char * currPtr = (char *)buffer;
    char * lastPtr = strchr(buffer, '\0');

    size_t indexPtr = 0;
    index[indexPtr++] = currPtr; // first string

    while ((currPtr <= lastPtr) && (*currPtr != '\0') && (currPtr != NULL)) {

        char * newPtr = strchr(currPtr, diffElem);
        //printf("NewPtr: %p", newPtr);

        if ((newPtr == NULL) || (newPtr > lastPtr)) {
            //printf("break\n");
            break;
        }
        currPtr = newPtr;
        *currPtr = '\0';

        index[indexPtr++] = currPtr+1;
        currPtr++;
    }

    return indexPtr;
}

void FillIndexes(FileStat * fileInfo) {

    ASSERT(fileInfo);
    ASSERT((*fileInfo).text_ptr);

    printf("FillIndexes\n");

    (*fileInfo).nLines = CountElems((*fileInfo).text_ptr, '\n');

    (*fileInfo).index = (char **)calloc((*fileInfo).nLines, sizeof(char *));

    StringsParser((*fileInfo).text_ptr, '\n', (*fileInfo).index);

}

size_t CountElems(const char * string, int Elem) {

    ASSERT(string);

    size_t count = 0;

    while ((*string != '\0') && (*string != EOF)) {

        if (*string == Elem) count++;
        string++;
    }
    count++;

    return count;
}

void PrintErrno(const char filename[], int err_inf) {

    ASSERT(filename);

    printf(RED "File: <%s>\n", filename);

    const char * errorMessage = strerror(err_inf);

    if (errorMessage) perror(errorMessage);
    printf(RESET);
}

size_t ReadByteCode(const FileStat * txtFile, int * codeBuffer) {

    ASSERT(txtFile);
    ASSERT(codeBuffer);

    // Count of elements that are writing to codeBuffer
    size_t count = 0;

    for (size_t i = 0; i < txtFile->nLines; i++) {

        int command = -1; // -1 using only for init, there's no real commands with code that equals -1
        int var = 0;

        int args = sscanf(txtFile->index[i], "%d %d", &command, &var);
        printf(GREEN "Line = %d, args = %d" RESET, i, args);

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
            codeBuffer[count++] = var;
        }

        else return ERR_BYTECODE;

        printf(YELLOW "Command = %d, variable = %d\n" RESET, command, var);
    }
    
    return count;
    
}