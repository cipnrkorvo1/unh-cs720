#include "vmx20.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void failAddr(char *arg) { printf("failed to retrieve address for %s\n", arg); }
void failGet(int addr) { printf("failed to get word at addr %d\n", addr); }
void failPut(int addr) { printf("failed to put word at addr %d\n", addr); }

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
    char *print_addr = calloc(argc, 1);
    for (int i = 2; i < argc; i++)
    {
        char* split = NULL;
        if ((split = strstr(argv[i], "=")) == NULL)
        {
            print_addr[i] = 1;
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
        printf("Put word 0x%x at %s (%d)\n", word, argv[i], addr);
    }

    int processors = 1;
    uint32_t initialSP[] = {0};
    int terminationStatus[] = {0};
    int trace = 0;
    if (!execute(handle, processors, initialSP, terminationStatus, trace))
    {
        fprintf(stderr, "Fatal error: exiting with code %d\n", err);
        exit(err);
    }

    printf("====================== RESULTS =======================\n");
    printf("[###]            Label:     Hex      Decimal    Float \n");
    printf("------------------------------------------------------\n");
    for (int i = 2; i < argc; i++)
    {
        if (!print_addr[i]) continue;
        uint32_t addr;
        int32_t wordx = 0;
        int result = getAddress(handle, argv[i], &addr);
        if (result == 0) {failAddr(argv[i]); continue;}
        result = getWord(handle, addr, &wordx);
        if (result == 0) {failGet(addr); continue;}
        printf("[%.3x] %16s: 0x%.8x %10d %8f\n", addr, argv[i], wordx, wordx, *(float*)&wordx);
    }

    free(print_addr);
    cleanup(handle);
    return 0;
}