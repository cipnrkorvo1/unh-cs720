#include "binary_info.h"
#include "elf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void* fbuf;
size_t bufsize;
char valid = 0;

size_t section_name_table_offset = -1;
size_t section_name_table_size;

int _mode = 0;
int _class = 0;


int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Error: incorrect usage\n");
        fprintf(stderr, "Usage: binary_info <option> <filename>\n");
        exit(1);
    }

    // verify correct option
    char* opt = argv[1];
    if (!strcmp(opt, "-h"))
    {
        _mode = OPT_H;
    }
    else if (!strcmp(opt, "-sn"))
    {
        _mode = OPT_SN;
    }
    else if (!strcmp(opt, "-sec"))
    {
        _mode = OPT_SEC;
    }
    else if (!strcmp(opt, "-sym"))
    {
        _mode = OPT_SYM;
    }
    else if (!strcmp(opt, "-dyn"))
    {
        _mode = OPT_DYN;
    } else {
        fprintf(stderr, "Invalid option: %s\n", opt);
        exit(1);
    }

    // attempt to open file
    printf("File: %s\n", argv[2]);
    FILE* fp = fopen(argv[2], "rb");
    if (fp) {
        // read file into buffer
        if (fseek(fp, 0, SEEK_END) == 0)
        {
            // get size of file
            bufsize = ftell(fp);
            if (bufsize <= 0)
            {
                fprintf(stderr, "File %s is empty.\n", argv[2]);
                exit(1);
            }
            // allocate enough bytes into buffer
            fbuf = malloc(bufsize+1);
            if (!fbuf)
            {
                fprintf(stderr, "Failed to allocate memory.\n");
                exit(1);
            }

            // go back to start
            if (fseek(fp, 0, SEEK_SET) != 0)
            {
                fprintf(stderr, "Failed to seek in file.\n");
                exit(1);
            }
            // read entire file into memory
            size_t len = fread(fbuf, 1, bufsize, fp);
            if (ferror(fp) != 0)
            {
                fprintf(stderr, "Error reading file.");
                exit(1);
            }
        }
        else
        {
            fprintf(stderr, "Failed to seek in file.\n");
            exit(1);
        }
    }
    else
    {
        fprintf(stderr, "Could not open file: %s\n", argv[1]);
        exit(1);
    }
    fclose(fp);

    // identify ELF file
    unsigned char* ident_buffer = (unsigned char*)fbuf;
    if (ident_buffer[EI_MAG0] != ELFMAG0 ||
        ident_buffer[EI_MAG1] != ELFMAG1 ||
        ident_buffer[EI_MAG2] != ELFMAG2 ||
        ident_buffer[EI_MAG3] != ELFMAG3)
        {
            fprintf(stderr, "Invalid file: failed to identify ELF magic number.\n");
            exit(2);
        }
    // determine class (impacts size)
    switch (ident_buffer[EI_CLASS])
    {
        case ELFCLASS32:
            _class = ELFCLASS32;
            break;
        case ELFCLASS64:
            _class = ELFCLASS64;
            break;
        default:
            fprintf(stderr, "Invalid file: invalid ELF class.\n");
            exit(2);
    }
    // determine if data type is valid
    switch (ident_buffer[EI_DATA])
    {
        case ELFDATA2LSB:
        case ELFDATA2MSB:
            break;
        default:
            fprintf(stderr, "Invalid file: invalid data type.\n");
            exit(2);
    }
    // determine if version is valid
    if (ident_buffer[EI_VERSION] != EV_CURRENT)
    {
        fprintf(stderr, "Invalid file: invalid version.\n");
        exit(2);
    }

    valid = 1;


    switch (_mode)
    {
        case OPT_H:
            print_header();
            break;
        case OPT_SN:
            string_dump(1);
            break;
        case OPT_SEC:
            string_dump(0);
            list_sect_headers();
            break;
        case OPT_SYM:
            symbol_dump();
            break;
        case OPT_DYN:
            dynsym_dump();
            break;
        default:
            printf("Unimplemented option!\n");
    }
    
    free(fbuf);
    return 0;
}


void print_header()
{
    if (!valid)
    {
        return;
    }

    Elf32_Ehdr* h32 = (Elf32_Ehdr*)fbuf;
    Elf64_Ehdr* h64 = (Elf64_Ehdr*)fbuf;

    // begin printing
    printf("ELF Header:\n");
    printf("Magic:");
    for (int i = 0; i < EI_NIDENT; i++)
    {
        printf(" %.2x", h32->e_ident[i]);
    }
    printf("\nClass: %s\n", _class == ELFCLASS32 ? "ELF32" : "ELF64");
    printf("Data: ");
    switch (h32->e_ident[EI_DATA])
    {
        case ELFDATA2LSB:
            printf("2's complement, little endian");
            break;
        case ELFDATA2MSB:
            printf("2's complement, big endian");
            break;
    }
    printf("\nType: ");
    {
        int type;
        type = _class == ELFCLASS32 ? h32->e_type : h64->e_type;
        switch (type)
        {
            case ET_NONE:
                printf("NONE (No file type)");
                break;
            case ET_REL:
                printf("REL (Relocatable file)");
                break;
            case ET_EXEC:
                printf("EXEC (Executable file)");
                break;
            case ET_DYN:
                printf("DYN (Shared object file)");
                break;
            case ET_CORE:
                printf("CORE (Core file)");
                break;
            default:
                fprintf(stderr, "[!] Illegal value for Type: %x. Stopping.\n", type);
                exit(3);
        };
    }
    
    printf("\nVersion: 0x%x\n", EV_CURRENT);
    if (_class == ELFCLASS32)
    {
        printf("Entry point address: 0x%x\n", h32->e_entry);
        printf("Start of program headers: %d (bytes into file)\n", h32->e_phoff);
        printf("Start of section headers: %d (bytes into file)\n", h32->e_shoff);
        printf("Flags: 0x%x\n", h32->e_flags);
        printf("Size of this header: %d (bytes)\n", h32->e_ehsize);
        printf("Size of program headers: %d (bytes)\n", h32->e_phentsize);
        printf("Number of program headers: %d\n", h32->e_phnum);
        printf("Size of section headers: %d (bytes)\n", h32->e_shentsize);
        printf("Number of section headers: %d\n", h32->e_shnum);
        printf("Section header string table index: %d\n", h32->e_shstrndx);
    }
    else
    {
        printf("Entry point address: 0x%lx\n", h64->e_entry);
        printf("Start of program headers: %ld (bytes into file)\n", h64->e_phoff);
        printf("Start of section headers: %ld (bytes into file)\n", h64->e_shoff);
        printf("Flags: 0x%x\n", h64->e_flags);
        printf("Size of this header: %d (bytes)\n", h64->e_ehsize);
        printf("Size of program headers: %d (bytes)\n", h64->e_phentsize);
        printf("Number of program headers: %d\n", h64->e_phnum);
        printf("Size of section headers: %d (bytes)\n", h64->e_shentsize);
        printf("Number of section headers: %d\n", h64->e_shnum);
        printf("Section header string table index: %d\n", h64->e_shstrndx);
    }

}

void string_dump(char print)
{
    if (!valid)
    {
        return;
    }

    Elf32_Ehdr* h32 = (Elf32_Ehdr*)fbuf;
    Elf64_Ehdr* h64 = (Elf64_Ehdr*)fbuf;

    size_t _sh_offset;
    size_t _tabsize;
    if (_class == ELFCLASS32)
    {
        // find .shstrtab
        size_t sht_offset = h32->e_shoff + h32->e_shentsize * h32->e_shstrndx;
        Elf32_Shdr* shdr = (Elf32_Shdr *)(fbuf + sht_offset);
        // extract info from header
        _sh_offset = shdr->sh_offset;
        _tabsize = shdr->sh_size;
    }
    else
    {
        // find .shstrtab
        size_t sht_offset = h64->e_shoff + h64->e_shentsize * h64->e_shstrndx;
        Elf64_Shdr* shdr = (Elf64_Shdr *)(fbuf + sht_offset);
        // extract info from header
        _sh_offset = shdr->sh_offset;
        _tabsize = shdr->sh_size;
    }

    section_name_table_offset = _sh_offset;
    section_name_table_size = _tabsize;

    char* shstrtab = (char*)(fbuf + section_name_table_offset);

    if (print) printf("Section names from \'.shstrtab\':\n");
    for (int i = 1; i < _tabsize; i++)
    {
        //if (print) printf("%c", shstrtab[i]);
        if (shstrtab[i] == '\0')
        {
            if (print) printf("\n");
        }
        else
        {
            if (print) printf("%c", shstrtab[i]);
        }
    }
}


static char* get_type_str(int type)
{
    switch(type)
    {
        case SHT_NULL:
            return "NULL";
        case SHT_PROGBITS:
            return "PROGBITS";
        case SHT_SYMTAB:
            return "SYMTAB";
        case SHT_STRTAB:
            return "STRTAB";
        case SHT_RELA:
            return "RELA";
        case SHT_HASH:
            return "HASH";
        case SHT_DYNAMIC:
            return "DYNAMIC";
        case SHT_NOTE:
            return "NOTE";
        case SHT_NOBITS:
            return "NOBITS";
        case SHT_REL:
            return "REL";
        case SHT_SHLIB:
            return "SHLIB";
        case SHT_DYNSYM:
            return "DYNSYM";
        case SHT_INIT_ARRAY:
            return "INIT_ARRAY";
        case SHT_FINI_ARRAY:
            return "FINI_ARRAY";
        case SHT_PREINIT_ARRAY:
            return "PREINIT_ARRAY";
        case SHT_GROUP:
            return "GROUP";
        case SHT_SYMTAB_SHNDX:
            return "SHNDX";
        case SHT_GNU_ATTRIBUTES:
            return "GNU_ATTRIBUTES";
        case SHT_GNU_HASH:
            return "GNU_HASH";
        case SHT_GNU_LIBLIST:
            return "GNU_LIBLIST";
        case SHT_CHECKSUM:
            return "CHECKSUM";
        case SHT_SUNW_move:
            return "MOVE";
        case SHT_SUNW_COMDAT:
            return "COMDAT";
        case SHT_SUNW_syminfo:
            return "SYMINFO";
        case SHT_GNU_verdef:
            return "VERDEF";
        case SHT_GNU_verneed:
            return "VERNEED";
        case SHT_GNU_versym:
            return "VERSYM";
        default:
            return "ILLEGAL_TYPE";
    }
}

void list_sect_headers()
{
    if (!valid || section_name_table_offset == -1)
    {
        return;
    }

    //fread(buffer, _header.e_shentsize, _header.e_shnum, fp);
    if (_class == ELFCLASS32)
    {
        // locate section header table
        Elf32_Ehdr* h32 = (Elf32_Ehdr*)fbuf;
        printf("There are %d section headers, starting at offset 0x%x\n",
            h32->e_shnum, h32->e_shoff);
        printf("Section Headers:\n");
        for (int i = 0; i < h32->e_shnum; i++)
        {
            Elf32_Shdr* shd = fbuf + h32->e_shoff;
            char *namestr = fbuf + section_name_table_offset + shd->sh_name;
            char *typestr = get_type_str(shd->sh_type);
            printf("[%2d] %s %s %.8x %.6x %.6x\n",
                i, namestr, typestr,
                shd->sh_addr, shd->sh_offset, shd->sh_size);
        }
    }
    else
    {
        // locate section header table
        Elf64_Ehdr* h64 = (Elf64_Ehdr*)fbuf;
        printf("There are %d section headers, starting at offset 0x%lx\n",
            h64->e_shnum, h64->e_shoff);
        printf("Section Headers:\n");
        for (int i = 0; i < h64->e_shnum; i++)
        {
            Elf64_Shdr* shd = fbuf + h64->e_shoff + (h64->e_shentsize * i);
            char *namestr = fbuf + section_name_table_offset + shd->sh_name;
            char *typestr = get_type_str(shd->sh_type);
            printf("[%2d] %s %s %.16lx %.6lx %.6lx\n",
                i, namestr, typestr,
                shd->sh_addr, shd->sh_offset, shd->sh_size);
        }
    }
}


static char* st_type_text(unsigned char st_info)
{
    
    unsigned char type = ELF32_ST_TYPE(st_info);

    switch (type)
    {
        case STT_NOTYPE:
            return "NOTYPE ";
        case STT_OBJECT:
            return "OBJECT ";
        case STT_FUNC:
            return "FUNC   ";
        case STT_SECTION:
            return "SECTION";
        case STT_FILE:
            return "FILE   ";
        case STT_COMMON:
            return "COMMON ";
        case STT_TLS:
            return "TLS    ";
        case STT_GNU_IFUNC:
            return "IFUNC  ";
        default:
            return "ILLEGAL";
    }
}

static char* st_bind_text(unsigned char st_info)
{
    unsigned char bind = ELF32_ST_BIND(st_info);
    switch (bind)
    {
        case STB_LOCAL:
            return "LOCAL ";
        case STB_GLOBAL:
            return "GLOBAL";
        case STB_WEAK:
            return "WEAK  ";
        case STB_GNU_UNIQUE:
            return "UNIQUE";
        default:
            return "ILLEGL";
    }
}

static char* st_other_text(unsigned char st_other)
{
    char vis = ELF32_ST_VISIBILITY(st_other);
    switch(vis)
    {
        case STV_DEFAULT:
            return "DEFAULT";
        case STV_INTERNAL:
            return "INTERNAL";
        case STV_HIDDEN:
            return "HIDDEN ";
        case STV_PROTECTED:
            return "PROTECTED";
        default:
            return "ILLEGAL";
    }
}

void symbol_dump()
{
    if (!valid)
    {
        return;
    }

    if (_class == ELFCLASS32)
    {
        Elf32_Ehdr* h32 = fbuf;
        // locate .symtab AND .strtab
        Elf32_Shdr* ptr = fbuf + h32->e_shoff;
        Elf32_Shdr* symtab_header = NULL;
        for (int i = 0; i < h32->e_shnum; i++)
        {
            if (ptr->sh_type == SHT_SYMTAB)
            {
                // found!
                symtab_header = ptr;
                break;
            }
            ptr += 1;
        }
        if (symtab_header == NULL)
        {
            fprintf(stderr, "Failed to find \'.symtab\'!\n");
            return;
        }

        // find strtab as an address/offset
        int sth_idx = symtab_header->sh_link;
        Elf32_Shdr* strtab_header = fbuf + h32->e_shoff + (h32->e_shentsize * sth_idx);
        size_t strtab_offset = strtab_header->sh_offset;
        size_t strtab_size = strtab_header->sh_size;

        int entries = symtab_header->sh_size / symtab_header->sh_entsize;
        printf("Symbol table \'.symtab\' contains %d entries:\n", entries);
        
        printf("   Num:    Value          Size Type    Bind   Vis      Ndx Name\n");
        // initialize first symbol
        Elf32_Sym* symbol = fbuf + symtab_header->sh_offset;
        for (int i = 1; i <= entries; i++)
        {
            printf("%6d: %.8x  %4u %s %s %s  ",
                i, symbol->st_value, symbol->st_size,
                st_type_text(symbol->st_info), st_bind_text(symbol->st_info),
                st_other_text(symbol->st_other));
            int ndx = symbol->st_shndx;
            switch (ndx) {
                case 0:
                    printf("UND ");
                    break;
                case 0xfff1:
                    printf("ABS ");
                    break;
                default:
                    printf("%3d ", ndx);
            }
            printf("%s\n", (char*)(fbuf + strtab_offset + symbol->st_name));
            // go to next symbol
            symbol = fbuf + symtab_header->sh_offset + (i * symtab_header->sh_entsize);
        }
    }
    else
    {
        Elf64_Ehdr* h64 = fbuf;
        // locate .symtab
        Elf64_Shdr* ptr = fbuf + h64->e_shoff;
        Elf64_Shdr* symtab_header = NULL;
        for (int i = 0; i < h64->e_shnum; i++)
        {
            if (ptr->sh_type == SHT_SYMTAB)
            {
                // found!
                symtab_header = ptr;
                break;
            }
            ptr += 1;
        }
        if (symtab_header == NULL)
        {
            fprintf(stderr, "Failed to find \'.symtab\'!\n");
            return;
        }

        // find strtab as an address/offset
        int sth_idx = symtab_header->sh_link;
        Elf64_Shdr* strtab_header = fbuf + h64->e_shoff + (h64->e_shentsize * sth_idx);
        size_t strtab_offset = strtab_header->sh_offset;
        size_t strtab_size = strtab_header->sh_size;

        int entries = symtab_header->sh_size / symtab_header->sh_entsize;
        printf("Symbol table \'.symtab\' contains %d entries:\n",
            entries);
        
        printf("   Num:    Value          Size Type    Bind   Vis      Ndx Name\n");
        // initialize first symbol
        Elf64_Sym* symbol = fbuf + symtab_header->sh_offset;
        for (int i = 1; i <= entries; i++)
        {
            printf("%6d: %.16lx  %4lu %s %s %s  ",
                i, symbol->st_value, symbol->st_size,
                st_type_text(symbol->st_info), st_bind_text(symbol->st_info),
                st_other_text(symbol->st_other));
            int ndx = symbol->st_shndx;
            switch (ndx) {
                case 0:
                    printf("UND ");
                    break;
                case 0xfff1:
                    printf("ABS ");
                    break;
                default:
                    printf("%3d ", ndx);
            }
            printf("%s\n", (char*)(fbuf + strtab_offset + symbol->st_name));
            // go to next symbol
            symbol = fbuf + symtab_header->sh_offset + (i * symtab_header->sh_entsize);
        }
    }

}


void dynsym_dump()
{
    if (!valid)
    {
        return;
    }

    if (_class == ELFCLASS32)
    {
        Elf32_Ehdr* h32 = fbuf;
        // locate dynsym
        Elf32_Shdr* ptr = fbuf + h32->e_shoff;
        Elf32_Shdr* dynsym_header = NULL;
        for (int i = 0; i < h32->e_shnum; i++)
        {
            if (ptr->sh_type == SHT_DYNSYM)
            {
                // found!
                dynsym_header = ptr;
                break;
            }
            ptr += 1;
        }
        if (dynsym_header == NULL)
        {
            fprintf(stderr, "Failed to find \'.dynsym\'!\n");
            return;
        }

        // find .dynstr
        int dyn_idx = dynsym_header->sh_link;
        Elf32_Shdr* dynstr_header = fbuf + h32->e_shoff + (h32->e_shentsize * dyn_idx);
        size_t dynstr_offset = dynstr_header->sh_offset;
        size_t dynstr_size = dynstr_header->sh_size;

        int entries = dynsym_header->sh_size / dynsym_header->sh_entsize;
        printf("Symbol table \'.dynsym\' contains %d entries:\n", entries);
        printf("   Num:    Value          Size Type    Bind   Vis      Ndx Name\n");
        // initialize first dynamic symbol
        Elf32_Sym* dyn = fbuf + dynsym_header->sh_offset;
        for (int i = 1; i <= entries; i++)
        {
            printf("%6d: %.8x  %4u %s %s %s  ",
                i, dyn->st_value, dyn->st_size,
                st_type_text(dyn->st_info), st_bind_text(dyn->st_info),
                st_other_text(dyn->st_other));
            int ndx = dyn->st_shndx;
            switch (ndx) {
                case 0:
                    printf("UND ");
                    break;
                case 0xfff1:
                    printf("ABS ");
                    break;
                default:
                    printf("%3d ", ndx);
            }
            printf("%s\n", (char*)(fbuf + dynstr_offset + dyn->st_name));
            // go to next symbol
            dyn = fbuf + dynsym_header->sh_offset + (i * dynsym_header->sh_entsize);
        }

    }
    else
    {
        Elf64_Ehdr* h64 = fbuf;
        // locate dynsym
        Elf64_Shdr* ptr = fbuf + h64->e_shoff;
        Elf64_Shdr* dynsym_header = NULL;
        for (int i = 0; i < h64->e_shnum; i++)
        {
            if (ptr->sh_type == SHT_DYNSYM)
            {
                // found!
                dynsym_header = ptr;
                break;
            }
            ptr += 1;
        }
        if (dynsym_header == NULL)
        {
            fprintf(stderr, "Failed to find \'.dynsym\'!\n");
            return;
        }

        // find .dynstr
        int dyn_idx = dynsym_header->sh_link;
        Elf64_Shdr* dynstr_header = fbuf + h64->e_shoff + (h64->e_shentsize * dyn_idx);
        size_t dynstr_offset = dynstr_header->sh_offset;
        size_t dynstr_size = dynstr_header->sh_size;

        int entries = dynsym_header->sh_size / dynsym_header->sh_entsize;
        printf("Symbol table \'.dynsym\' contains %d entries:\n", entries);
        printf("   Num:    Value          Size Type    Bind   Vis      Ndx Name\n");
        // initialize first dynamic symbol
        Elf64_Sym* dyn = fbuf + dynsym_header->sh_offset;
        for (int i = 1; i <= entries; i++)
        {
            printf("%6d: %.16lx  %4lu %s %s %s  ",
                i, dyn->st_value, dyn->st_size,
                st_type_text(dyn->st_info), st_bind_text(dyn->st_info),
                st_other_text(dyn->st_other));
            int ndx = dyn->st_shndx;
            switch (ndx) {
                case 0:
                    printf("UND ");
                    break;
                case 0xfff1:
                    printf("ABS ");
                    break;
                default:
                    printf("%3d ", ndx);
            }
            printf("%s\n", (char*)(fbuf + dynstr_offset + dyn->st_name));
            // go to next symbol
            dyn = fbuf + dynsym_header->sh_offset + (i * dynsym_header->sh_entsize);
        }

    }
}
