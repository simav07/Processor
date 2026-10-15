#include "./include/io.h"
#include "./include/colors.h"
#include <string.h>

err_t ReadTxtFile(const char filename[], const char mode[], FileStat * fileInfo) {

    ASSERT(filename);
    ASSERT(mode);
    ASSERT(fileInfo);

    struct _stat fileStat = {};

    // Accept "br" too, normalizing it to the standard spelling "rb".
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

    void * buffer = calloc(fileInfo->sizeInBytes + 1, sizeof(char));
    if (!buffer) {
        fclose(file_p);
        return false;
    }
    
    if (openMode != "rb") {
        size_t bytesRead = fread(buffer, sizeof(char), fileInfo->sizeInBytes, file_p);

        if (ferror(file_p)) {
            int savedErrno = errno;
            free(buffer);
            fclose(file_p);
            PrintErrno(fileInfo->readFrom, savedErrno);
            return false;
        }

        *((char *)buffer + bytesRead) = '\0';
        fileInfo->sizeInBytes = bytesRead;
        fileInfo->text_ptr = buffer;
    }

    else {

        size_t bytesRead = fread(buffer, sizeof(int32_t), fileInfo->sizeInBytes, file_p);

        if (ferror(file_p)) {
            int savedErrno = errno;
            free(buffer);
            fclose(file_p);
            PrintErrno(fileInfo->readFrom, savedErrno);
            return false;
        }

        fileInfo->sizeInBytes = bytesRead;
        fileInfo->text_ptr = buffer;

    }

    fclose(file_p);

    FillIndexes(fileInfo);

    return true;
}


//! Splits the file's strings from fileInfo.text_ptr at a specified char - "diffElem"
size_t StringsParser(char * buffer, char diffElem, StringInfo * index) {

    ASSERT(buffer);
    ASSERT(index);

    size_t indexPtr = 0;
    char * lineStart = buffer;

    if (*buffer == '\0') {
        return 0;
    }

    for (char * currPtr = buffer; ; currPtr++) {
        // Save this before modifying the current character.
        bool isEnd = (*currPtr == '\0');

        if (!isEnd && *currPtr != diffElem) {
            continue;
        }

        char * lineEnd = currPtr;

        // Windows: remove '\r' from CRLF.
        if (!isEnd && diffElem == '\n' && lineEnd > lineStart && *(lineEnd - 1) == '\r') {
            lineEnd--;
        }

        index[indexPtr].index = lineStart;
        index[indexPtr].length = (size_t)(lineEnd - lineStart);

        *lineEnd = '\0';
        indexPtr++;

        if (isEnd) {
            break;
        }

        lineStart = currPtr + 1;

        // Do not create an extra line after a trailing '\n'.
        if (*lineStart == '\0') {
            break;
        }
    }

    return indexPtr;
}


void FillIndexes(FileStat * fileInfo) {

    ASSERT(fileInfo);
    ASSERT(fileInfo->text_ptr);

    // Count lines before splitting the buffer.
    fileInfo->nLines = CountElems((const char *)fileInfo->text_ptr, '\n');

    fileInfo->index = (StringInfo *)calloc(fileInfo->nLines, sizeof(StringInfo));

    ASSERT(fileInfo->index);

    fileInfo->nLines = StringsParser((char *)fileInfo->text_ptr, '\n', fileInfo->index);
}


size_t CountElems(const char * string, int Elem) {

    ASSERT(string);

    if (*string == '\0') {
        return 0;
    }

    size_t count = 0;
    const char * curr = string;

    while (*curr != '\0') {
        if (*curr == Elem) count++;
        curr++;
    }

    if (*(curr - 1) != Elem) {
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


void PrintStrings(FILE * stream, const StringInfo * index_ptr, size_t nStrings) {

    ASSERT(index_ptr);

    for (size_t count = 0; count < nStrings; count++) {

        ASSERT(index_ptr[count].index);
        fprintf(stream, "%.*s\n", (int)index_ptr[count].length, index_ptr[count].index);
    }
}
