typedef int err_t;

#include "./include/io.h"
#include "./include/colors.h"
#include "./include/errors.h"

const char * COMMANDS_SOURCE    = "comsrc.txt";     //! File with commands for assembler
const char * HASH_COMMANDS_FILE = "hashcommands.h"; //! Auto-generated file with enum of hash values

err_t WriteHashCom(const char * source, const char * commandsFile);
size_t DjbHash(const void *data, size_t size);

int main() {

    printf("\nFile <%s>, [%s]\n", __FILE__, __TIME__);

    //! Get hash values of using commands
    int hashStat = WriteHashCom(COMMANDS_SOURCE, HASH_COMMANDS_FILE);
    if (hashStat != ERR_OK) {
        // error description
        printf(RED "Error of getting hash values of commands\n" RESET);
        return ERR_ASM;
    }
    printf(BOLD_GREEN "Successful generating of hash values.\n"
               YELLOW "File with hash_commands: <%s>\n" RESET, HASH_COMMANDS_FILE);

    return true;
}

err_t WriteHashCom(const char * source, const char * commandsFile) {

    FileStat sourceFile = {};

    int readStat = ReadTxtFile(source, "r", &sourceFile);
    if (!readStat) {
        printf(BOLD_RED "Error of reading file <%s>\n" RESET);
        return ERR_FILEOPEN;
    }

    FILE * fileForWriting = fopen(commandsFile, "w");

    //! Making a enum with hash values of all commands in the "commandsFile"

    fprintf(fileForWriting, "// There's all hash values of all commands\n");
    fprintf(fileForWriting, "#ifndef HASH_COMMANDS\n#define HASH_COMMANDS\n\n");
    fprintf(fileForWriting, "enum HASH_COMMANDS {\n");

    for (size_t i = 0; i < sourceFile.nLines; i++) {

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

size_t DjbHash(const void *data, size_t size) {

    ASSERT(data);

    const unsigned char *ptr = (const unsigned char *)data;
    ASSERT(ptr);

    size_t hash = 5381;

    for (size_t i = 0; i < size; i++)
        
        hash = hash * 33 + ptr[i];

    return hash;
}