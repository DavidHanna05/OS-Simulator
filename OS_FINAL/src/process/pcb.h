#ifndef PCB_H
#define PCB_H

typedef enum { NEW, READY, RUNNING, BLOCKED, FINISHED } ProcessState;

typedef struct {
    int processID;
    ProcessState state;
    int programCounter;
    int memoryLowerBound;
    int memoryUpperBound;
    int instructionEnd;  // memory index of last instruction line
    int arrivalTime;
    int burstTime;
    int waitingTime;
    int onDisk;          // 1 = process is on disk, 0 = process is in memory
} PCB;

PCB createPCB(int processID, int arrivalTime, int burstTime,
              int memoryLowerBound, int memoryUpperBound, int instructionEnd);
const char* getStateName(ProcessState state);
void printPCB(PCB* process);

#endif