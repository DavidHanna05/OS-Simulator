#ifndef MEMORY_H
#define MEMORY_H

#include "../process/pcb.h"

#define MEMORY_SIZE     40
#define MAX_LINE_LENGTH 100
#define MAX_LINES       20
#define DISK_DIR        "disk/"

extern char memory[MEMORY_SIZE][MAX_LINE_LENGTH];

// Finds a contiguous free block of the required size
// Returns lowerBound if found, -1 if memory is full
int allocateMemory(int instCount);

// Frees a process's memory slots by clearing them
void freeMemory(PCB* process);

// Writes a process's memory region to disk/processX.txt
// Sets process->onDisk = 1
void swapToDisk(PCB* process);

// Reads a process's memory region back from disk/processX.txt
// Finds free space (swapping another process out if needed)
// Sets process->onDisk = 0
void swapFromDisk(PCB* process, PCB** allProcesses, int processCount);

// Prints the full memory contents in human readable format
void printMemory();

#endif