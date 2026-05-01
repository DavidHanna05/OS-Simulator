#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../process/pcb.h"

#define MAX_PROCESSES 10
#define MLFQ_LEVELS   4
#define RR_QUANTUM    2

typedef struct {
    PCB* processes[MAX_PROCESSES];
    int count;
} Queue;

// Global queues — defined in scheduler.c, accessible everywhere via extern
extern Queue readyQueue;
extern Queue blockQueue;
extern Queue mlfqQueues[MLFQ_LEVELS];

// Queue operations
void addToReadyQueue(PCB* process);
void addToBlockQueue(PCB* process);
PCB* dequeueFromReadyQueue();
PCB* dequeueFromBlockQueue();
PCB* removefromReadyQueue(PCB* process);

// Scheduling algorithms
// RR: dequeues and returns the first process — main handles time slice and re-queuing
PCB* RRScheduler();

// HRRN: selects process with highest response ratio = (waitingTime + burstTime) / burstTime
PCB* HRRNScheduler(int clock);

// MLFQ: selects from highest priority non-empty queue
PCB* MLFQScheduler();
void addToMLFQQueue(PCB* process, int level);
PCB* dequeueFromMLFQQueue(int level);
int  getMlfqRestoreLevel(int processID);  // level to restore on unblock
int  getMLFQNextLevel(PCB* process);      // next level after quantum expires

// Print functions
void printReadyQueue();
void printBlockQueue();
void printQueues();
void printMLFQQueues();

#endif