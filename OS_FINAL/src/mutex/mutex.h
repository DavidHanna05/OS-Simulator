#ifndef MUTEX_H
#define MUTEX_H

#include "../process/pcb.h"
#include "../scheduler/scheduler.h"

typedef struct {
    int   value;
    PCB*  owner;
    Queue blockedQueue;
} Mutex;

void initMutex(Mutex* mutex);

// Returns 1 if process acquired mutex, 0 if process was blocked
int sem_wait(Mutex* mutex, PCB* process, Queue* readyQueue, Queue* blockQueue);

// Releases mutex — unblocked process is added to MLFQ Queue 0
void sem_signal(Mutex* mutex, Queue* readyQueue, Queue* blockQueue);

void printMutex(const char* name, Mutex* mutex);

#endif