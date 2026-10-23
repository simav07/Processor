#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "include/errors.h"
#include "include/commands.h"
#include "include/io.h"

const char * CMD_FLAG_BYTECODE = "--bytecode";

const uint8_t COMMAND_MASK = 0x1F;
const uint8_t CONST_MASK   = 0x20;
const uint8_t REG_MASK     = 0x40;
const uint8_t RAM_MASK     = 0x80;

struct ByteCode {
    uint8_t * data;
    size_t size;
    size_t * labels;
};

err_t ArgvParser(const int argc, const char * argv[], const char ** byteCodeFile);
err_t Disassemble(const char * byteCodeFile);
err_t ReadByteCode(const char * byteCodeFile, ByteCode * byteCode);
err_t GetInstructionSize(const uint8_t * byteCode, size_t byteCodeLen,
                         size_t address, size_t * instrSize);
err_t MarkLabels(const uint8_t * byteCode, size_t byteCodeLen, size_t * labelAddress);
err_t WriteByteCode(const uint8_t * byteCode, size_t byteCodeLen, const size_t * labelAddress);
err_t WriteInstruction(uint8_t commandCode, uint8_t flags, int32_t arg1, int32_t arg2, size_t label);
err_t ReadInstructionArguments(const uint8_t * byteCode, size_t address, uint8_t flags,
                               int32_t * arg1, int32_t * arg2);
const char * GetCommandName(uint8_t commandCode);
const char * GetRegisterName(int32_t regIndex);
bool IsPushPop(uint8_t commandCode);
bool IsJump(uint8_t commandCode);

int main(const int argc, const char * argv[]) {

    const char * byteCodeFile = NULL;

    int filenamesStat = ArgvParser(argc, argv, &byteCodeFile);
    if (filenamesStat != ERR_OK) {
        fprintf(stderr, "Usage: disassembler --bytecode <file>\n");
        return filenamesStat;
    }

    int disassembleStat = Disassemble(byteCodeFile);
    if (disassembleStat != ERR_OK) {
        fprintf(stderr, "Error of disassembling byte code\n");
        return disassembleStat;
    }

    return ERR_OK;
}

err_t ArgvParser(const int argc, const char * argv[], const char ** byteCodeFile) {

    ASSERT(argv);
    ASSERT(byteCodeFile);

    if (argc != 3 || strcmp(argv[1], CMD_FLAG_BYTECODE) != 0) {
        return ERR_FILEOPEN;
    }

    *byteCodeFile = argv[2];

    return ERR_OK;
}

err_t Disassemble(const char * byteCodeFile) {

    ASSERT(byteCodeFile);

    ByteCode byteCode = {};

    int readStat = ReadByteCode(byteCodeFile, &byteCode);
    if (readStat != ERR_OK) return readStat;

    byteCode.labels = (size_t *)calloc(byteCode.size + 1, sizeof(size_t));
    if (byteCode.labels == NULL) {
        free(byteCode.data);
        return ERR_CTOR;
    }

    int labelsStat = MarkLabels(byteCode.data, byteCode.size, byteCode.labels);
    if (labelsStat == ERR_OK) {
        labelsStat = WriteByteCode(byteCode.data, byteCode.size, byteCode.labels);
    }

    free(byteCode.labels);
    free(byteCode.data);

    return labelsStat;
}

err_t ReadByteCode(const char * byteCodeFile, ByteCode * byteCode) {

    ASSERT(byteCodeFile);
    ASSERT(byteCode);

    FILE * fileByteCode = fopen(byteCodeFile, "rb");
    if (fileByteCode == NULL) return ERR_FILEOPEN;

    if (fseek(fileByteCode, 0, SEEK_END) != 0) {
        fclose(fileByteCode);
        return ERR_BYTECODE;
    }

    long fileSize = ftell(fileByteCode);
    if (fileSize < 0 || fseek(fileByteCode, 0, SEEK_SET) != 0) {
        fclose(fileByteCode);
        return ERR_BYTECODE;
    }

    byteCode->size = (size_t)fileSize;
    byteCode->data = (uint8_t *)calloc(byteCode->size + 1, sizeof(uint8_t));
    if (byteCode->data == NULL) {
        fclose(fileByteCode);
        return ERR_CTOR;
    }

    if (fread(byteCode->data, sizeof(uint8_t), byteCode->size, fileByteCode) != byteCode->size) {
        free(byteCode->data);
        byteCode->data = NULL;
        fclose(fileByteCode);
        return ERR_BYTECODE;
    }

    fclose(fileByteCode);

    return ERR_OK;
}

err_t GetInstructionSize(const uint8_t * byteCode, size_t byteCodeLen,
                         size_t address, size_t * instrSize) {

    ASSERT(byteCode);
    ASSERT(instrSize);

    if (address >= byteCodeLen) return ERR_BYTECODE;

    uint8_t flags = byteCode[address];
    uint8_t commandCode = flags & COMMAND_MASK;

    *instrSize = 1;

    if (commandCode == SKIP_STEP) return ERR_BYTECODE;

    if (IsPushPop(commandCode)) {
        if (flags & REG_MASK) *instrSize += sizeof(int32_t);
        if (flags & CONST_MASK) *instrSize += sizeof(int32_t);
    }
    else if (IsJump(commandCode)) {
        *instrSize += sizeof(int32_t);
    }
    else if (flags & (CONST_MASK | REG_MASK | RAM_MASK)) {
        return ERR_BYTECODE;
    }

    if (*instrSize > byteCodeLen - address) return ERR_BYTECODE;

    return ERR_OK;
}

err_t MarkLabels(const uint8_t * byteCode, size_t byteCodeLen, size_t * labelAddress) {

    ASSERT(byteCode);
    ASSERT(labelAddress);

    size_t address = 0;
    size_t nLabels = 0;

    while (address < byteCodeLen) {

        size_t instrSize = 0;
        int sizeStat = GetInstructionSize(byteCode, byteCodeLen, address, &instrSize);
        if (sizeStat != ERR_OK) return sizeStat;

        uint8_t commandCode = byteCode[address] & COMMAND_MASK;

        if (IsJump(commandCode)) {
            int32_t target = 0;
            memcpy(&target, byteCode + address + 1, sizeof(target));

            if (target < 0 || (size_t)target >= byteCodeLen) return ERR_BYTECODE;

            if (labelAddress[target] == 0) {
                labelAddress[target] = ++nLabels;
            }
        }

        address += instrSize;
    }

    return ERR_OK;
}

err_t WriteByteCode(const uint8_t * byteCode, size_t byteCodeLen, const size_t * labelAddress) {

    ASSERT(byteCode);
    ASSERT(labelAddress);

    size_t address = 0;

    while (address < byteCodeLen) {

        if (labelAddress[address] != 0) {
            printf(":%zu\n", labelAddress[address]);
        }

        uint8_t flags = byteCode[address];
        uint8_t commandCode = flags & COMMAND_MASK;
        size_t instrSize = 0;

        int sizeStat = GetInstructionSize(byteCode, byteCodeLen, address, &instrSize);
        if (sizeStat != ERR_OK) return sizeStat;

        int32_t arg1 = 0;
        int32_t arg2 = 0;

        if (IsPushPop(commandCode)) {
            int argsStat = ReadInstructionArguments(byteCode, address, flags, &arg1, &arg2);
            if (argsStat != ERR_OK) return argsStat;
        }
        else if (IsJump(commandCode)) {
            memcpy(&arg1, byteCode + address + 1, sizeof(arg1));
        }

        size_t label = 0;
        if (IsJump(commandCode) && arg1 >= 0 && (size_t)arg1 < byteCodeLen) {
            label = labelAddress[arg1];
        }

        int writeStat = WriteInstruction(commandCode, flags, arg1, arg2, label);
        if (writeStat != ERR_OK) return writeStat;

        address += instrSize;
    }

    return ERR_OK;
}

err_t ReadInstructionArguments(const uint8_t * byteCode, size_t address, uint8_t flags,
                               int32_t * arg1, int32_t * arg2) {

    ASSERT(byteCode);
    ASSERT(arg1);
    ASSERT(arg2);

    size_t argIndex = address + 1;

    if (flags & REG_MASK) {
        memcpy(arg1, byteCode + argIndex, sizeof(*arg1));
        argIndex += sizeof(int32_t);
    }

    if (flags & CONST_MASK) {
        int32_t value = 0;
        memcpy(&value, byteCode + argIndex, sizeof(value));

        if (flags & REG_MASK) *arg2 = value;
        else *arg1 = value;
    }

    return ERR_OK;
}

err_t WriteInstruction(uint8_t commandCode, uint8_t flags, int32_t arg1, int32_t arg2, size_t label) {

    const char * commandName = GetCommandName(commandCode);
    if (commandName == NULL) return ERR_COMMAND;

    printf("%s", commandName);

    if (IsPushPop(commandCode)) {

        if (!(flags & (CONST_MASK | REG_MASK | RAM_MASK))) {
            printf("\n");
            return ERR_BYTECODE;
        }

        if (commandCode == POP_C && !(flags & RAM_MASK) &&
            (!(flags & REG_MASK) || (flags & CONST_MASK))) {
            printf("\n");
            return ERR_BYTECODE;
        }

        printf(" ");

        if (flags & RAM_MASK) printf("[");

        if (flags & REG_MASK) {
            const char * regName = GetRegisterName(arg1);
            if (regName == NULL) {
                printf("\n");
                return ERR_COMMAND;
            }

            printf("%s", regName);
        }

        if (flags & CONST_MASK) {
            if (flags & REG_MASK) printf("+");
            printf("%d", (flags & REG_MASK) ? arg2 : arg1);
        }

        if (flags & RAM_MASK) printf("]");

        printf("\n");
        return ERR_OK;
    }

    if (IsJump(commandCode)) {
        if (label == 0) return ERR_BYTECODE;
        printf(" :%zu\n", label);
        return ERR_OK;
    }

    if (flags & (CONST_MASK | REG_MASK | RAM_MASK)) return ERR_BYTECODE;

    printf("\n");

    return ERR_OK;
}

bool IsPushPop(uint8_t commandCode) {
    return commandCode == PUSH_C || commandCode == POP_C;
}

bool IsJump(uint8_t commandCode) {

    return commandCode == JUMP_C || commandCode == JA_C  ||
           commandCode == JAE_C  || commandCode == JB_C  ||
           commandCode == JBE_C  || commandCode == JE_C  ||
           commandCode == JNE_C  || commandCode == CALL_C;
}

const char * GetCommandName(uint8_t commandCode) {

    switch (commandCode) {

        case PUSH_C: return "PUSH";
        case ADD_C:  return "ADD";
        case SUB_C:  return "SUB";
        case DIV_C:  return "DIV";
        case MUL_C:  return "MUL";
        case POP_C:  return "POP";
        case JUMP_C: return "JUMP";
        case JA_C:   return "JA";
        case JAE_C:  return "JAE";
        case JB_C:   return "JB";
        case JBE_C:  return "JBE";
        case JE_C:   return "JE";
        case JNE_C:  return "JNE";
        case IN_C:   return "IN";
        case CALL_C: return "CALL";
        case RET_C:  return "RET";
        case OUT_C:  return "OUT";
        case HLT_C:  return "HLT";

        default:     return NULL;
    }
}

const char * GetRegisterName(int32_t regIndex) {

    switch (regIndex) {

        case 0: return "OX";
        case 1: return "AX";
        case 2: return "BX";
        case 3: return "CX";
        case 4: return "DX";

        default: return NULL;
    }
}
