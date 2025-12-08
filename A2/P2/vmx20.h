//
// vmx20.h
//
// public interface for vmx20 processor simulation
//

#include <stdint.h>

// maximum number of processors
#define VMX20_MAX_PROCESSORS 16


// error codes 
#define VMX20_NORMAL_TERMINATION 0

#define VMX20_INITIALIZE_FAILURE -1
#define VMX20_FILE_NOT_FOUND -2
#define VMX20_FILE_IS_NOT_VALID -3

#define VMX20_FILE_CONTAINS_OUTSYMBOLS -4

#define VMX20_DIVIDE_BY_ZERO -5
#define VMX20_ADDRESS_OUT_OF_RANGE -6
#define VMX20_ILLEGAL_INSTRUCTION -7

// initialize the vm
//   function returns a handle to the structure holding the vm
//	 an error number is returned through the second
//     parameter
//   the following error numbers are supported:
//	   VMX20_NORMAL_TERMINATION
//     VMX20_INITIALIZE_FAILURE
void *initVm(int32_t *errorNumber);

// load an executable file
//   only one executable file may be loaded at a time
//   the function returns 1 if successful and 0 otherwise
//   if 0 is returned then an error number is returned through the third
//     parameter
//   the following error numbers are supported:
//     VMX20_FILE_NOT_FOUND
//     VMX20_FILE_CONTAINS_OUTSYMBOLS
//     VMX20_FILE_IS_NOT_VALID

int32_t loadExecutableFile(void *handle, char *filename, int32_t *errorNumber);

// get the address of a symbol in the current executable file
//   the label must be a symbol in the insymbol section of the executable file
//   the address is returned through the third parameter
//   the function returns 1 if successful and 0 otherwise
int32_t getAddress(void *handle, char *label, uint32_t *outAddr);

// read a word from memory
//   the word is returned through the third parameter
//   the function returns 1 if successful and 0 otherwise
int32_t getWord(void *handle, uint32_t addr, int32_t *outWord);

// write a word to memory
//   the function returns 1 if successful and 0 otherwise
int32_t putWord(void *handle, uint32_t addr, int32_t word);

// execute the current loaded executable file
//   the function returns 1 if all processors are successfully started and
//     0 otherwise
//   the second parameter specifies the number of processors to use
//   the third parameter provides an initial SP value for each processor
//   the fourth parameter is used to return the termination status for
//     each processor
//   the following termination statuses are supported:
//     VMX20_NORMAL_TERMINATION
//     VMX20_DIVIDE_BY_ZERO
//     VMX20_ADDRESS_OUT_OF_RANGE
//     VMX20_ILLEGAL_INSTRUCTION
//   the fourth parameter is a Boolean indicating whether an instruction
//     trace should be be printed to stderr
//   Note: that all other registers will be initialized to 0, including
//     the PC and the FP.
//
int32_t execute(void *handle, uint32_t numProcessors, uint32_t initialSP[],
      int terminationStatus[], int32_t trace);

// disassemble the word at the given address
//   return 1 if successful and 0 otherwise
//   the second parameter contains the address of the word to disassemble
//   the third parameter is a pointer to a buffer where the output should be
//     placed
//   the output will be the opcode followed by a space, followed by the
//     comma separated operands (if any). each comma will be followed by
//     a space. PC-relative addresses are converted to absolute addresses
//     and displayed in decimal. offsets and immediate constants are displayed
//     in decimal.
//   the caller can rely that the output will certainly not consume more than
//     100 characters
//   the third parameter is used to return an error code if an error is
//     encountered
//   the following error codes are supported:
//     VMX20_ADDRESS_OUT_OF_RANGE
//     VMX20_ILLEGAL_INSTRUCTION
int disassemble(void *handle, uint32_t address, char *buffer, int32_t *errorNumber);

// cleanup everything after simulation is done
void cleanup(void *handle);