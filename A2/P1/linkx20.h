#include <stdint.h>

typedef struct symbol {
    unsigned long address;
    char* name;
    void* next;
    char flags;
} sym_t;

typedef struct fheader {
    uint32_t* data;     // data in file
    long size;          // in bytes
    char* filename;     // name of file
    void* next;         // ptr to next fheader
    sym_t* insyms;      // list of insymbols
    sym_t* outsyms;     // list of outsymbols
} fheader_t;


#define MAX_SYMBOL_LENGTH 17

#define FLAG_OUTSYM     0b00000001
#define FLAG_DEFINED    0b00000010

int extract_symbols(fheader_t* fh, sym_t** insyms, sym_t** outsyms);

long generate_output_file(fheader_t* fh, char* filename, char is_object);