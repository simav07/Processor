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
#include "errors.h"

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

//! Commands for StackResize
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

//!-------------------------------------------------------------------------
//! LOGGING
//!-------------------------------------------------------------------------

#ifdef LOG_MODE

#define STACK_LOGGING(logfile, stk) STACK_LOG(logfile, &(stk), #stk, __FILE__, __PRETTY_FUNCTION__, __LINE__)

#define WRITE_ERROR_LOG(logfile, message) WriteErrorLog(logfile, message, __FILE__, __PRETTY_FUNCTION__)

#else
#define STACK_LOGGING(logfile, stk) (void)0

#define WRITE_ERROR_LOG (void)0
#endif

//! --------------------------------------------------------------------------
//!  Functions for STACK
//! --------------------------------------------------------------------------

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