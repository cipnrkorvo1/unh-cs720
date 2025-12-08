#include "linkx20.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG 0

void free_all(fheader_t*);

int main(int argc, char* argv[])
{

    if (argc == 1)
    {
        perror("Usage: ./linkx20 < >.obj ... -o <name>\n");
        exit(1);
    }

    fheader_t* file_headers = NULL;
    char obj_file = 0;
    char* exec_name = malloc(strlen("mainx20.exe") + 1);
    strcpy(exec_name, "mainx20.exe");
    for (int i = 1; i < argc; i++)
    {
        if (i == argc - 2 && strcmp(argv[i], "-o") == 0)
        {
            // final argument is the executable name
            free(exec_name);
            exec_name = malloc(5 + strlen(argv[argc-1]));
            strcpy(exec_name, argv[argc-1]);
            strcat(exec_name, ".exe");
            break;
        }
        fheader_t* header = malloc(sizeof(fheader_t));
        header->filename = argv[i];
        FILE* fp = fopen(argv[i], "rb");
        if (!fp)
        {
            fprintf(stderr, "Invalid file name %s\n", argv[i]);
            exit(1);
        }
        fseek(fp, 0, SEEK_END);
        header->size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        header->data = malloc(header->size);
        fread(header->data, 1, header->size, fp);
        if (ferror(fp) != 0 || header->data == NULL)
        {
            fprintf(stderr, "Error reading file %s\n", argv[i]);
            exit(1);
        }
        fclose(fp);
        header->insyms = NULL;
        header->outsyms = NULL;
        header->next = file_headers;
        file_headers = header;
    }

    fheader_t* cur = file_headers;
    int found = 0;
    while (cur)
    {
        found |= extract_symbols(cur, &cur->insyms, &cur->outsyms);
        cur = cur->next;
    }
    if (!found)
    {
        perror("Error: No entry point \"mainx20\"\n");
        obj_file = 1;
    }

    #if DEBUG
    printf("All Insymbols:\n");
    fheader_t* fh = file_headers;
    while (fh)
    {
        sym_t* ptr = fh->insyms;
        while (ptr)
        {
            printf("%-16s @ %-5lu in %s\n", ptr->name, ptr->address, fh->filename);
            ptr = ptr->next;
        }
        fh = fh->next;
    }
    printf("All Outsymbols:\n");
    fh = file_headers;
    while (fh)
    {
        sym_t* ptr = fh->outsyms;
        while (ptr)
        {
            printf("%-16s @ %-5lu in %s\n", ptr->name, ptr->address, fh->filename);
            ptr = ptr->next;
        }
        fh = fh->next;
    }
    printf("\n");
    #endif

    // generate the output file
    generate_output_file(file_headers, exec_name, obj_file);

    free(exec_name);
    free_all(file_headers);
    return 0;
}

int extract_symbols(fheader_t* fh, sym_t** insyms, sym_t** outsyms)
{
    int in_count =  fh->data[0];
    int out_count = fh->data[1];
    int found = 0;    // for keeping track of whether 'mainx20' is found in insymbols

    if (DEBUG) printf("%s %d %d\n", fh->filename, in_count, out_count);

    uint32_t* ptr = fh->data + 3;
    for (int i = 0; i < in_count + out_count; i += 5)
    {
        sym_t* symbol = malloc(sizeof(sym_t));
        symbol->flags = 0;
        symbol->name = calloc(MAX_SYMBOL_LENGTH, sizeof(char));
        strncpy(symbol->name, (char*)ptr, 16);
        ptr += 4;
        symbol->address = *ptr;
        ptr += 1;

        if (i < in_count)
        {
            // is an insymbol
            if (found == 0 && !strcmp(symbol->name, "mainx20"))
            {
                found = 1;
            }
            symbol->flags |= FLAG_DEFINED;
            symbol->next = *insyms;
            *insyms = symbol;
        }
        else
        {
            // is an outsymbol
            symbol->flags |= FLAG_OUTSYM;
            symbol->next = *outsyms;
            *outsyms = symbol;
        }
    }
    return found;
}

long generate_output_file(fheader_t* fh, char* filename, char is_object)
{
    // get total size needed to allocate
    int num_files = 0;
    long insym_words = 0;
    long outsym_words = 0;
    long object_words = 0;
    long bytes_written = 0;
    fheader_t* cur = fh;
    while (cur)
    {
        num_files +=   1;
        insym_words +=  cur->data[0];
        outsym_words += cur->data[1];
        object_words += cur->data[2];
        //total_bytes +=  cur->size;
        cur = cur->next;
    }
    if (!object_words) exit(50);

    #if (DEBUG)
        printf("insym:  %ld words\n", insym_words);
        printf("outsym: %ld words\n", outsym_words);
        printf("code:   %ld words\n ", object_words);
    #endif

    // initialize output file
    uint32_t* output_buffer = malloc((3 + insym_words + outsym_words + object_words) * sizeof(uint32_t));
    uint32_t* const insym_section = output_buffer + 3;
    uint32_t* const outsym_section = insym_section + insym_words;
    uint32_t* code_section = outsym_section + outsym_words;
    
    // initialize each section
    // uint32_t* insym_section = malloc(sizeof(uint32_t) * insym_words);
    // uint32_t* outsym_section = malloc(sizeof(uint32_t) * outsym_words);
    // uint32_t* code_section = malloc(sizeof(uint32_t) * object_words);

    // build object file
    // (slow algorithm for checking for duplicate insymbols while inserting)
    fheader_t* curf = fh;
    long target = 0;
    long pc_offset = 0;
    while (curf)
    {
        // insert insymbol
        sym_t* curs = curf->insyms;
        while (curs)
        {
            for (int i = 0; i < target; i += 5)
            {
                // check to see if this insymbol name matches the current one
                if (strncmp(curs->name, (char*)(insym_section + i), 16) == 0)
                {
                    // match found; fatal error
                    fprintf(stderr, "Fatal error: Duplicate insymbol \"%s\"\n", curs->name);
                    exit(1);
                }
            }
            // no matches found; insert insymbol here
            memcpy((char*)(insym_section + target), curs->name, 16);
            insym_section[target + 4] = curs->address + pc_offset;
        
            curs = curs->next;
            target += 5;
            bytes_written += 5 * 4;
        }
        // insert object code
        uint32_t* file_data_ptr = curf->data + curf->data[0] + curf->data[1] + 3;
        memcpy(code_section + pc_offset, file_data_ptr, sizeof(uint32_t) * curf->data[2]);
        bytes_written += curf->data[2] * 4;

        pc_offset += curf->data[2]; // increase offset by length of object code
        curf = curf->next;
    }
    // sanity check
    if (DEBUG && insym_section + target != outsym_section) perror("<!> insym_section + target != outsym_section");

    // resolve all outsymbols
    curf = fh;
    target = 0;
    pc_offset = 0;
    while (curf)
    {
        sym_t* curs = curf->outsyms;
        while (curs)
        {
            // search NEW insymbol table for outsymbol name
            for (int i = 0; i < insym_words; i += 5)
            {
                if (strncmp(curs->name, (char*)(insym_section + i), 16) == 0)
                {
                    // match found; success
                    curs->flags |= FLAG_DEFINED;
                    // rewrite instruction to reference new address
                    uint32_t symbol_definition = insym_section[i + 4];
                    long pc = pc_offset + curs->address;
                    uint32_t* instr = code_section + pc;
                    int32_t new_addr = symbol_definition - (pc + 1);
                    switch (*instr & 0xff)
                    {
                        // op addr
                        case 0x0f:
                        case 0x14:
                        // op reg,addr
                        case 0x01:
                        case 0x02:
                        case 0x04:
                            *instr = ((new_addr << 12) & 0xfffff000) | ((*instr) & 0xfff);
                            break;
                        // op reg,reg,addr
                        case 0x11:
                        case 0x12:
                        case 0x13:
                        case 0x15:
                            *instr = ((new_addr << 16) & 0xffff0000) | ((*instr) & 0xffff);
                            break;
                    }
                    
                }
            }
            if (!(curs->flags & FLAG_DEFINED))
            {
                // no match found
                fprintf(stderr, "Error: Unresolved outsymbol \"%s\"\n", curs->name);
                // must write outsymbol to outsymbol table
                memcpy((char*)(outsym_section + target), curs->name, 16);
                outsym_section[target + 4] = curs->address + pc_offset;
                target += 5;
                bytes_written += 5 * 4;
                is_object = 1;
            }
            curs = curs->next;
        }
        pc_offset += curf->data[2]; // increase offset by length of object code
        curf = curf->next;
    }
    // write first 3 words (lengths)
    output_buffer[0] = insym_words;
    output_buffer[1] = target;
    output_buffer[2] = object_words;

    // convert filename if object file
    if (is_object)
    {
        char* end = filename + strlen(filename) - 1;
        end[-2] = 'o';
        end[-1] = 'b';
        end[0]  = 'j';
    }
    // create file pointer
    FILE* fp = fopen(filename, "wb");
    if (!fp) { perror("failed to write to executable"); exit(-2); }
    // write to file
    size_t words = fwrite((char*)output_buffer, sizeof(uint32_t), 3, fp);
    words = fwrite((char*)insym_section, sizeof(uint32_t), insym_words, fp);
    if (words != insym_words) { fprintf(stderr, "ins %zd %zd\n", insym_words, words); }
    words = fwrite((char*)outsym_section, sizeof(uint32_t), target, fp);
    if (words != target) { fprintf(stderr, "ous %zd %zd\n", outsym_words, words); }
    words = fwrite((char*)code_section, sizeof(uint32_t), object_words, fp);
    if (words != object_words) { fprintf(stderr, "obj %zd %zd\n", object_words, words); }

    fclose(fp);
    free(output_buffer);
    return bytes_written + (3 * 4);
}

void free_all(fheader_t* fh)
{
    while (fh)
    {
        while (fh->insyms)
        {
            sym_t* cur = fh->insyms;
            if (cur->name) free(cur->name);
            fh->insyms = cur->next;
            free(cur);
        }
        while (fh->outsyms)
        {
            sym_t* cur = fh->outsyms;
            if (cur->name) free(cur->name);
            fh->outsyms = cur->next;
            free(cur);
        }
        fheader_t* cur = fh;
        if (cur->data) free(cur->data);
        fh = cur->next;
        free(cur);
    }
}