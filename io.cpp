#include "./include/io.h"
#include "./include/colors.h"
#include <string.h>

err_t ReadTxtFile(const char filename[], const char mode[], FileStat * fileInfo) {

    ASSERT(filename);
    ASSERT(mode);
    ASSERT(fileInfo);

    struct _stat fileStat = {};

    //Accept "br" too, normalizing it to the standard spelling "rb".
    const char * openMode = mode;
    if (strcmp(mode, "br") == 0) {
        openMode = "rb";
    }

    FILE * file_p = fopen(filename, openMode);

    if (!file_p) {
        PrintErrno(fileInfo->readFrom, errno);
        return false;
    }

    if (_stat(filename, &fileStat) == -1) {
        int savedErrno = errno;
        fclose(file_p);
        PrintErrno(fileInfo->readFrom, savedErrno);
        return false;
    }

    fileInfo->sizeInBytes = (size_t)fileStat.st_size;

    char * buffer = (char *)calloc(fileInfo->sizeInBytes + 1, sizeof(char));
    if (!buffer) {
        fclose(file_p);
        return false;
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileInfo->sizeInBytes, file_p);

    if (ferror(file_p)) {
        int savedErrno = errno;
        free(buffer);
        fclose(file_p);
        PrintErrno(fileInfo->readFrom, savedErrno);
        return false;
    }

    buffer[bytesRead] = '\0';
    fileInfo->sizeInBytes = bytesRead;
    fileInfo->text_ptr = buffer;

    fclose(file_p);

    FillIndexes(fileInfo);

    return true;
}


//! Splits the file's strings from fileInfo.text_ptr at a specified char - "diffElem"

size_t StringsParser(char * buffer, char diffElem, char ** index) {

    ASSERT(buffer);
    ASSERT(index);

    size_t indexPtr = 0;
    char * lineStart = buffer;

    index[indexPtr++] = buffer;

    for (char * currPtr = buffer; *currPtr != '\0'; currPtr++) {

        if (*currPtr != diffElem) {
            continue;
        }

        // Windows: remove '\r' from CRLF.
        if (diffElem == '\n' &&
            currPtr > lineStart &&
            *(currPtr - 1) == '\r') {

            *(currPtr - 1) = '\0';
        } else {
            *currPtr = '\0';
        }

        lineStart = currPtr + 1;

        // Do not create an extra line after a trailing '\n'.
        if (*lineStart != '\0') {
            index[indexPtr++] = lineStart;
        } else {
            break;
        }
    }

    return indexPtr;
}




void FillIndexes(FileStat * fileInfo) {

    ASSERT(fileInfo);
    ASSERT(fileInfo->text_ptr);

    // Allocate enough pointers for every possible line.
    fileInfo->nLines = CountElems(fileInfo->text_ptr, '\n') + 1;

    fileInfo->index = (char **)calloc(fileInfo->nLines, sizeof(char *));

    ASSERT(fileInfo->index);

    fileInfo->nLines = StringsParser(fileInfo->text_ptr, '\n', fileInfo->index);
}



size_t CountElems(const char * string, int Elem) {

    ASSERT(string);

    if (*string == '\0') {
        return 1;
    }

    size_t count = 0;
    const char * curr = string;

    while (*curr != '\0') {
        if (*curr == Elem) count++;
        curr++;
    }

    if (curr > string && *(curr - 1) != Elem) {
        count++;
    }

    return count;
}


void PrintErrno(const char filename[], int err_inf) {

    ASSERT(filename);

    printf(RED "File: <%s>\n", filename);

    const char * errorMessage = strerror(err_inf);

    if (errorMessage) perror(errorMessage);
    printf(RESET);
}


void PrintStrings(FILE * stream, char ** index_ptr, size_t nStrings) {

    ASSERT(index_ptr);
    ASSERT(*index_ptr);

    for (size_t count = 0; count < nStrings; count++) {

        ASSERT(index_ptr[count]);
        fprintf(stream, "%s\n", index_ptr[count]);
    }
}
