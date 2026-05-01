#ifndef SYSTEM_CALL_H
#define SYSTEM_CALL_H

#include "../memory/memory.h"
#include "../process/pcb.h"

// 1. Read data from a file on disk
void sys_readFile(const char* filename, char* outBuf, int bufSize);

// 2. Write data to a file on disk
void sys_writeFile(const char* filename, const char* data);

// 3. Print data to the screen
void sys_print(const char* data);

// 4. Take text input from the user
void sys_userInput(char* outBuf, int bufSize);

// 5. Read a variable's value from a process's memory
const char* sys_readMemory(PCB* process, const char* varName);

// 6. Write a variable's value into a process's memory
void sys_writeMemory(PCB* process, const char* varName, const char* value);

// Print all integers from start to end inclusive
void sys_printFromTo(int start, int end);

#endif