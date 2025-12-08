#include "vmx20.h"
#include "vmx20_macros.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG 0

#define FP 13
#define SP 14
#define PC 15
#define MEM_SIZE 0xfffff / sizeof(int32_t)    // 1_048_575 bytes; 1MB
#define HALT 0x1f

typedef struct Symbol {
    char *name;
    void *next;
    int32_t address;
} sym_t;

// VM shouldn't have registers at all; should all be in Core
struct VM {
    int32_t reg[16];    // registers
    int32_t *memory;    // main memory
                        // memory[0] is where program is loaded
    uint32_t entryPoint;   // where execution should begin
    uint32_t progEnd;      // end of program (memory[n] < prog_end is program)
    int numProcessors;     // number of processors (set on execute())
    int trace;             // whether to trace execution or not
    
    pthread_mutex_t *traceLock;
    pthread_mutex_t *memLock;

    sym_t *symbols;     // linked list of the insymbols
};

typedef struct Core {
    int32_t reg[16];    // registers
    uint32_t stack;     // memory addr of bottom of stack
    //uint32_t stackSize;   // size of stack
    struct VM *vm;      // vm handle
    int status;         // terminationStatus
    int pid;             // getpid instr return value
} core_t;

static char* op_name(unsigned char op)
{
    switch(op)
    {
        case INS_HALT:
            return "halt";
        case INS_LOAD:
            return "load";
        case INS_STORE:
            return "store";
        case INS_LDIMM:
            return "ldimm";
        case INS_LDADDR:
            return "ldaddr";
        case INS_LDIND:
            return "ldind";
        case INS_STIND:
            return "stind";
        case INS_ADDF:
            return "addf";
        case INS_SUBF:
            return "subf";
        case INS_DIVF:
            return "divf";
        case INS_MULF:
            return "mulf";
        case INS_ADDI:
            return "addi";
        case INS_SUBI:
            return "subi";
        case INS_DIVI:
            return "divi";
        case INS_MULI:
            return "muli";
        case INS_CALL:
            return "call";
        case INS_RET:
            return "ret";
        case INS_BLT:
            return "blt";
        case INS_BGT:
            return "bgt";
        case INS_BEQ:
            return "beq";
        case INS_JMP:
            return "jmp";
        case INS_CMPXCHG:
            return "cmpxchg";
        case INS_GETPID:
            return "getpid";
        case INS_GETPN:
            return "getpn";
        case INS_PUSH:
            return "push";
        case INS_POP:
            return "pop";
        case INS_NOP:
            return "NOP";
        case INS_INVALID:
        default:
            return "unknown";
    }
}
static char op_format(unsigned char op)
{
    switch(op)
    {
        case INS_HALT:
            return F_OP;
        case INS_LOAD:
        case INS_STORE:
            return F_REGADDR;
        case INS_LDIMM:
            return F_REGCONST;
        case INS_LDADDR:
            return F_REGADDR;
        case INS_LDIND:
        case INS_STIND:
            return F_REGOFF;
        case INS_ADDF:
        case INS_SUBF:
        case INS_DIVF:
        case INS_MULF:
        case INS_ADDI:
        case INS_SUBI:
        case INS_DIVI:
        case INS_MULI:
            return F_REGREG;
        case INS_CALL:
            return F_ADDR;
        case INS_RET:
            return F_OP;
        case INS_BLT:
        case INS_BGT:
        case INS_BEQ:
            return F_REGREGADDR;
        case INS_JMP:
            return F_ADDR;
        case INS_CMPXCHG:
            return F_REGREGADDR;
        case INS_GETPID:
        case INS_GETPN:
        case INS_PUSH:
        case INS_POP:
            return F_REG;
        case INS_NOP:
            return F_OP;
        case INS_INVALID:
        default:
            return F_INVALID;
    }
}

static void printTrace(void *core)
{
    core_t *cpu = core;
    pthread_mutex_lock(cpu->vm->traceLock);
    printf("Core %d\n", cpu->pid);
    for (int i = 0; i < 16; i++)
    {
        printf("%.8x ", cpu->reg[i]);
        if (i == 7) printf("\n");
    }
    //pthread_mutex_unlock(cpu->vm->traceLock);
    char buffer[100];
    int err = 0;
    disassemble(cpu->vm, cpu->reg[PC], buffer, &err);
    //pthread_mutex_lock(cpu->vm->traceLock);
    printf("\n%s\n\n", buffer);
    pthread_mutex_unlock(cpu->vm->traceLock);
}

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
    // init mutexes
    vm->traceLock = malloc(sizeof(pthread_mutex_t));
    vm->memLock = malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(vm->traceLock, NULL))
    {
        fprintf(stderr, "Failed to init mutex.\n");
        exit(50);
    }
    if (pthread_mutex_init(vm->memLock, NULL))
    {
        fprintf(stderr, "Failed to init mutex.\n");
        exit(50);
    }

    *errorNumber = VMX20_NORMAL_TERMINATION;
    return vm;
}

int32_t loadExecutableFile(void *handle, char *filename, int32_t *errorNumber)
{   // verify .exe file
    /*char* filetype = strstr(filename, ".exe");
    if (!filetype || *(filetype + 4) != '\0') return (*errorNumber = VMX20_FILE_IS_NOT_VALID) & 0;*/
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
    vm->progEnd = wordsRead;
    // verify bytesRead == fileLength
    if (wordsRead != lengths[2]) {free(symbuffer); return (*errorNumber = VMX20_FILE_IS_NOT_VALID) & fclose(fp) & 0;}
    fclose(fp);

    // parse insymbols
    int32_t *ptr = symbuffer;
    char mainFound = 0;
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
            if (mainFound) {free(symbuffer); *errorNumber = VMX20_FILE_IS_NOT_VALID; return 0;}
            // set pc to address
            vm->entryPoint = symbol->address;
            mainFound = 1;
        }

        if (DEBUG) printf("Insymbol %s at %x\n", symbol->name, symbol->address);
        ptr += 5;
    }

    free(symbuffer);

    if (!mainFound)
    {
        *errorNumber = VMX20_FILE_IS_NOT_VALID;
        return 0;
    }

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

static int32_t executeInstruction(void *core, uint32_t instrAddr, int32_t *termCode)
{
    core_t *cpu = core;
    int32_t success = 1;
    int32_t addr = 0;
    
    //pthread_mutex_lock(cpu->vm->memLock);

    int32_t word = cpu->vm->memory[instrAddr];
    int reg1 = word >> 8 & 0xf;
    int reg2 = word >> 12 & 0xf;
    float r1f, r2f;
    //if (DEBUG) printf("%.8x\n", word);
    uint32_t tPC = cpu->reg[PC] + 1;     // target PC; for use when executing operations
    switch (word & 0xff)
    {
        case INS_HALT:  // halt
            *termCode = VMX20_NORMAL_TERMINATION;
            //**termInfo = '\0';
            success = 0;
            break;
        case INS_LOAD:  // load
            addr = EXTENDSIGN20(word >> 12);
            pthread_mutex_lock(cpu->vm->memLock);
            getWord(cpu->vm, tPC + addr, &cpu->reg[reg1]); 
            pthread_mutex_unlock(cpu->vm->memLock);
            break;
        case INS_STORE:  // store
            addr = EXTENDSIGN20(word >> 12);
            pthread_mutex_lock(cpu->vm->memLock);
            putWord(cpu->vm, tPC + addr, cpu->reg[reg1]);
            pthread_mutex_unlock(cpu->vm->memLock);
            break;
        case INS_LDIMM:  // ldimm
            addr = EXTENDSIGN20(word >> 12);
            cpu->reg[reg1] = addr;
            break;
        case INS_LDADDR:  // ldaddr
            addr = EXTENDSIGN20(word >> 12);
            cpu->reg[reg1] = tPC + addr;
            break;
        case INS_LDIND:  // ldind
            addr = EXTENDSIGN16(word >> 16);
            if ((uint64_t)cpu->reg[reg2] + addr >= MEM_SIZE) {
                *termCode = VMX20_ADDRESS_OUT_OF_RANGE;
                //sprintf(*termInfo, "%8x (%8x + %8x)", cpu->reg[reg2] + addr, cpu->reg[reg2], addr);
                success = 0;
                break;
            }
            pthread_mutex_lock(cpu->vm->memLock);
            getWord(cpu->vm, cpu->reg[reg2] + addr, &cpu->reg[reg1]);
            pthread_mutex_unlock(cpu->vm->memLock);
            break;
        case INS_STIND:  // stind
            addr = EXTENDSIGN16(word >> 16);
            if ((uint64_t)reg2 + addr >= MEM_SIZE) {
                *termCode = VMX20_ADDRESS_OUT_OF_RANGE;
                //sprintf(*termInfo, "%8x (%8x + %8x)", cpu->reg[reg2] + addr, cpu->reg[reg2], addr);
                success = 0;
                break;
            }
            pthread_mutex_lock(cpu->vm->memLock);
            putWord(cpu->vm, cpu->reg[reg2] + addr, cpu->reg[reg1]);
            pthread_mutex_unlock(cpu->vm->memLock);
            break;
        case INS_ADDF:  // addf
            memcpy(&r1f, &cpu->reg[reg1], sizeof(int32_t));
            memcpy(&r2f, &cpu->reg[reg2], sizeof(int32_t));
            r1f = r1f + r2f;
            memcpy(&cpu->reg[reg1], &r1f, sizeof(int32_t));
            break;
        case INS_SUBF:  // subf
            memcpy(&r1f, &cpu->reg[reg1], sizeof(int32_t));
            memcpy(&r2f, &cpu->reg[reg2], sizeof(int32_t));
            r1f = r1f - r2f;
            memcpy(&cpu->reg[reg1], &r1f, sizeof(int32_t));
            break;
        case INS_DIVF:  // divf
            memcpy(&r1f, &cpu->reg[reg1], sizeof(int32_t));
            memcpy(&r2f, &cpu->reg[reg2], sizeof(int32_t));
            if (r2f == 0.0f) {
                *termCode = VMX20_DIVIDE_BY_ZERO;
                //**termInfo = '\0';
                success = 0;
                break;
            }
            r1f = r1f / r2f;
            memcpy(&cpu->reg[reg1], &r1f, sizeof(int32_t));
            break;
        case INS_MULF:  // mulf
            memcpy(&r1f, &cpu->reg[reg1], sizeof(int32_t));
            memcpy(&r2f, &cpu->reg[reg2], sizeof(int32_t));
            r1f = r1f * r2f;
            memcpy(&cpu->reg[reg1], &r1f, sizeof(int32_t));
            break;
        case INS_ADDI:  // addi
            cpu->reg[reg1] = cpu->reg[reg1] + cpu->reg[reg2];
            break;
        case INS_SUBI:  // subi
            cpu->reg[reg1] = cpu->reg[reg1] - cpu->reg[reg2];
            break;
        case INS_DIVI:  // divi
            if (cpu->reg[reg2] == 0) {
                *termCode = VMX20_DIVIDE_BY_ZERO;
                //**termInfo = '\0';
                success = 0;
                break;
            }
            cpu->reg[reg1] = cpu->reg[reg1] / cpu->reg[reg2];
            break;
        case INS_MULI:  // muli
            cpu->reg[reg1] = cpu->reg[reg1] * cpu->reg[reg2];
            break;
        case INS_CALL:  // call
            // check if sp is/will be be out of bounds
            if (cpu->reg[SP] > cpu->stack || cpu->reg[SP] - 3 < cpu->vm->progEnd)
            {
                // out of bounds
                if (DEBUG) printf("<before> sp %d out of range\n", cpu->reg[SP]);
                *termCode = VMX20_ADDRESS_OUT_OF_RANGE;
                success = 0;
                break;
            }
            addr = EXTENDSIGN20(word >> 12);
            // acquire memory lock first
            pthread_mutex_lock(cpu->vm->memLock);
            // push pc on the stack
            cpu->reg[SP] -= 1;
            putWord(cpu->vm, cpu->reg[SP], tPC);
            // push contents of fp register onto stack
            tPC += addr;
            cpu->reg[SP] -= 1;
            putWord(cpu->vm, cpu->reg[SP], cpu->reg[FP]);
            // assign contents of fp register to sp register
            cpu->reg[FP] = cpu->reg[SP];
            // push a zero on top of the stack
            cpu->reg[SP] -= 1;
            putWord(cpu->vm, cpu->reg[SP], 0);
            // release memory lock
            pthread_mutex_unlock(cpu->vm->memLock);
            break;
        case INS_RET:  // ret
            // check if sp is/will be be out of bounds
            if (cpu->reg[SP] + 3 > cpu->stack|| cpu->reg[SP] < cpu->vm->progEnd)
            {
                // out of bounds
                *termCode = VMX20_ADDRESS_OUT_OF_RANGE;
                success = 0;
                break;
            }
            // acquire memory lock
            pthread_mutex_lock(cpu->vm->memLock);
            // retrieve the return value
            getWord(cpu->vm, cpu->reg[SP], &word);              // returnValue
            cpu->reg[SP] += 1;
            // set FP
            getWord(cpu->vm, cpu->reg[SP], &cpu->reg[FP]);      // savedFP
            cpu->reg[SP] += 1;
            // set PC
            getWord(cpu->vm, cpu->reg[SP], (int32_t *)&tPC);    // returnAddress
            cpu->reg[SP] += 1;
            // store return value
            putWord(cpu->vm, cpu->reg[FP] - 1, word);
            // release memory lock
            pthread_mutex_unlock(cpu->vm->memLock);
            break;
        case INS_BLT:  // blt
            addr = EXTENDSIGN16(word >> 16);
            if (cpu->reg[reg1] < cpu->reg[reg2])
            {
                tPC += addr;
            }
            break;
        case INS_BGT:  // bgt
            addr = EXTENDSIGN16(word >> 16);
            if (cpu->reg[reg1] > cpu->reg[reg2])
            {
                tPC += addr;
            }
            break;
        case INS_BEQ:  // beq
            addr = EXTENDSIGN16(word >> 16);
            if (cpu->reg[reg1] == cpu->reg[reg2])
            {
                tPC += addr;
            }
            break;
        case INS_JMP:  // jmp
            addr = EXTENDSIGN20(word >> 12);
            tPC += addr;
            break;
        case INS_CMPXCHG:  // cmpxchg
            addr = EXTENDSIGN16(word >> 16);
            // lock memory
            pthread_mutex_lock(cpu->vm->memLock);
            int32_t _w = 0;
            getWord(cpu->vm, tPC + addr, &_w);
            if (cpu->reg[reg1] == _w)
            {
                putWord(cpu->vm, tPC + addr, cpu->reg[reg2]);
            }
            else
            {
                getWord(cpu->vm, tPC + addr, &cpu->reg[reg1]);
            }
            // unlock memory
            pthread_mutex_unlock(cpu->vm->memLock);
            break;
        case INS_GETPID:  // getpid
            cpu->reg[reg1] = cpu->pid;
            break;
        case INS_GETPN:  // getpn
            cpu->reg[reg1] = cpu->vm->numProcessors;
            break;
        case INS_PUSH:  // push
            // check if sp out of bounds
            if (cpu->reg[SP] > cpu->stack || cpu->reg[SP] < cpu->vm->progEnd)
            {
                // out of bounds
                if (DEBUG) printf("<push> sp %d out of range\n", cpu->reg[SP]);
                *termCode = VMX20_ADDRESS_OUT_OF_RANGE;
                success = 0;
                break;
            }
            cpu->reg[SP] -= 1;
            pthread_mutex_lock(cpu->vm->memLock);
            putWord(cpu->vm, cpu->reg[SP], cpu->reg[reg1]);
            pthread_mutex_unlock(cpu->vm->memLock);
            break;
        case INS_POP:  // pop
            // check if sp out of bounds
            if (cpu->reg[SP] > cpu->stack || cpu->reg[SP] < cpu->vm->progEnd)
            {
                // out of bounds
                if (DEBUG) printf("<pop> sp %d out of range\n", cpu->reg[SP]);
                *termCode = VMX20_ADDRESS_OUT_OF_RANGE;
                success = 0;
                break;
            }
            pthread_mutex_lock(cpu->vm->memLock);
            getWord(cpu->vm, cpu->reg[SP], &cpu->reg[reg1]);
            pthread_mutex_unlock(cpu->vm->memLock);
            cpu->reg[SP] += 1;
        default:
            *termCode = VMX20_ILLEGAL_INSTRUCTION;
            //sprintf(*termInfo, "%.2x", word & 0xff);
            success = 0;
            break;
    }

    //pthread_mutex_unlock(cpu->vm->memLock);

    if (cpu->vm->trace) printTrace(cpu);
    cpu->reg[PC] = tPC;
    return success;

}

static void *fetchDecodeExecute(void *core) {
    // init cycle
    core_t *cpu = core;
    struct VM *vm = cpu->vm;
    cpu->status = 1;
    char *termInfo = malloc(256);
    // set pc to entry_point
    cpu->reg[PC] = vm->entryPoint;
    if (DEBUG) printf("entry: %d\n", vm->entryPoint);

    while (executeInstruction(cpu, cpu->reg[PC], &cpu->status) == 1)
    {
        ;
    }

    free(termInfo);
    return NULL;
}


int32_t execute(void *handle, uint32_t numProcessors, uint32_t initialSP[],
      int terminationStatus[], int32_t trace)
{
    // init VM
    struct VM *vm = handle;
    if (numProcessors > VMX20_MAX_PROCESSORS)
    {
        return 0;
    }
    vm->numProcessors = numProcessors;
    vm->trace = trace;

    // init core(s)
    core_t **cores = malloc(sizeof(core_t*) * numProcessors);
    pthread_t *threads = malloc(sizeof(pthread_t) * numProcessors);
    //uint32_t coreStackSize = 1000; // TODO: calculate
    for (int i = 0; i < numProcessors; i++)
    {
        core_t *core = malloc(sizeof(core_t));
        memcpy(core->reg, vm->reg, 16 * sizeof(int32_t));
        //core->stackSize = coreStackSize;    
        //core->stack = vm->progEnd + (core->stackSize * (i+1));  // TODO: initialSP??
        core->stack = initialSP[i];
        core->reg[SP] = core->stack;
        core->vm = vm;
        core->status = 1;
        core->pid = i;
        cores[i] = core;

        // create thread
        if (pthread_create(&threads[i], NULL, &fetchDecodeExecute, core))
        {
            // failure starting thread, fatal error
            for (int j = 0; j < i; j++) if (cores[j]) free(cores[j]);
            free(cores);
            return 0;
        }
    }

    // start thread work
    for (int i = 0; i < numProcessors; i++)
    {
        pthread_join(threads[i], NULL);
    }
    // all threads done

    // cleanup cores
    for (int i = 0; i < numProcessors; i++)
    {
        if (cores[i] == NULL) continue;
        terminationStatus[i] = cores[i]->status;
        free(cores[i]);
    }
    free(cores);
    free(threads);

    return 1;
}

int disassemble(void *handle, uint32_t address, char *buffer, int32_t *errorNumber)
{
    *errorNumber = 0;
    struct VM *vm = handle;
    if (address >= vm->progEnd)
    {
        *errorNumber = VMX20_ADDRESS_OUT_OF_RANGE;
        return 0;
    }
    int32_t word;
    if (!getWord(vm, address, &word))
    {
        *errorNumber = VMX20_ADDRESS_OUT_OF_RANGE;
        return 0;
    }
    unsigned char op = word & 0xff;
    int32_t addr;
    char format = op_format(op);
    // print pc, word, instruction
    sprintf(buffer, "%-8s ", op_name(op));
    char *b = malloc(32);
    switch (format) {
        case F_OP:
            break;
        case F_INVALID:
            *errorNumber = VMX20_ILLEGAL_INSTRUCTION;
            break;
        case F_ADDR:    // call, jmp
            addr = EXTENDSIGN20(word >> 12);
            sprintf(b, "%d", addr + address + 1);
            strcat(buffer, b);
            break;
        case F_REG:
            sprintf(b, "r%d", (word >> 8) & 0xf);
            strcat(buffer, b);
            break;
        case F_REGCONST:
            sprintf(b, "r%d, %d", (word >> 8) & 0xf, (int32_t)EXTENDSIGN20((word >> 12) & 0xfffff));
            strcat(buffer, b);
            break;
        case F_REGADDR:
            addr = EXTENDSIGN20(word >> 12);
            sprintf(b, "r%d, ", (word >> 8) & 0xf);
            strcat(buffer, b);
            sprintf(b, "%d", addr+address+1);
            strcat(buffer, b);
            break;
        case F_REGREG:
            sprintf(b, "r%d, r%d", (word >> 8) & 0xf, (word >> 12) & 0xf);
            strcat(buffer, b);
            break;
        case F_REGOFF:
            sprintf(b, "r%d, %d(r%d)", (word >> 8) & 0xf, (int16_t)(word >> 16), (word >> 12) & 0xf);
            strcat(buffer, b);
            break;
        case F_REGREGADDR:
            addr = (int16_t)(word >> 16);
            sprintf(b, "r%d, r%d, ", (word >> 8) & 0xf, (word >> 12) & 0xf);
            strcat(buffer, b);
            sprintf(b, "%d", addr+address+1);
            strcat(buffer, b);
            break;
    }
    free(b);
    return *errorNumber == 0;
 
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
    free(vm->memLock);
    free(vm->traceLock);
    free(vm);
}