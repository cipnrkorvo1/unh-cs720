#include "vmx20.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG 0
#define EXTENDSIGN20(x) ((x) >> 19 ? (x) | 0xfff00000 : (x)) 
#define EXTENDSIGN16(x) ((x) >> 15 ? (x) | 0xffff0000 : (x))

#define FP 13
#define SP 14
#define PC 15
#define MEM_SIZE 0xfffff    // 1_048_575 bytes; 1MB
#define HALT 0x1f

typedef struct Symbol {
    char *name;
    void *next;
    int32_t address;
} sym_t;

struct VM {
    int32_t reg[16];    // registers
    int32_t *memory;    // main memory
                        // memory[0] is where program is loaded
    uint32_t entry_point;   // where execution should begin
    uint32_t prog_end;      // end of program (memory[n] < prog_end is program)

    sym_t *symbols;     // linked list of the insymbols
};



void* initVm(int32_t *errorNumber)
{   // allocate memory
    struct VM *vm = malloc(sizeof(struct VM));
    if (!vm) {*errorNumber = VMX20_INITIALIZE_FAILURE; return NULL;}
    vm->memory = malloc(sizeof(int32_t) * MEM_SIZE);
    if (!vm->memory) {*errorNumber = VMX20_INITIALIZE_FAILURE; free(vm); return NULL;}
    // initialize all registers to 0
    for (int i = 0; i < 16; i++)
    {
        vm->reg[i] = 0;
    }

    *errorNumber = VMX20_NORMAL_TERMINATION;
    return vm;
}

int32_t loadExecutableFile(void *handle, char *filename, int32_t *errorNumber)
{   // verify .exe file
    char* filetype = strstr(filename, ".exe");
    if (!filetype || *(filetype + 4) != '\0') return (*errorNumber = VMX20_FILE_IS_NOT_VALID) & 0;
    // open file
    FILE *fp = fopen(filename, "rb");
    if (!fp) return (*errorNumber = VMX20_FILE_NOT_FOUND) & 0;
    // convert handle
    struct VM *vm = handle;
    if (!handle) return (*errorNumber = -99) & fclose(fp) & 0;

    // get section lengths
    int32_t lengths[3] = {0, 0, 0};
    fread(&lengths[0], sizeof(int32_t), 3, fp);
    if (DEBUG) printf("lengths %d %d %d\n", lengths[0], lengths[1], lengths[2]);
    // if outsymbol section: fatal error
    if (lengths[1] != 0) return (*errorNumber = VMX20_FILE_CONTAINS_OUTSYMBOLS) & fclose(fp) & 0;
    // read insymbols into vm symbol table
    int32_t *symbuffer = malloc(sizeof(int32_t) * lengths[0]);
    fseek(fp, 12, SEEK_SET);
    fread(symbuffer, sizeof(int32_t), lengths[0], fp);

    // load program into memory (starts at 0)
    fseek(fp, 12 + ((lengths[0] + lengths[1]) * sizeof(int32_t)), SEEK_SET);
    size_t wordsRead = fread(vm->memory, sizeof(int32_t), lengths[2], fp);
    // set prog_end to end of instructions
    vm->prog_end = wordsRead;
    // verify bytesRead == fileLength
    if (wordsRead != lengths[2]) {free(symbuffer); return (*errorNumber = -98) & fclose(fp) & 0;}
    fclose(fp);

    // parse insymbols
    int32_t *ptr = symbuffer;
    for (int t = 0; t < lengths[0]; t += 5)
    {
        sym_t *symbol = malloc(sizeof(sym_t));
        symbol->name = calloc(16, sizeof(char));
        strcpy(symbol->name, (char*)ptr);
        symbol->address = *(ptr + 4);
        symbol->next = vm->symbols;
        vm->symbols = symbol;

        if (strcmp(symbol->name, "mainx20") == 0)
        {
            // set pc to address
            vm->entry_point = symbol->address;
        }

        if (DEBUG) printf("Insymbol %s at %x\n", symbol->name, symbol->address);
        ptr += 5;
    }

    free(symbuffer);
    return 1;
}
    
int32_t getAddress(void *handle, char *label, uint32_t *outAddr)
{
    struct VM *vm = handle;
    sym_t *cur = vm->symbols;
    while (cur)
    {
        if (strcmp(cur->name, label) == 0)
        {
            // found!
            *outAddr = cur->address;
            return 1;
        }
        cur = cur->next;
    }
    // not found ):
    return 0;
}

int32_t getWord(void *handle, uint32_t addr, int32_t *outWord)
{
    struct VM *vm = handle;
    if (addr >= MEM_SIZE) return 0;
    *outWord = vm->memory[addr];
    return 1;
}

int32_t putWord(void *handle, uint32_t addr, int32_t word)
{
    struct VM *vm = handle;
    if (addr >= MEM_SIZE) return 0;
    vm->memory[addr] = word;
    return 1;
}

// Part A: jmp, load*, store, halt, add*, sub*
static int32_t executeInstruction(void *handle, uint32_t word, int32_t *term_code, char *term_info[])
{
    struct VM *vm = handle;
    int32_t success = 1;
    uint32_t addr = -1;
    int reg1 = word >> 8 & 0xf;
    int reg2 = word >> 12 & 0xf;
    float r1f, r2f;
    if (DEBUG) printf("%.8x\n", word);
    uint32_t tPC = vm->reg[PC] + 1;     // target PC; for use when executing operations
    switch (word & 0xff)
    {
        case 0x00:  // halt
            *term_code = VMX20_NORMAL_TERMINATION;
            **term_info = '\0';
            success = 0;
            break;
        case 0x01:  // load
            addr = EXTENDSIGN20(word >> 12);
            getWord(vm, tPC + addr, &vm->reg[reg1]); 
            break;
        case 0x02:  // store
            addr = EXTENDSIGN20(word >> 12);
            putWord(vm, tPC + addr, vm->reg[reg1]);
            break;
        case 0x03:  // ldimm
            addr = EXTENDSIGN20(word >> 12);
            vm->reg[reg1] = addr;
            break;
        case 0x04:  // ldaddr
            addr = EXTENDSIGN20(word >> 12);
            vm->reg[reg1] = tPC + addr;
            break;
        case 0x05:  // ldind
            addr = EXTENDSIGN16(word >> 16);
            if ((uint64_t)vm->reg[reg2] + addr >= MEM_SIZE) {
                *term_code = VMX20_ADDRESS_OUT_OF_RANGE;
                sprintf(*term_info, "%8x (%8x + %8x)", vm->reg[reg2] + addr, vm->reg[reg2], addr);
                success = 0;
                break;
            }
            getWord(vm, vm->reg[reg2] + addr, &vm->reg[reg1]);
            break;
        case 0x06:  // stind
            addr = EXTENDSIGN16(word >> 16);
            if ((uint64_t)reg2 + addr >= MEM_SIZE) {
                *term_code = VMX20_ADDRESS_OUT_OF_RANGE;
                sprintf(*term_info, "%8x (%8x + %8x)", vm->reg[reg2] + addr, vm->reg[reg2], addr);
                success = 0;
                break;
            }
            putWord(vm, vm->reg[reg2] + addr, vm->reg[reg1]);
            break;
        case 0x07:  // addf
            memcpy(&r1f, &vm->reg[reg1], sizeof(int32_t));
            memcpy(&r2f, &vm->reg[reg2], sizeof(int32_t));
            r1f = r1f + r2f;
            memcpy(&vm->reg[reg1], &r1f, sizeof(int32_t));
            break;
        case 0x08:  // subf
            memcpy(&r1f, &vm->reg[reg1], sizeof(int32_t));
            memcpy(&r2f, &vm->reg[reg2], sizeof(int32_t));
            r1f = r1f - r2f;
            memcpy(&vm->reg[reg1], &r1f, sizeof(int32_t));
            break;
        case 0x0b:  // addi
            vm->reg[reg1] = vm->reg[reg1] + vm->reg[reg2];
            break;
        case 0x0c:  // subi
            vm->reg[reg1] = vm->reg[reg1] - vm->reg[reg2];
            break;
        case 0x14:  // jmp
            addr = EXTENDSIGN20(word >> 12);
            tPC += addr;
            break;
        default:
            *term_code = VMX20_ILLEGAL_INSTRUCTION;
            sprintf(*term_info, "%.2x", word & 0xff);
            success = 0;
            break;
    }
    vm->reg[PC] = tPC;
    return success;

}

//      ( single function with mainx20 as the entry point )
//      trace argument ignored
int32_t execute(void *handle, uint32_t numProcessors, uint32_t initialSP[],
      int terminationStatus[], int32_t trace)
{
    struct VM *vm = handle;
    // currently ignoring all arguments
    int32_t term_code = 1;
    char *term_info = malloc(256);
    // set pc to entry_point
    vm->reg[PC] = vm->entry_point;
    if (DEBUG) printf("entry: %d\n", vm->entry_point);
    while (executeInstruction(handle, vm->memory[vm->reg[PC]], &term_code, (char **)&term_info))
    {
        // check below only works for no function calls
        if (vm->reg[PC] >= vm->prog_end)
        {
            term_code = VMX20_ADDRESS_OUT_OF_RANGE;
            sprintf(term_info, "%d", vm->reg[PC]);
            break;
        };
        if (DEBUG) printf("%d\n", vm->reg[PC]);

    }

    switch (term_code)
    {
        case VMX20_NORMAL_TERMINATION:
            fprintf(stderr, "Program halted from normal execution.\n");
            break;
        case VMX20_ADDRESS_OUT_OF_RANGE:
            fprintf(stderr, "Error: Address %s out of range.\n", term_info);
            break;
        case VMX20_ILLEGAL_INSTRUCTION:
            fprintf(stderr, "Error: Illegal instruction 0x%s.\n", term_info);
            break;
        default:
            fprintf(stderr, "Unknown termination code %d.\n", term_code);
    }
    free(term_info);
    return term_code == 0;
}

int disassemble(void *handle, uint32_t address, char *buffer, int32_t *errorNumber)
{
    return 0;
}

void cleanup(void *handle)
{
    struct VM *vm = handle;
    if (!vm) return;
    if (vm->memory) free(vm->memory);
    while (vm->symbols)
    {
        sym_t *cur = vm->symbols;
        vm->symbols = cur->next;
        free(cur->name);
        free(cur);
    }
    free(vm);
}