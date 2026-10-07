#include "stack.h"

err_t CheckFile(FILE * file_p) {

    if (file_p == NULL) return ERR_FILEOPEN;

    return ERR_OK;
}

err_t StackCtor(stack_t * stk, ssize_t capacity) {

    if (stk == NULL) return ERR_NULL_STACK;

    //! Error from constructor: stack is already constructed and maybe data has been using
    if ((*stk).data != NULL || (*stk).capacity != 0 || (*stk).size != 0) {
        return ERR_CTOR;
    }
    
    stackElem_t * NewPtr = (stackElem_t *)calloc(capacity + 2, sizeof(stackElem_t));

    if (NewPtr == NULL) return ERR_CALLOC;

    stk->dataBegin = NewPtr;

    stk->data = NewPtr + stk->nCanaries;

    stk->capacity = capacity;
    stk->size = 0;

    err_t memoryInitStat = NewMemoryInit(stk, (*stk).dataBegin, (*stk).data + (*stk).capacity + stk->nCanaries);

    #ifdef CANARY
    //! Left canary
    *(stk->dataBegin) = stk->leftCanary;
    //! Right canary
    *(stk->data + stk->capacity) = stk->rightCanary;
    #endif

    STACK_HASH(stk);

    ASSERT((StackVerify(stk, memoryInitStat) == ERR_OK));

    return ERR_OK;
}

err_t NewMemoryInit(stack_t * stk, stackElem_t * firstIndex, stackElem_t * lastIndex) {

    ASSERT(firstIndex);
    ASSERT(lastIndex);
    ASSERT(stk);
    ASSERT(stk->data);

    if ((firstIndex >= lastIndex)) return ERR_MEMORY_INIT;

    stackElem_t * currIndex = firstIndex;

    for (currIndex; currIndex < lastIndex; currIndex++) {

        ASSERT(currIndex);

        *currIndex = STACK_POISON;

    }

    STACK_HASH(stk);

    return ERR_OK;
}

err_t StackDtor(stack_t * stk) {

    ASSERT(stk);

    free((*stk).dataBegin);
    stk->dataBegin = NULL;

    (*stk).capacity = 0;
    (*stk).size = 0;

    return ERR_OK;
}

err_t StackPush(stack_t * stk, stackElem_t value) {

    ASSERT((StackVerify(stk, ERR_OK) == ERR_OK));

    if ((stk->size == stk->capacity) && (stk->size != 0)) {

        err_t resizeStat = StackResize(stk, MEM_UP);
        STACK_HASH(stk);

        ASSERT((StackVerify(stk, resizeStat) == ERR_OK));

        if (resizeStat != ERR_OK) return ERR_REALLOC;

    }

    (*stk).data[(*stk).size++] = (stackElem_t)value;

    STACK_HASH(stk);

    ASSERT((StackVerify(stk, ERR_OK) == ERR_OK));

    return ERR_OK;
}

err_t StackResize(stack_t * stk, resizeCommand command) {

    if (command == MEM_UP) {

        if (stk->size != stk->capacity) return ERR_UP_RESIZE;

        stackElem_t * NewPtr = (stackElem_t *)realloc((*stk).dataBegin, 
                                    sizeof(stackElem_t)*(ssize_t)((*stk).capacity * MEMORY_UP_COEFF + 2 * stk->nCanaries));

        if (NewPtr == NULL) return ERR_REALLOC;

        stk->dataBegin = NewPtr;
        stk->data = NewPtr + stk->nCanaries;

        stk->capacity = (ssize_t)((*stk).capacity * MEMORY_UP_COEFF);

        err_t memInitStat = NewMemoryInit(stk, (*stk).data + (*stk).size, (*stk).data + (*stk).capacity);
        
        #ifdef CANARY
        //! Right canary
        stk->dataBegin[stk->capacity + 1] = stk->rightCanary;
        #endif

        STACK_HASH(stk);

        ASSERT((StackVerify(stk, memInitStat) == ERR_OK));
        return ERR_OK;
    }

    if (command == MEM_DOWN) {

        if (stk->size >= (stk->capacity / MEMORY_UP_COEFF)) return ERR_DOWN_RESIZE;

        stackElem_t * NewPtr = (stackElem_t *)realloc((*stk).dataBegin,
                                     sizeof(stackElem_t)*(ssize_t)(((*stk).capacity) / MEMORY_UP_COEFF + 2 * stk->nCanaries));

        if (NewPtr == NULL) {
            return ERR_REALLOC;
        }

        stk->dataBegin = NewPtr;
        stk->data = NewPtr + stk->nCanaries;

        stk->capacity = (ssize_t)(((*stk).capacity) / MEMORY_UP_COEFF);

        #ifdef CANARY
        //! Right canary
        stk->dataBegin[stk->capacity + 1] = stk->rightCanary;
        #endif

        STACK_HASH(stk);

        ASSERT((StackVerify(stk, ERR_OK) == ERR_OK));

        return ERR_OK;
    }

    return ERR_RESIZE;
}

stackElem_t StackPop(stack_t * stk, err_t * PopStatus) {

    ASSERT(stk);
    ASSERT((StackVerify(stk, ERR_OK) == ERR_OK));
    
    if (stk->size == 0) {StackVerify(stk, ERR_FAKE_POP_PTR); return STACK_POISON;}

    stk->size--;

    STACK_HASH(stk);

    ASSERT((StackVerify(stk, ERR_OK) == ERR_OK));

    stackElem_t currElem = stk->data[(*stk).size];

    ASSERT(currElem);
    stk->data[stk->size] = STACK_POISON;

    STACK_HASH(stk);

    ASSERT((StackVerify(stk, ERR_OK) == ERR_OK));

    //! Free up memory if there's a lot of space
    //! It works only if we have more than one realloc
    /*if (((*stk).capacity > MIN_CAPACITY * 0.5) && (*stk).size < ((*stk).capacity / 4)) {

        err_t resizeStat = StackResize(stk, MEM_DOWN);

        STACK_HASH(stk);

        ASSERT((StackVerify(stk, resizeStat) == ERR_OK));

    } */

    *PopStatus = ERR_OK;

    return currElem;
}

err_t CheckStackError(const stack_t * stk) {

    if (stk == NULL) return ERR_NULL_POINTER;

    if (stk->data == NULL) return ERR_DATA_PTR;

    if (stk->dataBegin == NULL) return ERR_DATA_BEGIN;

    if (stk->size > stk->capacity) return ERR_BAD_SIZE;

    if (stk->size < 0) return ERR_SIGN_SIZE;

    if (stk->capacity < 0) return ERR_SIGN_CAPACITY;

    #ifdef CANARY

    if (stk->leftCanary != STACK_LEFT_CANARY) return ERR_LEFT_CANARY;

    if (stk->rightCanary != STACK_RIGHT_CANARY) return ERR_RIGHT_CANARY;

    if (stk->dataBegin[stk->capacity + 1] != stk->rightCanary) return ERR_RIGHT_CANARY;

    if (stk->dataBegin[0] != stk->leftCanary) return ERR_LEFT_CANARY;

    #endif
    
    #ifdef HASH
    if (DjbHash(stk->data, (sizeof(stackElem_t) * (stk->size))) != stk->hash) return ERR_HASH;
    #endif

    return ERR_OK;
}

err_t StackVerify(const stack_t * stk, err_t ERR_CODE) {

    if (ERR_CODE == ERR_OK) {
        ERR_CODE = CheckStackError(stk);
    }
    if (ERR_CODE == ERR_OK) {
        return ERR_OK;
    }

    const char * err_descr = ErrorDescription(ERR_CODE);

    fprintf(stderr, RED "%s\n" RESET, err_descr);
    WRITE_ERROR_LOG(LOGFILE, err_descr);
    STACK_LOGGING(LOGFILE, (*stk));
    return ERR_CODE;
}

const char * ErrorDescription(err_t errCode) {

    switch (errCode)
    {
    case ERR_OK:
        return "ERR_OK: All good";

    case ERR_CTOR:
        return "ERR_CTOR: Getted stack was already constructed";

    case ERR_NULL_STACK:
        return "ERR_NULL_STACK: Getted null-pointer to stack";

    case ERR_CALLOC:
        return "ERR_CALLOC: If calloc returned NULL";

    case ERR_REALLOC:
        return "ERR_REALLOC: If realloc returned NULL";

    case ERR_CAPACITY:
        return "ERR_CAPACITY: Incorrect capacity value";

    case ERR_FILEOPEN:
        return "ERR_FILEOPEN: fopen returned NULL";

    case ERR_BAD_SIZE:
        return "ERR_BAD_SIZE: The size is more than capacity";

    case ERR_ZERO_SIZE:
        return "ERR_ZERO_SIZE: Stack try to return value but size is zero";

    case ERR_SIGN_SIZE:
        return "ERR_SIGN_SIZE: The size is less than zero";

    case ERR_DATA_PTR:
        return "ERR_DATA_PTR: Pointer to the stack if null, but stack already constructed";

    case ERR_SIGN_CAPACITY:
        return "ERR_SIGN_CAPACITY: The capacity is less than zero";

    case ERR_FAKE_POP_PTR:
        return "ERR_FAKE_POP_PTR: Stack try to return poison value";

    case ERR_MEMORY_INIT:
        return "ERR_MEMORY_INIT: Incorrect pointers for the place of new memory";

    case ERR_NULL_POINTER:
        return "ERR_NULL_POINTER: Getted null-pointer to stack";

    case ERR_RESIZE:
        return "ERR_RESIZE: Error by StackResize";

    case ERR_UP_RESIZE:
        return "ERR_UP_RESIZE: Size != capacity but program try to increase stack";

    case ERR_DOWN_RESIZE:
        return "ERR_DOWN_RESIZE: Size is pretty big but program try to reduce stack";

    case ERR_RIGHT_CANARY:
        return "ERR_RIGHT_CANARY: Right canary was not defined";
    
    case ERR_LEFT_CANARY:
        return "ERR_LEFT_CANARY: Left canary was not defined";

    case ERR_DATA_BEGIN:
        return "ERR_DATA_BEGIN: Getted null-pointer to begin of data";

    case ERR_HASH:
        return "ERR_HASH: Stack data was changed";
    
    default:
        ASSERT(0);
        return "UNKNOWN ERROR: Unknown error code";
    }
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

size_t StackHash(stack_t *stk) {

    #ifdef HASH
    ASSERT(stk);
    ASSERT((stk->size >= 0));
    ASSERT((stk->data != NULL));
    
    stk->hash = DjbHash(stk->data, sizeof(stackElem_t) * stk->size);

    return stk->hash;

    #endif
    return false;
}


void StackDump(FILE * stream, const stack_t * stk) {

    ASSERT(stream);

    if (stk == NULL) {
        fprintf(stream, "ATTENTION! POINTER TO STACK IS NULL\n");
    }

    #ifdef HASH

    fprintf(stream, "[HASH VALUE = %zx]\n", stk->hash);

    #endif

    fprintf(stream, "Capacity = %llu\n", stk->capacity);
    fprintf(stream, "Size = %zd", stk->size);
    fprintf(stream, "\ndata[%p]\n{\n", stk->data);

    if (stk->dataBegin == NULL) {
        fprintf(stream, "Data is NULL, there is no elements\n");
        return;
    }

    for (ssize_t i = 0; i < stk->size + stk->nCanaries; i++) {

        #ifdef CANARY
        //! Print left canary
        if (i == 0) {
            fprintf(stream, "\t[canary] = %X\n", (unsigned int)stk->dataBegin[i]);
            continue;
        }
        #endif

        fprintf(stream, "\t*[%zd] = " STACK_ELEM_FORMAT "\n", i, stk->dataBegin[i]);
    }

    for (ssize_t i = stk->size + stk->nCanaries; i < stk->capacity + (2 * stk->nCanaries); i++) {

        #ifdef CANARY
        //! Print right canary
        if (i == stk->capacity + 1) {
            fprintf(stream, "\t[canary] = %X\n", (unsigned int)stk->dataBegin[i]);
            break;
        }
        #endif

        fprintf(stream, "\t [%zd] = " STACK_ELEM_FORMAT "\n", i, stk->dataBegin[i]);
    }

    fprintf(stream, "}");
}

void STACK_LOG(FILE * logFile, const stack_t * stk, const char stkName[], const char * fileName,
                const char *funcName, unsigned int nLine) {

    ASSERT(stk);
    ASSERT(fileName);
    ASSERT(funcName);
    ASSERT(stkName);

    if (!logFile) {
        fprintf(stderr, "Error of opening file: <%s>\n", LOGFILE_NAME); 
        return;
    }

    fprintf(logFile, "\n--------------------------------------------------------\n");
    fprintf(logFile, "stack_t \"%s\"", stkName);
    fprintf(logFile, " created by function <%s> in <%s>. Line: %u\n", funcName, fileName, nLine);

    StackDump(logFile, stk);

    //! Forced writing to disk 
    fflush(logFile);
}

void WriteErrorLog(FILE * logfile, const char * message, const char * fileName, const char *funcName) {

    ASSERT(logfile);
    ASSERT(fileName);
    ASSERT(funcName);

    fprintf(logfile, "\n-------------------WARNING------------------------------");
    fprintf(logfile, "\nCalled from <%s> in <%s>.\n", funcName, fileName);

    fprintf(logfile, "\nVerified error: %s\n", message);
    fprintf(logfile, "--------------------------------------------------------\n");
    fprintf(logfile, "\n");
}