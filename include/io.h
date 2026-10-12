#ifndef IO
#define IO
typedef int err_t;

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

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

//! ---------------------------------------------------------
//! Functions for IO
//! ---------------------------------------------------------

err_t ReadTxtFile(const char filename[], const char mode[], FileStat * fileInfo);

size_t StringsParser(char * buffer, char diffElem, char ** index);

void FillIndexes(FileStat * fileInfo);

void PrintStrings(FILE * stream, char ** index_ptr, size_t nStrings);

size_t CountElems(const char * string, int Elem);

void PrintErrno(const char filename[], int err_inf);
// --------------------------------------------------------------------

#endif