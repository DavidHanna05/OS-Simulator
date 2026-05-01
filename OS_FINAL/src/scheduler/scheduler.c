#include "scheduler.h"
#include <stdio.h>
#include "../process/pcb.h"

// Global queue definitions
Queue readyQueue;
Queue blockQueue;
Queue mlfqQueues[MLFQ_LEVELS];

// ─── addToReadyQueue ──────────────────────────────────────────────────────────
// Adds a process to the end of the ready queue
void addToReadyQueue(PCB* process) {
    readyQueue.processes[readyQueue.count++] = process;
}

// ─── addToBlockQueue ──────────────────────────────────────────────────────────
// Adds a process to the end of the blocked queue
void addToBlockQueue(PCB* process) {
    blockQueue.processes[blockQueue.count++] = process;
}

// ─── dequeueFromReadyQueue ────────────────────────────────────────────────────
// Removes and returns the first process from the ready queue
PCB* dequeueFromReadyQueue() {
    if (readyQueue.count == 0) {
            return NULL;
    }
    PCB* first = readyQueue.processes[0];
    for (int i = 1; i < readyQueue.count; i++) {
        readyQueue.processes[i - 1] = readyQueue.processes[i];
    }
    readyQueue.count--;
    return first;
}

// ─── dequeueFromBlockQueue ────────────────────────────────────────────────────
// Removes and returns the first process from the blocked queue
PCB* dequeueFromBlockQueue() {
    if (blockQueue.count == 0) {
        printf("[SCHEDULER] Blocked Queue is empty.\n");
        return NULL;
    }
    PCB* first = blockQueue.processes[0];
    for (int i = 1; i < blockQueue.count; i++) {
        blockQueue.processes[i - 1] = blockQueue.processes[i];
    }
    blockQueue.count--;
    return first;
}

// ─── removefromReadyQueue ─────────────────────────────────────────────────────
// Removes a specific process from the ready queue by pointer
PCB* removefromReadyQueue(PCB* process) {
    for (int i = 0; i < readyQueue.count; i++) {
        if (readyQueue.processes[i] == process) {
            PCB* removed = readyQueue.processes[i];
            for (int j = i + 1; j < readyQueue.count; j++) {
                readyQueue.processes[j - 1] = readyQueue.processes[j];
            }
            readyQueue.count--;
            return removed;
        }
    }
    printf("[SCHEDULER] Process %d not found in Ready Queue.\n", process->processID);
    return NULL;
}

// ─── RRScheduler ─────────────────────────────────────────────────────────────
// Round Robin: dequeues and returns the first process in the ready queue
// Main is responsible for:
//   - executing TIME_SLICE instructions
//   - putting the process back at the end of the queue if still running
//   - marking it FINISHED if done
PCB* RRScheduler() {
    if (readyQueue.count == 0) {
            return NULL;
    }
    PCB* process = dequeueFromReadyQueue();
    return process;
}

// ─── HRRNScheduler ───────────────────────────────────────────────────────────
// Highest Response Ratio Next: selects process with highest response ratio
// Response Ratio = (waitingTime + burstTime) / burstTime
// Non-preemptive — selected process runs until it blocks or finishes
PCB* HRRNScheduler(int clock) {
    if (readyQueue.count == 0) return NULL;

    double max     = -1;
    int maxIndex   = -1;

    for (int i = 0; i < readyQueue.count; i++) {
        PCB* p = readyQueue.processes[i];
        // Use the tracked waitingTime — incremented each cycle in main
        double responseRatio = (p->waitingTime + p->burstTime) / (double)p->burstTime;
        if (responseRatio > max) {
            max      = responseRatio;
            maxIndex = i;
        }
    }

    PCB* selected = readyQueue.processes[maxIndex];
    removefromReadyQueue(selected);
    return selected;
}

// ─── MLFQ Functions ───────────────────────────────────────────────────────────

// Adds a process to a specific MLFQ level
void addToMLFQQueue(PCB* process, int level) {
    if (level < 0 || level >= MLFQ_LEVELS) {
            return;
    }
    mlfqQueues[level].processes[mlfqQueues[level].count++] = process;
}

// Removes and returns the front process from a specific MLFQ level
PCB* dequeueFromMLFQQueue(int level) {
    if (level < 0 || level >= MLFQ_LEVELS) {
            return NULL;
    }
    Queue* q = &mlfqQueues[level];
    if (q->count == 0) {
            return NULL;
    }
    PCB* front = q->processes[0];
    for (int i = 1; i < q->count; i++) {
        q->processes[i - 1] = q->processes[i];
    }
    q->count--;
    return front;
}

// Per-process MLFQ level tracking — two separate tables:
// mlfqRunLevel:  the level the process RAN from (restore here on unblock)
// mlfqNextLevel: the level to DEMOTE to after full quantum (used by main)
static int mlfqRunLevel[MAX_PROCESSES + 1];
static int mlfqDemoteLevel[MAX_PROCESSES + 1];

PCB* MLFQScheduler() {
    int chosenLevel = -1;
    for (int lvl = 0; lvl < MLFQ_LEVELS; lvl++) {
        if (mlfqQueues[lvl].count > 0) {
            chosenLevel = lvl;
            break;
        }
    }
    if (chosenLevel == -1) return NULL;

    PCB* process = dequeueFromMLFQQueue(chosenLevel);

    // Save which level this process ran from — sem_signal restores to this level
    mlfqRunLevel[process->processID] = chosenLevel;

    int nextLevel = (chosenLevel < MLFQ_LEVELS - 1)
                    ? chosenLevel + 1
                    : MLFQ_LEVELS - 1;

    // Save demotion level — handleEndOfQuantum uses this
    mlfqDemoteLevel[process->processID] = nextLevel;

    // Pass nextLevel to main temporarily via waitingTime
    // selectProcess in main.c restores waitingTime from mlfqSavedWait immediately
    process->waitingTime = nextLevel;
    return process;
}

// Returns the level to RESTORE a process to when it unblocks (same level it blocked from)
int getMlfqRestoreLevel(int processID) {
    return mlfqRunLevel[processID];
}

// Returns the level to DEMOTE a process to after full quantum expires
int getMLFQNextLevel(PCB* process) {
    return mlfqDemoteLevel[process->processID];
}

// ─── Print Functions ──────────────────────────────────────────────────────────

void printReadyQueue() {
    printf("[READY QUEUE] (%d): ", readyQueue.count);
    if (readyQueue.count == 0) {
        printf("empty");
    }
    for (int i = 0; i < readyQueue.count; i++) {
        printf("P%d", readyQueue.processes[i]->processID);
        if (i < readyQueue.count - 1) printf(" | ");
    }
    printf("\n");
}

void printBlockQueue() {
    printf("[BLOCKED QUEUE] (%d): ", blockQueue.count);
    if (blockQueue.count == 0) {
        printf("empty");
    }
    for (int i = 0; i < blockQueue.count; i++) {
        printf("P%d", blockQueue.processes[i]->processID);
        if (i < blockQueue.count - 1) printf(" | ");
    }
    printf("\n");
}

// Prints both ready and blocked queues — called after every scheduling event
void printQueues() {
    printReadyQueue();
    printBlockQueue();
}

void printMLFQQueues() {
    printf("[MLFQ QUEUES]\n");
    for (int lvl = 0; lvl < MLFQ_LEVELS; lvl++) {
        printf("  Queue %d (quantum=%d): ", lvl, (1 << lvl));
        if (mlfqQueues[lvl].count == 0) {
            printf("empty");
        }
        for (int i = 0; i < mlfqQueues[lvl].count; i++) {
            printf("P%d", mlfqQueues[lvl].processes[i]->processID);
            if (i < mlfqQueues[lvl].count - 1) printf(" | ");
        }
        printf("\n");
    }
}