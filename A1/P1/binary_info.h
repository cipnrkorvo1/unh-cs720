#define OPT_H    1
#define OPT_SN   2
#define OPT_SEC  3
#define OPT_SYM  4
#define OPT_DYN  5

/*
    Gathers header information from FILE* and puts it into a
    global _header object. Does not close or rewind the FILE*,
    only reads until end of header.
    32 and 64-bit versions (does not verify!)
    Also checks the verify sentinel so informational functions can run.
*/
void analyze_header32();
void analyze_header64();

/*
    Prints the basic header information.
    Used when the mode is set to OPT_H
*/
void print_header();

/*
    String dump of all names in the .shstrtab section
*/
void string_dump(char print);

/*
    Lists all section headers with name, type, address, offset, size
*/
void list_sect_headers();
static char* get_type_str(int type);

/*
    Dump of .symtab symbol table
*/
void symbol_dump();

/*
    Dump of .dynsym symbol table
*/
void dynsym_dump();