#include "mutex.h"
#include "mutex_manager.h"
#include "../scheduler/scheduler.h"
#include <stdio.h>
#include <stdlib.h>

// Access the policy from main.c
// 0=RR, 1=HRRN, 2=MLFQ
extern int currentPolicy;

void initMutex(Mutex* mutex) {
    mutex->value = 1;
    mutex->owner = NULL;
    mutex->blockedQueue.count = 0;
}

static void removeFromQueue(Queue* queue, PCB* process) {
    for (int i = 0; i < queue->count; i++) {
        if (queue->processes[i] == process) {
            for (int j = i + 1; j < queue->count; j++) {
                queue->processes[j - 1] = queue->processes[j];
            }
            queue->count--;
            return;
        }
    }
}

int sem_wait(Mutex* mutex, PCB* process, Queue* readyQueue, Queue* blockQueue) {
    if (mutex->value == 1 || mutex->owner == process) {
        mutex->value = 0;
        mutex->owner = process;
        printMutex("acquired", mutex);
        return 1;
    } else {
        process->state = BLOCKED;
        removeFromQueue(readyQueue, process);
        mutex->blockedQueue.processes[mutex->blockedQueue.count++] = process;
        blockQueue->processes[blockQueue->count++] = process;

        printMutex("blocked on", mutex);
        return 0;
    }
}

void sem_signal(Mutex* mutex, Queue* readyQueue, Queue* blockQueue) {
    if (mutex->blockedQueue.count == 0) {
        mutex->value = 1;
        mutex->owner = NULL;
        printMutex("released", mutex);
    } else {
        PCB* next = mutex->blockedQueue.processes[0];
        for (int i = 1; i < mutex->blockedQueue.count; i++) {
            mutex->blockedQueue.processes[i - 1] = mutex->blockedQueue.processes[i];
        }
        mutex->blockedQueue.count--;

        removeFromQueue(blockQueue, next);

        mutex->owner = next;
        next->state  = READY;

        // Add unblocked process to correct queue based on current policy
        // For MLFQ: restore to the level it was at before blocking (stored in mlfqNextLevel)
        // NOT always Q0 — that would unfairly reset demoted processes
        if (currentPolicy == 2) { // MLFQ
            addToMLFQQueue(next, getMlfqRestoreLevel(next->processID));
        } else { // RR or HRRN
            addToReadyQueue(next);
        }

        printMutex("handed to", mutex);
    }
}

void printMutex(const char* name, Mutex* mutex) {
    if (mutex->owner != NULL) {
    }
    if (mutex->blockedQueue.count > 0) {
        for (int i = 0; i < mutex->blockedQueue.count; i++) {
        }
    }
}