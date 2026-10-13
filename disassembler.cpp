typedef int err_t;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#include "./include/errors.h"
#include "./include/colors.h"
#include "./include/commands.h"

const char *CMD_FLAG_BYTECODE = "--bytecode";
const char *PROG_ASM_FILE = "disassembled.asm";
const int MAX_MARKS = 20;

struct DisasmFilesInfo {
    const char *readFrom = NULL;
    const char *writeTo = NULL;
};

struct Instruction {
    int code = 0;
    int arg = 0;
    int hasArg = 0;
    long offset = 0;       // позиция кода команды в потоке числовых значений
};

struct Mark {
    long offset = 0;
    int number = 0;
};

err_t GetDisasmFilenames(int argc, char *argv[], DisasmFilesInfo *info);
err_t Disassemble(const char *byteCodeName, const char *asmName);
int CommandHasArgument(int code);
const char *GetRegisterName(int regCode);
const char *GetCommandName(int code);
int FindMarkByOffset(const Mark *marks, int nMarks, long offset);
err_t ReadInstructions(FILE *file, Instruction **instructions, size_t *nInstructions);
err_t WriteAsm(FILE *file, const Instruction *instructions, size_t nInstructions,
               const Mark *marks, int nMarks);

int main(int argc, char *argv[]) {
    printf("\nFile <%s>, [%s]\n", __FILE__, __TIME__);

    DisasmFilesInfo info = {};

    if (GetDisasmFilenames(argc, argv, &info) != ERR_OK) {
        printf("Usage: %s %s <bytecode-file> [asm-output-file]\n",
               argv[0], CMD_FLAG_BYTECODE);
        return ERR_ASM;
    }

    err_t status = Disassemble(info.readFrom, info.writeTo);
    if (status != ERR_OK) {
        printf(BOLD_RED "Disassembling failed.\n" RESET);
        return status;
    }

    printf(BOLD_GREEN "Successful disassembling.\n" RESET);
    printf(YELLOW "Assembly source: <%s>\n" RESET, info.writeTo);
    return 0;
}

err_t GetDisasmFilenames(int argc, char *argv[], DisasmFilesInfo *info) {
    if (argv == NULL || info == NULL) return ERR_ASM;

    if ((argc == 3 || argc == 4) &&
        strcmp(argv[1], CMD_FLAG_BYTECODE) == 0 &&
        argv[2] != NULL && argv[2][0] != '\0') {
        info->readFrom = argv[2];
        info->writeTo = (argc == 4) ? argv[3] : PROG_ASM_FILE;

        if (strcmp(info->readFrom, info->writeTo) == 0) {
            printf("Input and output filenames must be different.\n");
            return ERR_ASM;
        }
        return ERR_OK;
    }

    return ERR_ASM;
}

/*
 * В bytecode, который создаёт данный assembler, значения записываются
 * через fprintf("%d ..."), поэтому читаем их как текстовые целые числа.
 */
err_t ReadInstructions(FILE *file, Instruction **instructions, size_t *nInstructions) {
    if (file == NULL || instructions == NULL || nInstructions == NULL)
        return ERR_ASM;

    size_t capacity = 16;
    size_t count = 0;
    Instruction *data = (Instruction *)calloc(capacity, sizeof(Instruction));
    if (data == NULL) return ERR_CALLOC;

    long offset = 0;
    int code = 0;

    while (fscanf(file, "%d", &code) == 1) {
        if (count == capacity) {
            if (capacity > SIZE_MAX / 2 / sizeof(Instruction)) {
                free(data);
                return ERR_OVERFLOW;
            }

            capacity *= 2;
            Instruction *newData =
                (Instruction *)realloc(data, capacity * sizeof(Instruction));
            if (newData == NULL) {
                free(data);
                return ERR_CALLOC;
            }
            data = newData;
        }

        data[count].code = code;
        data[count].offset = offset;
        data[count].hasArg = CommandHasArgument(code);

        if (data[count].hasArg) {
            if (fscanf(file, "%d", &data[count].arg) != 1) {
                printf("Bytecode ends before the argument of command %d.\n", code);
                free(data);
                return ERR_ASM;
            }
        }

        offset += data[count].hasArg ? 2 : 1;
        ++count;
    }

    if (!feof(file)) {
        printf("Bytecode contains an invalid value.\n");
        free(data);
        return ERR_ASM;
    }

    *instructions = data;
    *nInstructions = count;
    return ERR_OK;
}

int CommandHasArgument(int code) {
    switch (code) {
        case PUSH_C:
        case PUSH_R:
        case POP_R:
        case JUMP_C:
        case JA_C:
            return 1;

        default:
            return 0;
    }
}

const char *GetRegisterName(int regCode) {
    switch (regCode) {
        case 0: return "OX";
        case 1: return "AX";
        case 2: return "BX";
        case 3: return "CX";
        case 4: return "DX";
        default: return NULL;
    }
}

const char *GetCommandName(int code) {
    switch (code) {
        case PUSH_C: return "PUSH";
        case PUSH_R: return "PUSH";
        case POP_R:  return "POP";
        case JUMP_C: return "JUMP";
        case JA_C:   return "JA";
        case ADD_C:  return "ADD";
        case SUB_C:  return "SUB";
        case DIV_C:  return "DIV";
        case OUT_C:  return "OUT";
        case HLT_C:  return "HLT";
        case MUL_C:  return "MUL";
        default:     return NULL;
    }
}

int FindMarkByOffset(const Mark *marks, int nMarks, long offset) {
    for (int i = 0; i < nMarks; ++i) {
        if (marks[i].offset == offset) return marks[i].number;
    }
    return -1;
}

err_t WriteAsm(FILE *file, const Instruction *instructions, size_t nInstructions,
               const Mark *marks, int nMarks) {
    if (file == NULL) return ERR_ASM;

    for (size_t i = 0; i < nInstructions; ++i) {
        int markNumber = FindMarkByOffset(marks, nMarks, instructions[i].offset);
        if (markNumber >= 0) {
            fprintf(file, ":%d\n", markNumber);
        }

        const char *name = GetCommandName(instructions[i].code);
        if (name == NULL) {
            printf("Unknown command code %d at offset %ld.\n",
                   instructions[i].code, instructions[i].offset);
            return ERR_INTPR;
        }

        fprintf(file, "%s", name);

        if (instructions[i].hasArg) {
            int code = instructions[i].code;

            if (code == PUSH_R || code == POP_R) {
                const char *regName = GetRegisterName(instructions[i].arg);
                if (regName == NULL) {
                    printf("Invalid register code %d at offset %ld.\n",
                           instructions[i].arg, instructions[i].offset);
                    return ERR_REGISTER;
                }
                fprintf(file, " %s", regName);
            } else if (code == JUMP_C || code == JA_C) {
                int targetMark = FindMarkByOffset(marks, nMarks, instructions[i].arg);
                if (targetMark >= 0) {
                    fprintf(file, " :%d", targetMark);
                } else {
                    /*
                     * Если адрес перехода не совпадает с началом инструкции,
                     * сохраняем числовой адрес. Для повторной сборки такой
                     * переход потребует поддержки числовых адресов в assembler.
                     */
                    fprintf(file, " %d", instructions[i].arg);
                }
            } else {
                fprintf(file, " %d", instructions[i].arg);
            }
        }

        fputc('\n', file);
    }

    /*
     * A jump may target the position immediately after the final instruction.
     * Emit such labels too, although the current assembler's zero-initialized
     * label table cannot distinguish an undefined label from a label at offset 0.
     */
    long endOffset = 0;
    if (nInstructions > 0) {
        const Instruction *last = &instructions[nInstructions - 1];
        endOffset = last->offset + (last->hasArg ? 2 : 1);
    }

    int endMark = FindMarkByOffset(marks, nMarks, endOffset);
    if (endMark >= 0) {
        fprintf(file, ":%d\n", endMark);
    }

    return ferror(file) ? ERR_INTPR : ERR_OK;
}

err_t Disassemble(const char *byteCodeName, const char *asmName) {
    if (byteCodeName == NULL || asmName == NULL ||
        strcmp(byteCodeName, asmName) == 0) {
        return ERR_ASM;
    }

    FILE *input = fopen(byteCodeName, "r");
    if (input == NULL) {
        perror("Cannot open bytecode file");
        return ERR_ASM;
    }

    Instruction *instructions = NULL;
    size_t nInstructions = 0;
    err_t status = ReadInstructions(input, &instructions, &nInstructions);
    fclose(input);

    if (status != ERR_OK) {
        free(instructions);
        return status;
    }

    /*
     * Собираем адреса переходов и назначаем им метки :0, :1, ...
     * В текущем проекте assembler хранит не более 20 меток.
     */
    Mark marks[MAX_MARKS] = {};
    int nMarks = 0;

    for (size_t i = 0; i < nInstructions; ++i) {
        int code = instructions[i].code;
        if (code != JUMP_C && code != JA_C) continue;

        long target = instructions[i].arg;
        long codeEnd = 0;
        if (nInstructions > 0) {
            const Instruction *last = &instructions[nInstructions - 1];
            codeEnd = last->offset + (last->hasArg ? 2 : 1);
        }

        if (target < 0 || target > codeEnd) {
            printf("Warning: jump target %ld is outside bytecode range [0, %ld].\n",
                   target, codeEnd);
        }

        int alreadyAdded = 0;

        for (int j = 0; j < nMarks; ++j) {
            if (marks[j].offset == target) {
                alreadyAdded = 1;
                break;
            }
        }

        if (!alreadyAdded && nMarks < MAX_MARKS) {
            marks[nMarks].offset = target;
            marks[nMarks].number = nMarks;
            ++nMarks;
        }
    }

    FILE *output = fopen(asmName, "w");
    if (output == NULL) {
        perror("Cannot open output assembly file");
        free(instructions);
        return ERR_ASM;
    }

    status = WriteAsm(output, instructions, nInstructions, marks, nMarks);

    if (fclose(output) != 0 && status == ERR_OK) status = ERR_INTPR;
    free(instructions);
    return status;
}
