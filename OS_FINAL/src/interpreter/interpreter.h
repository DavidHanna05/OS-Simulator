#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "../process/pcb.h"
#include "../memory/memory.h"
#include "../mutex/mutex_manager.h"
#include "../scheduler/scheduler.h"

#define PRINT           0
#define ASSIGN          1
#define WRITE_FILE      2
#define READ_FILE       3
#define PRINT_FROM_TO   4
#define SEM_WAIT        5
#define SEM_SIGNAL      6
#define UNKNOWN        -1

// Counts non-empty lines in a program file
int countInstructions(const char* filename);

// Writes PCB fields and instruction lines into memory
void loadInstructions(PCB* process, const char* filename);

// Creates a process at arrival time — allocates memory, creates PCB, loads instructions
// allProcesses and processCount are needed in case a swap is required
// Burst time is NOT a parameter — it is derived from the program file automatically
PCB* createProcess(int processID, int arrivalTime,
                   const char* filename, PCB** allProcesses, int processCount);

// Executes one instruction for the given process
// Returns: 1 = executed, 0 = blocked, 2 = already finished (no clock cost)
int executeInstruction(PCB* process, Queue* readyQueue, Queue* blockedQueue,
                       PCB** allProcesses, int processCount);

#endif