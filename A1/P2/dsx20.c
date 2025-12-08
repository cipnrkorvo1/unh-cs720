#include "dsx20.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG 0

void* fbuf = NULL;
size_t fsize = -1;

int insym_count = 0;
int outsym_count = 0;
char** insymbol_names = NULL;
char** outsymbol_names = NULL;
WORD32* insymbol_addresses = NULL;
WORD32* outsymbol_addresses = NULL;

void read_file(char *filename)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp)
    {
        fprintf(stderr, "Could not open file %s\n", filename);
        exit(1);
    }
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fprintf(stderr, "Read failure in %s\n", filename);
        exit(1);
    }
    // acquire file size
    fsize = ftell(fp);
    if (fsize <= 0)
    {
        fprintf(stderr, "File %s is empty\n", filename);
        exit(1);
    }
    // allocate memory
    fbuf = malloc(fsize+1);
    if (!fbuf)
    {
        fprintf(stderr, "Failed to allocate memory.\n");
        exit(1);
    }
    rewind(fp);
    if (ferror(fp) != 0)
    {
        fprintf(stderr, "Read failure in %s\n", filename);
        exit(1);
    }
    // read file into memory
    fread(fbuf, 1, fsize, fp);
    if (ferror(fp) != 0)
    {
        fprintf(stderr, "Read failure in %s\n", filename);
        exit(1);
    }
    // file is read
    fclose(fp);
}

int main(int argc, char* argv[])
{
    // verify arguments
    if (argc != 2 && argc != 3)
    {
        printf("Usage: dsx20 [-r] <object file>\n");
        exit(1);
    }
    if (argc == 3)
    {
        if (!(*argv[1] == '-' && *(argv[1]+1) == 'r'))
        {
            printf("Usage: dsx20 [-r] <object file>\n");
            exit(1);
        }
    }

    char* filename = argv[argc-1];
    char is_recursive = argc == 3 ? 1 : 0;

    read_file(filename);
    if (DEBUG) printf("Disassembling %s\n", filename);


    // first 3 32-bit words should be:
    //  length of insymbol section
    //  length of outsymbol section
    //  length of object code
    WORD32* buf = fbuf;
    insym_count = *buf / 5;
    outsym_count = *(buf + 1) / 5;
    unsigned int obj_len = *(buf + 2);

    // allocate memory for names and addresses
    insymbol_names = calloc(insym_count, sizeof(char*));
    outsymbol_names = calloc(outsym_count, sizeof(char*));
    insymbol_addresses = calloc(insym_count, sizeof(WORD32));
    outsymbol_addresses = calloc(outsym_count, sizeof(WORD32));

    if (!insymbol_names || !outsymbol_names || !insymbol_addresses || !outsymbol_addresses)
    {
        fprintf(stderr, "Failed to allocate memory.\n");
        exit(1);
    }

    // read in/outsymbols
    read_insymbols(insym_count);
    read_outsymbols(3+(insym_count*5), outsym_count);

    // print insymbols
    printf("Insymbol Section (%d entries)\n\n", insym_count);
    for (int i = 0; i < insym_count; i++)
    {
        printf("%s %d\n", insymbol_names[i], insymbol_addresses[i]);
    }

    // print outsymbols
    printf("\nOutsymbol Section (%d entries)\n\n", outsym_count);
    for (int i = 0; i < outsym_count; i++)
    {
        printf("%s %d\n", outsymbol_names[i], outsymbol_addresses[i]);
    }

    int obj_start = 3+(insym_count*5)+(outsym_count*5);
    char* data_arr = NULL;
    if (is_recursive == 1)
    {
        // invoke recursive disassembly
        data_arr = parse_code(obj_start, obj_len);
    }


    // print object code
    printf("\nObject Code (%d words)\n\n", obj_len);
    print_code(obj_start, obj_len, data_arr);
    
    // free memory before closing program
    for (int i = 0; i < insym_count; i++)
    {
        if (insymbol_names[i]) free(insymbol_names[i]);
    }
    free(insymbol_names);
    free(insymbol_addresses);
    for (int i = 0; i < outsym_count; i++)
    {
        if (outsymbol_names[i]) free(outsymbol_names[i]);
    }
    free(outsymbol_names);
    free(outsymbol_addresses);

    if (data_arr) free(data_arr);

    free(fbuf);

    return 0;
}

void read_insymbols(int count)
{
    WORD32 start = 3;
    WORD32 *ptr = (WORD32*)fbuf + start;

    for (int i = 0; i < count; i++)
    {
        // first 4 words: name (little endian)
        // fifth word: address
        char* name = malloc(sizeof(char) * SYMBOL_NAME_LEN); 
        for (int word = 0; word < 4; word++)
        {
            WORD32 mask = 0x000000ff;
            WORD32 this_word = *ptr;
            for (int j = 0; j < 4; j++)
            {
                name[4*word + j] = this_word & mask;
                this_word >>= 8;
            }
            ptr += 1;
        }
        insymbol_names[i] = name;
        
        insymbol_addresses[i] = *ptr;
        ptr += 1;
    }

}

void read_outsymbols(WORD32 start, int count)
{
    WORD32* ptr = (WORD32*)fbuf + start;
    
    for (int i = 0; i < count; i++)
    {
        // first 4 words: name (little endian)
        // fifth word: address
        char* name = malloc(sizeof(char) * SYMBOL_NAME_LEN); 
        for (int word = 0; word < 4; word++)
        {
            WORD32 mask = 0x000000ff;
            WORD32 this_word = *ptr;
            for (int j = 0; j < 4; j++)
            {
                name[4*word + j] = this_word & mask;
                this_word >>= 8;
            }
            ptr += 1;
        }
        outsymbol_names[i] = name;
        
        outsymbol_addresses[i] = *ptr;
        ptr += 1;
    }

}

char* op_name(unsigned char op)
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

char op_format(unsigned char op)
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

char is_outsymbol(int32_t addr)
{
    for (int i = 0; i < outsym_count; i++)
    {
        if (outsymbol_addresses[i] == addr)
        {
            return 1;
        }
    }
    return 0;
}

char* parse_code(WORD32 start, int count)
{
    if (DEBUG) printf("Parsing code (len %d)\n", count);
    char* data = calloc(count+1, sizeof(char));
    const WORD32* const ptr = (WORD32*)fbuf + start;
    data[0] = D_INS;    // line 0 is always an instruction
    // begin loop
    for (int pc = 0; pc < count; pc++)
    {
        WORD32 cur = *(ptr + pc);
        unsigned char op = cur & 0xff;
        if (pc == 0 || data[pc] == D_INS)
        {
            // this line is an instruction
            int32_t target = -1;
            switch(op_format(op))
            {
                case F_ADDR:
                case F_REGADDR:
                    // assume addr == 0 is undefined for now
                    if (!(cur >> 12)) break;
                    target = EXTENDBIT20(cur >> 12)+pc+1;
                    break;
                case F_REGREGADDR:
                    // assume addr == 0 is undefined for now
                    if (!(cur >> 16)) break;
                    target = (int16_t)(cur >> 16)+pc+1;
                    break;
            }
            if (!(op == INS_JMP || op == INS_HALT || op == INS_RET))
            {
                // only jmp,halt,ret don't guarantee an instruction after
                data[pc+1] = D_INS;
            }
            if (op == INS_JMP || op == INS_BEQ || op == INS_BLT || 
                op == INS_BGT || op == INS_CALL)
            {
                // jmp/beq/etc x & x is defined -> x is an instruction
                if (target != -1) 
                {
                    // if jumping back to a new label,
                    // need to reparse from target
                    if (target < pc && data[target] == D_DATA)
                    {
                        pc = target - 1;
                    }
                    data[target] = D_INS;
                    
                }
                
            }
        }
                
    }

    return data;
}

void print_code(WORD32 start, int count, char* data_arr)
{
    WORD32* ptr = (WORD32*)fbuf + start;
    for (int pc = 0; pc < count; pc++)
    {
        WORD32 cur = *ptr;
        unsigned char op = cur & 0xff;
        if (data_arr && data_arr[pc] == D_DATA) op = INS_NOP;
        int32_t addr;
        char format = op_format(op);
        // print pc, word, instruction
        printf("%.7d   %.8x   %-8s    ", pc, cur, op_name(op));
        switch (format) {
            case F_OP:
            case F_INVALID:
                break;
            case F_ADDR:    // call, jmp
                addr = EXTENDBIT20(cur >> 12);
                !is_outsymbol(pc) ? printf("%d", addr+pc+1) : printf("[undefined]");
                break;
            case F_REG:
                printf("r%d", (cur >> 8) & 0xf);
                break;
            case F_REGCONST:
                printf("r%d, %d", (cur >> 8) & 0xf, (int32_t)EXTENDBIT20((cur >> 12) & 0xfffff));
                break;
            case F_REGADDR:
                addr = EXTENDBIT20(cur >> 12);
                printf("r%d, ", (cur >> 8) & 0xf);
                !is_outsymbol(pc) ? printf("%d", addr+pc+1) : printf("[undefined]");
                break;
            case F_REGREG:
                printf("r%d, r%d", (cur >> 8) & 0xf, (cur >> 12) & 0xf);
                break;
            case F_REGOFF:
                printf("r%d, %d(r%d)", (cur >> 8) & 0xf, (int16_t)(cur >> 16), (cur >> 12) & 0xf);
                break;
            case F_REGREGADDR:
                addr = (int16_t)(cur >> 16);
                printf("r%d, r%d, ", (cur >> 8) & 0xf, (cur >> 12) & 0xf);
                !is_outsymbol(pc) ? printf("%d", addr+pc+1) : printf("[undefined]");
                break;
        }
        printf("\n");
        
        ptr += 1;
    }
}