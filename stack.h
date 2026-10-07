#ifndef STACK
#define STACK

typedef int err_t;
typedef double stackElem_t;
#define STACK_ELEM_FORMAT  "%lg"
#define STACK_POISON NAN

#define STACK_LEFT_CANARY  0xDEADBABE
#define STACK_RIGHT_CANARY 0xCAFEBABE

#ifdef HASH
#define STACK_HASH(stk) StackHash(stk)

#else
#define STACK_HASH(stk) (void)0

#endif

#include "colors.h"

#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>

//! All possible errors this stack working
enum {
    INIT_NUM = 100,     //! Starting value for protecting

    ERR_OK,             //! All good
    ERR_CTOR,           //! Getted stack was already constructed
    ERR_NULL_STACK,     //! Getted null-pointer to stack
    ERR_CALLOC,         //! If calloc returned NULL
    ERR_REALLOC,        //! If realloc returned NULL
    ERR_CAPACITY,       //! Incorrect capacity value
    ERR_FILEOPEN,       //! fopen returned NULL
    ERR_BAD_SIZE,       //! The size is more than capacity
    ERR_ZERO_SIZE,      //! Stack try to return value but size is zero
    ERR_SIGN_SIZE,      //! The size is less than zero
    ERR_DATA_PTR,       //! Pointer to the stack if null, but stack already constructed
    ERR_SIGN_CAPACITY,  //! The capacity is less than zero
    ERR_FAKE_POP_PTR,   //! Stack try to return poison value
    ERR_MEMORY_INIT,    //! Incorrect pointers for the place of new memory
    ERR_NULL_POINTER,   //! Getted null-pointer to stack
    ERR_DATA_BEGIN,     //! Getted null-pointer to begin of data
    ERR_RESIZE,         //! Error by StackResize
    ERR_UP_RESIZE,      //! Size != capacity but program try to increase stack
    ERR_DOWN_RESIZE,    //! Size is pretty big but program try to reduce stack
    ERR_LEFT_CANARY,    //! Left canary was not defined
    ERR_RIGHT_CANARY,   //! Right canary was not defined
    ERR_HASH,           //! Stack data was changed

//---------------------------------------------------------
// Errors with processor

    ERR_BYTECODE,
    ERR_COMMAND,     // False command in byte code
    ERR_PROCESSING,
    ERR_PUSH_COM,
    ERR_ADD_COM,
    ERR_DIV_COM,
    ERR_OUT_PROCESSING,
    ERR_ASM,
};

//! Commands for StackResize()
enum resizeCommand {
    
    MEM_UP   = 10, //! Increase memory
    MEM_DOWN = 20, //! Reduce memory

};

//! Stack and all using parameters
struct stack_t {

    #ifdef CANARY
    //! Left canary
    const stackElem_t leftCanary = STACK_LEFT_CANARY;
    #endif

    stackElem_t * data = NULL;
    stackElem_t * dataBegin = NULL;
    ssize_t size = 0;
    ssize_t capacity = 0;

    #ifdef CANARY
    //! Number of canaries at one side of stack
    const size_t nCanaries = 1;

    //! Right canary
    const stackElem_t rightCanary = STACK_RIGHT_CANARY;

    #else
    const size_t nCanaries = 0;
    #endif

    #ifdef HASH
    size_t hash = 0;
    #endif
};

//! Filename for logging
const char LOGFILE_NAME[] = "MyLogfile.txt";

#ifdef MYDEBUG

#define ASSERT(right_instr) do { \
    if (!right_instr) { \
        fprintf(stderr, "\nAssertion failed: (%s), file <%s>, line: %d\n\n", #right_instr, __FILE__, __LINE__); \
        printf("Current function: %s\n", __PRETTY_FUNCTION__);\
        printf(RED "Link to the line:\n" RESET);          \
        printf("File: " __FILE__ ":%d:1:\n\n", __LINE__); \
        printf(RED "--------------------------------------------------" RESET); \
        abort(); \
    } \
} while (0)
#else
#define ASSERT(right_instr) (void)0
#endif

//-------------------------------------------------------------------------
// LOGGING
//-------------------------------------------------------------------------

#ifdef LOG_MODE

#define STACK_LOGGING(logfile, stk) STACK_LOG(logfile, &(stk), #stk, __FILE__, __PRETTY_FUNCTION__, __LINE__)

#define WRITE_ERROR_LOG(logfile, message) WriteErrorLog(logfile, message, __FILE__, __PRETTY_FUNCTION__)

#else
#define STACK_LOGGING(logfile, stk) (void)0

#define WRITE_ERROR_LOG (void)0

#endif

// --------------------------------------------------------------------------
//  Functions
// --------------------------------------------------------------------------

//! Minimal capacity for increasing stack
const ssize_t MIN_CAPACITY = 5;

//! Coefficient for increase in several times
const int MEMORY_UP_COEFF = 2;

//! Stack initialisation
err_t StackCtor(stack_t * stk, ssize_t capacity);

err_t NewMemoryInit(stack_t * stk, stackElem_t * firstIndex, stackElem_t * lastIndex);

//! Stack distraction
err_t StackDtor(stack_t * stk);

//! Standard push
err_t StackPush(stack_t * stk, stackElem_t value);
//! Standard pop
stackElem_t StackPop(stack_t * stk, err_t * PopStatus);

//! Change memory size
//! command: MEM_UP for increase, MEM_DOWN for reduce
err_t StackResize(stack_t * stk, resizeCommand command);

//! Checking all possibly stack errors
err_t CheckStackError(const stack_t * stk);

//! Get error description
const char * ErrorDescription(err_t errCode);

//! Verification of all errors
err_t StackVerify(const stack_t * stk, err_t ERR_CODE);

//! Calculate hash value
size_t DjbHash(const void *data, size_t size);

//! Update hash value
size_t StackHash(stack_t *stk);

//! Pointer to LogFile
static FILE * LOGFILE = NULL;

err_t CheckFile(FILE * file_p);

//! Maximum complete stack printout
void StackDump(FILE * stream, const stack_t * stk);

//! Printout to the logfile (using StackDump)
void STACK_LOG(FILE * logFile, const stack_t * stk, const char stkName[], const char * fileName, const char *funcName, unsigned int nLine);

//! Write verified by StackVerify error to the logfile
void WriteErrorLog(FILE * file_p, const char * message, const char * fileName, const char *funcName);

#endif