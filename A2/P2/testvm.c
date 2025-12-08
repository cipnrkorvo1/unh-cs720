#include "vmx20.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void failAddr(char *arg) { printf("failed to retrieve address for %s\n", arg); }
void failGet(int addr) { printf("failed to get word at addr %d\n", addr); }
void failPut(int addr) { printf("failed to put word at addr %d\n", addr); }

#define PRINT_RESULTS 0
#define TEST_DISASSEMBLE 0

int testDisassemble(void *handle, char *filename)
{
    printf("\ntestDisassemble start\n");
    int success = 1;
    int err = 0;
    if (!loadExecutableFile(handle, filename, &err))
    {
        fprintf(stderr, "Fatal error: exiting with code %d\n", err);
        exit(err);
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) return fprintf(stderr, "Failed to open file\n") & 0;
    fseek(fp, 2*4, SEEK_SET);
    int32_t maxAddress;
    fread(&maxAddress, sizeof(int32_t), 1, fp);
    fclose(fp);

    char *buffer = malloc(100);
    for (int i = 0; i < maxAddress; i++)
    {
        if (!disassemble(handle, i, buffer, &err))
        {
            printf("Disassemble failed at %d with code %d\n", i, err);
            success = 0;
        }
        else
        {
            printf("%.3d: %s\n", i, buffer);
        }
    }

    free(buffer);
    return success;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        perror("Usage: ./testvm <executable> [var] ... [var=value] ...");
        exit(1);
    }

    int err = 0;
    void* handle = initVm(&err);
    if (err) {
        perror("Failed to initialize vm.");
        exit(err);
    }

    if (!loadExecutableFile(handle, argv[1], &err))
    {
        fprintf(stderr, "Fatal error: exiting with code %d\n", err);
        exit(err);
    }

    // determine whether each argument needs to be printed (1 == print later)
    char *printAddr = calloc(argc, 1);
    int processors = 1;
    int trace = 0;
    int printResults = 0;
    for (int i = 2; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (strcmp(argv[i], "-t") == 0)
            {
                trace = 1;
                printf("Option trace (-t)\n");
            }
            else if (argv[i][1] == 'p' && argv[i][2] != '\0')
            {
                sscanf(argv[i], "-p%d", &processors);
                printf("Option processors: %d (-p%d)\n", processors, processors);
                if (processors <= 0)
                {
                    fprintf(stderr, "Must have more than 0 processors\n");
                    exit(50);
                }
            }
            else
            {
                fprintf(stderr, "Invalid option %s\n", argv[i]);
                exit(50);
            }
            continue;
        }
        char* split = NULL;
        if ((split = strstr(argv[i], "=")) == NULL)
        {
            printAddr[i] = 1;
            printResults = 1;
            continue;
        }
        *split = '\0';
        split = split + 1;
        int32_t word;
        if (strstr(split, "f") || strstr(split, ".")) {
            float _f = atof(split);
            word = *(int32_t*)&_f;
        } else {
            word = atoi(split);
        }
        uint32_t addr;
        int result = getAddress(handle, argv[i], &addr);
        if (result == 0) {failAddr(argv[i]); continue;}
        result = putWord(handle, addr, word);
        if (result == 0) {failPut(addr); continue;}
        if (PRINT_RESULTS) printf("Put word 0x%x at %s (%d)\n", word, argv[i], addr);
    }

    uint32_t initialSP[VMX20_MAX_PROCESSORS] = {0};
    int STACK_SIZE = 1000;
    for (int i = 0; i < processors; i++)
    {
        initialSP[i] = 0xfffff - (STACK_SIZE * i);
    }
    int terminationStatus[VMX20_MAX_PROCESSORS] = {0};
    if (!execute(handle, processors, initialSP, terminationStatus, trace))
    {
        fprintf(stderr, "Fatal error, processors failed to start\n");
        exit(50);
    }
    /**
     *  #define VMX20_INITIALIZE_FAILURE -1
        #define VMX20_FILE_NOT_FOUND -2
        #define VMX20_FILE_IS_NOT_VALID -3

        #define VMX20_FILE_CONTAINS_OUTSYMBOLS -4

        #define VMX20_DIVIDE_BY_ZERO -5
        #define VMX20_ADDRESS_OUT_OF_RANGE -6
        #define VMX20_ILLEGAL_INSTRUCTION -7
     */
    for (int i = 0; i < processors; i++)
    {
        if (terminationStatus[i] != 0)
        {
            fprintf(stderr, "[%d] Error: code %d ", i, terminationStatus[i]);
            switch (terminationStatus[i])
            {
                case VMX20_DIVIDE_BY_ZERO:
                    fprintf(stderr, "(division by zero)\n");
                    break;
                case VMX20_ADDRESS_OUT_OF_RANGE:
                    fprintf(stderr, "(address out of range)\n");
                    break;
                case VMX20_ILLEGAL_INSTRUCTION:
                    fprintf(stderr, "(illegal instruction)\n");
                    break;
                default:
                    fprintf(stderr, "(unknown)\n");
            }
        } 
    }

    if (printResults) {
        printf("====================== RESULTS =======================\n");
        printf("[###]            Label:     Hex      Decimal    Float \n");
        printf("------------------------------------------------------\n");
        for (int i = 2; i < argc; i++)
        {
            if (!printAddr[i]) continue;
            uint32_t addr;
            int32_t wordx = 0;
            int result = getAddress(handle, argv[i], &addr);
            if (result == 0) {failAddr(argv[i]); continue;}
            result = getWord(handle, addr, &wordx);
            if (result == 0) {failGet(addr); continue;}
            printf("[%.3x] %16s: 0x%.8x %10d %8f\n", addr, argv[i], wordx, wordx, *(float*)&wordx);
        }
    }

    #if (TEST_DISASSEMBLE)
    int _r = testDisassemble(handle, argv[1]);
    if (_r)
    {
        printf("testDisassemble success!\n");
    }
    else
    {
        printf("testDisassemble falure!\n");
    }
    #endif

    free(printAddr);
    cleanup(handle);
    return 0;
}