#include "pcb.h"
#include <stdio.h>

PCB createPCB(int processID, int arrivalTime, int burstTime,
              int memoryLowerBound, int memoryUpperBound, int instructionEnd) {
    PCB process;
    process.processID        = processID;
    process.state            = NEW;
    process.programCounter   = 0;
    process.memoryLowerBound = memoryLowerBound;
    process.memoryUpperBound = memoryUpperBound;
    process.instructionEnd   = instructionEnd;
    process.arrivalTime      = arrivalTime;
    process.burstTime        = burstTime;
    process.waitingTime      = 0;
    process.onDisk           = 0;
    return process;
}

const char* getStateName(ProcessState state) {
    switch (state) {
        case NEW:      return "NEW";
        case READY:    return "READY";
        case RUNNING:  return "RUNNING";
        case BLOCKED:  return "BLOCKED";
        case FINISHED: return "FINISHED";
        default:       return "UNKNOWN";
    }
}

void printPCB(PCB* process) {
    printf("---------------------------\n");
    printf("Process ID     : %d\n",   process->processID);
    printf("State          : %s\n",   getStateName(process->state));
    printf("Program Counter: %d\n",   process->programCounter);
    printf("Memory Bounds  : [%d-%d]\n", process->memoryLowerBound, process->memoryUpperBound);
    printf("Instruction End: %d\n",   process->instructionEnd);
    printf("Arrival Time   : %d\n",   process->arrivalTime);
    printf("Burst Time     : %d\n",   process->burstTime);
    printf("Waiting Time   : %d\n",   process->waitingTime);
    printf("On Disk        : %s\n",   process->onDisk ? "YES" : "NO");
    printf("---------------------------\n");
}