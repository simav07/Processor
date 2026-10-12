#ifndef ERRORS
#define ERRORS

//! All possible errors in this project
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
    ERR_INTPR,
    ERR_WRITEHASH,
    ERR_OVERFLOW,
};

#endif