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

enum COMMANDS {
    PUSH_C = 1,
    ADD_C  = 2,
    SUB_C  = 3,
    DIV_C  = 4,
    OUT_C  = 5,
    HLT_C,
};

const double NO_ARGS = 1.5;

//const char * FILE_BYTECODE = "bytecode.txt";

int GetFilenameStd(int argc, char * argv[], FileStat * fileInfo);

//! Fill buffer from file
int ReadTxtFile(const char filename[], FileStat * fileInfo);

size_t StringsParser(char * buffer, char diffElem, char ** index);

void FillIndexes(FileStat * fileInfo);

size_t CountElems(const char * string, int Elem);

void PrintErrno(const char filename[], int err_inf);

//! Fill byte code buffer
int ReadByteCode(const FileStat * txtFile, int * codeBuffer);

int GetByteCode(const int argc, char * argv[], int ** byteCode);

//! Main virtual machine
stackElem_t VirtualMachine(int * byteCode, int byteCodeLen);

int RunCommand(int * byteCode, stack_t * stack, int commandCode);

//! Commands for virtual machine
int PushCom(int * byteCode, stack_t * stack, int index);
int GetTwoPop(stack_t * stack, int * a, int * b);
int AddCom(stack_t * stack);
int SubCom(stack_t * stack);
int DivCom(stack_t * stack);
stackElem_t OutCom(stack_t * stack);

int main(const int argc, char * argv[]) {

    // fill code buffer
    int * byteCode = NULL;

    int byteCodeLen = GetByteCode(argc, argv, &byteCode);

    if (byteCodeLen == ERR_BYTECODE) {
        printf(RED "ERR_BYTECODE\n" RESET);
        return false;
    }
    ASSERT(byteCode);

    printf(GREEN "Output = " STACK_ELEM_FORMAT "\n" RESET, VirtualMachine(byteCode, byteCodeLen));

    return true;
}

int GetByteCode(const int argc, char * argv[], int ** byteCode) {

    ASSERT(argv);

    FileStat txtFile = {};

    if (!GetFilenameStd(argc, argv, &txtFile)) ASSERT(0);
    if (!ReadTxtFile(txtFile.readFrom, &txtFile)) ASSERT(0);

    //txtFile.nLines = CountElems(txtFile.text_ptr, '\0');
    printf("Number of '0' = %zu\n", txtFile.nLines);

    int * newBuffer = (int *)calloc(txtFile.nLines, 2 * sizeof(int));

    if (!newBuffer) {
        printf("newBuffer error\n");
        return ERR_BYTECODE;
    }

    *byteCode = newBuffer;

    int codeBytesLen = ReadByteCode(&txtFile, *byteCode);

    printf("ByteCodeLen = %d\n", codeBytesLen);

    if (codeBytesLen == ERR_BYTECODE) {
        printf("ERR_BYTECODE\n");
        return ERR_BYTECODE;
    }

    for (int i = 0; i < codeBytesLen; i++) {

        printf("[%d] ", *(*byteCode + i));

    }

    return codeBytesLen;

}

stackElem_t VirtualMachine(int * byteCode, int byteCodeLen) {

    ASSERT(byteCode);

    int i = 0;

    stack_t stack = {};
    StackCtor(&stack, 10);

    FILE * file_p = fopen("dump.txt", "w");

    while ((i <= byteCodeLen) && (byteCode[i] != HLT_C)) {

        StackDump(file_p, &stack);

        if (byteCode[i] == OUT_C) {
            printf(GREEN "OutCom\n" RESET);
            return OutCom(&stack);

        }

        int pc = RunCommand(byteCode, &stack, i);
        printf("i = %d\n", i);

        if (pc == ERR_COMMAND) {
            printf(RED "ERROR_PROCESSING\n" RESET);
            return ERR_PROCESSING;
        }
        i = i + pc;

    }
    fclose(file_p);
    return 0;

}

int RunCommand(int * byteCode, stack_t * stack, int index) {

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
        ASSERT(0);
        return 0;
    
    default:
        return ERR_COMMAND;
    }

}

//------------------------------------------------------------------
// Functions for processor
//------------------------------------------------------------------

int PushCom(int * byteCode, stack_t * stack, int index) {

    err_t pushStat = StackPush(stack, byteCode[index + 1]);

    if (pushStat != ERR_OK) return ERR_COMMAND;

    return 2;

}

// Get 2 last elems from stack
int GetTwoPop(stack_t * stack, int * a, int * b) {

    err_t popStat = ERR_OK;

    *a = (int)StackPop(stack, &popStat);
    if (popStat != ERR_OK) return ERR_COMMAND;

    *b = (int)StackPop(stack, &popStat);
    if (popStat != ERR_OK) return ERR_COMMAND;

    printf("Getted: %d, %d\n", *a, *b);

    return ERR_OK;
}

int AddCom(stack_t * stack) {

    int a = 0;
    int b = 0;

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

int SubCom(stack_t * stack) {

    int a = 0, b = 0;

    if (GetTwoPop(stack, &a, &b) != ERR_OK) return ERR_COMMAND;

    printf("Sub = %d\n", b - a);

    int pushStat = StackPush(stack, (stackElem_t)(b - a));

    if (pushStat != ERR_OK) return ERR_COMMAND;

    return 1;

}

int DivCom(stack_t * stack) {

    int a = 0, b = 0;

    if (GetTwoPop(stack, &a, &b) != ERR_OK) return ERR_COMMAND;

    printf("Div = %d\n", b / a);

    int pushStat = StackPush(stack, (stackElem_t)((double)b / (double)a));

    if (pushStat != ERR_OK) return ERR_COMMAND;

    return 1;

}

stackElem_t OutCom(stack_t * stack) {

    int outStat = ERR_OK;
    int out = StackPop(stack, &outStat);

    if (outStat != ERR_OK) return ERR_COMMAND;

    return out;

}
//------------------------------------------------------------------

int GetFilenameStd(int argc, char * argv[], FileStat * fileInfo) {

    // add more if
        if ((argc == 5) && ((!strcmp(argv[1], "--readfile")  && !strcmp(argv[3], "--printfile")) ||
                        (!strcmp(argv[1], "--printfile") && !strcmp(argv[3], "--readfile")))) {
        if (!argv[2] || !argv[4]) return false;

        if (!strcmp(argv[1], "--readfile")  && !strcmp(argv[3], "--printfile")) {

            (*fileInfo).readFrom = argv[2];
            (*fileInfo).printTo  = argv[4];

            return true;
        }

        (*fileInfo).readFrom = argv[4];
        (*fileInfo).printTo  = argv[2];
        return true;
    }
    printf("\nError of reading stdin\n");
    return false;
}

int ReadTxtFile(const char filename[], FileStat * fileInfo) {

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

    printf(RED "File: <%s>\n", filename);

    const char * errorMessage = strerror(err_inf);

    if (errorMessage) perror(errorMessage);
    printf(RESET);
}

int ReadByteCode(const FileStat * txtFile, int * codeBuffer) {

    ASSERT(txtFile);
    ASSERT(codeBuffer);

    // Count of elements that are writing to codeBuffer
    size_t count = 0;

    for (size_t i = 0; i < txtFile->nLines; i++) {

        int command = -1; // -1 using only for init, there's no real commands with code that equals -1
        int var = 0;

        int args = sscanf(txtFile->index[i], "%d %d", &command, &var);
        printf(GREEN "Line = %d, args = %d", i, args);

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