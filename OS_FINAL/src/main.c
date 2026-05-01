#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include "interpreter/interpreter.h"
#include "memory/memory.h"
#include "mutex/mutex_manager.h"
#include "process/pcb.h"
#include "scheduler/scheduler.h"

extern char memory[MEMORY_SIZE][MAX_LINE_LENGTH];

#define MAX_PROCESSES  10
#define MAX_CYCLES     200

typedef enum { RR, HRRN, MLFQ } Policy;

Policy currentPolicy;

typedef struct {
    int  processID;
    int  arrivalTime;
    char filename[256];
} ProcessConfig;

static ProcessConfig processConfigs[] = {
    { 1, 0, "programs/Program 1.txt" },
    { 2, 1, "programs/Program_2.txt" },
    { 3, 4, "programs/Program_3.txt" },
};
static int processCount = sizeof(processConfigs) / sizeof(processConfigs[0]);

// --- printCycleQueues ---------------------------------------------------------
// Compact queue snapshot printed at the top of every clock cycle.
static void printCycleQueues(Policy policy) {
    printf("  Queues  | ");
    if (policy == MLFQ) {
        for (int lvl = 0; lvl < MLFQ_LEVELS; lvl++) {
            printf("Q%d(q=%d):[", lvl, 1 << lvl);
            if (mlfqQueues[lvl].count == 0) {
                printf("-");
            } else {
                for (int i = 0; i < mlfqQueues[lvl].count; i++) {
                    printf("P%d", mlfqQueues[lvl].processes[i]->processID);
                    if (i < mlfqQueues[lvl].count - 1) printf(",");
                }
            }
            printf("]  ");
        }
    } else {
        printf("Ready:[");
        if (readyQueue.count == 0) {
            printf("-");
        } else {
            for (int i = 0; i < readyQueue.count; i++) {
                printf("P%d", readyQueue.processes[i]->processID);
                if (i < readyQueue.count - 1) printf(",");
            }
        }
        printf("]  ");
    }
    printf("Blocked:[");
    if (blockQueue.count == 0) {
        printf("-");
    } else {
        for (int i = 0; i < blockQueue.count; i++) {
            printf("P%d", blockQueue.processes[i]->processID);
            if (i < blockQueue.count - 1) printf(",");
        }
    }
    printf("]\n");
}

// --- checkArrivals ------------------------------------------------------------
static void checkArrivals(int clock, PCB** allProcesses, int* createdCount,
                           Policy policy) {
    for (int i = 0; i < processCount; i++) {
        ProcessConfig* cfg = &processConfigs[i];
        if (clock == cfg->arrivalTime && *createdCount == i) {
            PCB* p = createProcess(cfg->processID, cfg->arrivalTime,
                                   cfg->filename, allProcesses, *createdCount);
            if (p) {
                p->state = READY;
                snprintf(memory[p->memoryLowerBound + 1], MAX_LINE_LENGTH, "state READY");
                allProcesses[(*createdCount)++] = p;
                if (policy == MLFQ) addToMLFQQueue(p, 0);
                else                addToReadyQueue(p);
                printf("  >> P%d arrived | burst=%d | mem[%d-%d] | added to %s\n",
                       p->processID, p->burstTime,
                       p->memoryLowerBound, p->memoryUpperBound,
                       policy == MLFQ ? "MLFQ Q0" : "Ready Queue");
            }
        }
    }
}

// --- syncWaitingToMemory ------------------------------------------------------
// Writes the current waitingTime from every PCB struct into memory.
// Called right before printMemory() to ensure the dump is always current.
static void syncWaitingToMemory(PCB** all, int count) {
    for (int i = 0; i < count; i++) {
        PCB* p = all[i];
        if (p->state != FINISHED && p->onDisk == 0) {
            snprintf(memory[p->memoryLowerBound + 8], MAX_LINE_LENGTH,
                     "waiting %d", p->waitingTime);
        }
    }
}static int allQueuesEmpty(Policy policy) {
    if (policy == MLFQ) {
        for (int i = 0; i < MLFQ_LEVELS; i++)
            if (mlfqQueues[i].count > 0) return 0;
        return blockQueue.count == 0;
    }
    return readyQueue.count == 0 && blockQueue.count == 0;
}

// Saved waitingTime before MLFQScheduler overwrites it
static int mlfqSavedWait[MAX_PROCESSES + 1];

static PCB* selectProcess(Policy policy, int clock) {
    if (policy == RR)   return RRScheduler();
    if (policy == HRRN) return HRRNScheduler(clock);
    if (policy == MLFQ) {
        PCB* p = MLFQScheduler();
        if (p == NULL) return NULL;
        // MLFQScheduler stored nextLevel in p->waitingTime — restore real value
        p->waitingTime = mlfqSavedWait[p->processID];
        return p;
    }
    return NULL;
}

static int getQuantum(Policy policy, PCB* process) {
    if (policy == RR)   return RR_QUANTUM;
    if (policy == HRRN) return 999;
    // MLFQ: quantum = 2^(level it was dequeued from)
    // getMlfqRestoreLevel returns the level the process ran from
    int runLevel = getMlfqRestoreLevel(process->processID);
    return (1 << runLevel);
}

static void handleEndOfQuantum(Policy policy, PCB* process) {
    process->state = READY;
    snprintf(memory[process->memoryLowerBound + 1], MAX_LINE_LENGTH, "state READY");
    if (policy == RR) {
        addToReadyQueue(process);
        printf("  >> P%d quantum expired -- preempted, back to Ready Queue\n",
               process->processID);
    } else if (policy == HRRN) {
        addToReadyQueue(process);
        printf("  >> P%d returned to Ready Queue\n", process->processID);
    } else {
        // MLFQ: demote to next level — use getMLFQNextLevel, NOT mlfqNextLevel table
        int nextLevel = getMLFQNextLevel(process);
        addToMLFQQueue(process, nextLevel);
        printf("  >> P%d quantum expired -- demoted to MLFQ Queue %d (quantum=%d)\n",
               process->processID, nextLevel, 1 << nextLevel);
    }
}

static int countFinished(PCB** all, int count) {
    int n = 0;
    for (int i = 0; i < count; i++)
        if (all[i]->state == FINISHED) n++;
    return n;
}

// --- main ---------------------------------------------------------------------
int main(int argc, char* argv[]) {

    char exePath[1024];
    strncpy(exePath, argv[0], sizeof(exePath) - 1);
    char* exeDir = dirname(exePath);
    if (chdir(exeDir) != 0)
        perror("WARNING: could not chdir to executable directory");

    int choice;
    printf("\n==========================================\n");
    printf("           OS SIMULATION\n");
    printf("==========================================\n");
    printf("\nSelect scheduling algorithm:\n");
    printf("  1. Round Robin (RR)\n");
    printf("  2. Highest Response Ratio Next (HRRN)\n");
    printf("  3. Multi-Level Feedback Queue (MLFQ)\n");
    printf("Enter choice (1/2/3): ");
    fflush(stdout);
    scanf("%d", &choice);
    int ch; while ((ch = getchar()) != '\n' && ch != EOF);

    switch (choice) {
        case 1:  currentPolicy = RR;
                 printf("\n[CONFIG] Round Robin | quantum = %d instruction(s)\n", RR_QUANTUM);
                 break;
        case 2:  currentPolicy = HRRN;
                 printf("\n[CONFIG] HRRN | non-preemptive\n");
                 break;
        case 3:  currentPolicy = MLFQ;
                 printf("\n[CONFIG] MLFQ | %d levels | quanta: ", MLFQ_LEVELS);
                 for (int i = 0; i < MLFQ_LEVELS; i++)
                     printf("Q%d=%d%s", i, 1 << i, i < MLFQ_LEVELS - 1 ? ", " : "\n");
                 break;
        default: currentPolicy = RR;
                 printf("\n[CONFIG] Invalid -- defaulting to Round Robin\n");
                 break;
    }

    initMutexManager();
    readyQueue.count = 0;
    blockQueue.count = 0;
    for (int i = 0; i < MLFQ_LEVELS; i++) mlfqQueues[i].count = 0;
    for (int i = 0; i <= MAX_PROCESSES; i++) {
        mlfqSavedWait[i] = 0;
    }

    PCB* allProcesses[MAX_PROCESSES];
    int  createdCount     = 0;
    int  clock            = 0;
    int  lastPrintedClock = -1;

    printf("\n==========================================\n");
    printf("           SIMULATION START\n");
    printf("==========================================\n");

    while (clock < MAX_CYCLES) {

        if (createdCount == processCount &&
            countFinished(allProcesses, createdCount) == processCount)
            break;

        // All queues empty: print header, check arrivals, wait if still empty
        if (allQueuesEmpty(currentPolicy)) {
            if (clock != lastPrintedClock) {
                printf("\n+--- CLOCK CYCLE %-3d ------------------------------------\n", clock);
                lastPrintedClock = clock;
            }
            checkArrivals(clock, allProcesses, &createdCount, currentPolicy);
            if (!allQueuesEmpty(currentPolicy)) continue;
            printCycleQueues(currentPolicy);
            printf("  Action  | All queues empty -- waiting for next arrival\n");
            syncWaitingToMemory(allProcesses, createdCount);
            printMemory();
            clock++;
            continue;
        }

        // Print clock header once per unique clock value
        if (clock != lastPrintedClock) {
            printf("\n+--- CLOCK CYCLE %-3d ------------------------------------\n", clock);
            lastPrintedClock = clock;
        }
        // Always check arrivals (even when clock hasn't changed — process may have just finished)
        checkArrivals(clock, allProcesses, &createdCount, currentPolicy);
        printCycleQueues(currentPolicy);

        PCB* current = selectProcess(currentPolicy, clock);
        if (current == NULL) {
            printf("  Action  | No process selected\n");
            syncWaitingToMemory(allProcesses, createdCount);
            printMemory();
            clock++;
            continue;
        }

        current->state = RUNNING;
        int quantum    = getQuantum(currentPolicy, current);
        int blocked    = 0;
        int finished   = 0;
        int preempted  = 0;

        for (int q = 0; q < quantum; q++) {

            if (q > 0) {
                if (clock != lastPrintedClock) {
                    printf("\n+--- CLOCK CYCLE %-3d ------------------------------------\n", clock);
                    lastPrintedClock = clock;
                }
                checkArrivals(clock, allProcesses, &createdCount, currentPolicy);
                printCycleQueues(currentPolicy);
            }

            printf("  Running | P%d (pc=%d)  [%s]\n",
                   current->processID,
                   current->programCounter,
                   current->onDisk ? "swapping in from disk..." : "in memory");

            int result = executeInstruction(current, &readyQueue, &blockQueue,
                                            allProcesses, createdCount);

            if (result == 0) {
                // Blocked: attempt cost 1 clock
                clock++;
                // Update waiting times right after clock advances
                for (int i = 0; i < readyQueue.count; i++) {
                    PCB* p = readyQueue.processes[i];
                    p->waitingTime++;
                    mlfqSavedWait[p->processID] = p->waitingTime;
                    if (p->onDisk == 0)
                        snprintf(memory[p->memoryLowerBound + 8], MAX_LINE_LENGTH,
                                 "waiting %d", p->waitingTime);
                }
                for (int lvl = 0; lvl < MLFQ_LEVELS; lvl++) {
                    for (int i = 0; i < mlfqQueues[lvl].count; i++) {
                        PCB* p = mlfqQueues[lvl].processes[i];
                        p->waitingTime++;
                        mlfqSavedWait[p->processID] = p->waitingTime;
                        if (p->onDisk == 0)
                            snprintf(memory[p->memoryLowerBound + 8], MAX_LINE_LENGTH,
                                     "waiting %d", p->waitingTime);
                    }
                }
                for (int i = 0; i < blockQueue.count; i++) {
                    PCB* p = blockQueue.processes[i];
                    p->waitingTime++;
                    mlfqSavedWait[p->processID] = p->waitingTime;
                    if (p->onDisk == 0)
                        snprintf(memory[p->memoryLowerBound + 8], MAX_LINE_LENGTH,
                                 "waiting %d", p->waitingTime);
                }
                blocked = 1;
                printf("  Result  | P%d BLOCKED -- waiting for mutex (clock now %d)\n",
                       current->processID, clock);
                printCycleQueues(currentPolicy);
                break;
            }

            if (result == 2) {
                // Already finished, no clock cost
                printf("  Result  | P%d FINISHED -- all %d instructions done (clock %d)\n",
                       current->processID, current->burstTime, clock);
                finished = 1;
                printCycleQueues(currentPolicy);
                syncWaitingToMemory(allProcesses, createdCount);
            printMemory();
                break;
            }

            // Every instruction costs exactly 1 clock cycle
            clock++;

            // Update waiting times right after clock advances — before any printMemory
            // For RR/HRRN: processes are in readyQueue
            // For MLFQ: processes are in mlfqQueues[] — must update those too
            for (int i = 0; i < readyQueue.count; i++) {
                PCB* p = readyQueue.processes[i];
                p->waitingTime++;
                mlfqSavedWait[p->processID] = p->waitingTime;
                if (p->onDisk == 0)
                    snprintf(memory[p->memoryLowerBound + 8], MAX_LINE_LENGTH,
                             "waiting %d", p->waitingTime);
            }
            for (int lvl = 0; lvl < MLFQ_LEVELS; lvl++) {
                for (int i = 0; i < mlfqQueues[lvl].count; i++) {
                    PCB* p = mlfqQueues[lvl].processes[i];
                    p->waitingTime++;
                    mlfqSavedWait[p->processID] = p->waitingTime;
                    if (p->onDisk == 0)
                        snprintf(memory[p->memoryLowerBound + 8], MAX_LINE_LENGTH,
                                 "waiting %d", p->waitingTime);
                }
            }
            for (int i = 0; i < blockQueue.count; i++) {
                PCB* p = blockQueue.processes[i];
                p->waitingTime++;
                mlfqSavedWait[p->processID] = p->waitingTime;
                if (p->onDisk == 0)
                    snprintf(memory[p->memoryLowerBound + 8], MAX_LINE_LENGTH,
                             "waiting %d", p->waitingTime);
            }

            // Check if this was the last instruction
            int nextMemIndex = current->memoryLowerBound + 10 + current->programCounter;
            if (nextMemIndex > current->instructionEnd || current->state == FINISHED) {
                printf("  Result  | P%d FINISHED -- all %d instructions done (clock %d)\n",
                       current->processID, current->burstTime, clock);
                current->state = FINISHED;
                snprintf(memory[current->memoryLowerBound + 1], MAX_LINE_LENGTH,
                         "state FINISHED");
                freeMemory(current);
                finished = 1;
                printCycleQueues(currentPolicy);
                syncWaitingToMemory(allProcesses, createdCount);
            printMemory();
                break;
            }

            snprintf(memory[current->memoryLowerBound + 1], MAX_LINE_LENGTH, "state RUNNING");

            // MLFQ: after every instruction, check if a higher-priority queue
            // has a process already in memory. If so, preempt immediately.
            // We do NOT preempt for on-disk processes — swapping in to preempt
            // causes unnecessary memory churn.
            if (currentPolicy == MLFQ) {
                int runLevel = getMlfqRestoreLevel(current->processID);
                int higherExists = 0;
                for (int lvl = 0; lvl < runLevel && !higherExists; lvl++) {
                    for (int i = 0; i < mlfqQueues[lvl].count; i++) {
                        if (mlfqQueues[lvl].processes[i]->onDisk == 0) {
                            higherExists = 1;
                            break;
                        }
                    }
                }
                if (higherExists) {
                    addToMLFQQueue(current, runLevel);
                    current->state = READY;
                    snprintf(memory[current->memoryLowerBound + 1], MAX_LINE_LENGTH, "state READY");
                    printf("  >> P%d preempted by higher-priority process -- back to MLFQ Queue %d\n",
                           current->processID, runLevel);
                    preempted = 1;
                    lastPrintedClock = -1;
                    syncWaitingToMemory(allProcesses, createdCount);
                    printMemory();
                    break;
                }
            }

            // Print memory after every clock cycle (mid-quantum)
            if (q < quantum - 1) {
                syncWaitingToMemory(allProcesses, createdCount);
            printMemory();
            }
        }

        if (preempted) {
            // Already re-queued at same level — no demotion, no end-of-cycle print
            // lastPrintedClock already reset inside the preemption block
        } else if (!blocked && !finished) {
            handleEndOfQuantum(currentPolicy, current);
            lastPrintedClock = -1;
        } else if (blocked) {
            lastPrintedClock = -1;
        } else if (finished) {
            lastPrintedClock = -1;
        }

        if (!preempted && !finished) {
            if (clock != lastPrintedClock || blocked) {
                printf("+--- End of cycle %-3d ", clock);
                printCycleQueues(currentPolicy);
            }
            syncWaitingToMemory(allProcesses, createdCount);
            printMemory();
            lastPrintedClock = -1;
        }
    }

    if (clock >= MAX_CYCLES)
        printf("\n[!] Simulation halted -- exceeded %d cycles.\n", MAX_CYCLES);

    printf("\n==========================================\n");
    printf("           SIMULATION COMPLETE\n");
    printf("  Processes finished : %d / %d\n",
           countFinished(allProcesses, createdCount), processCount);
    printf("  Total clock cycles : %d\n", clock);
    printf("==========================================\n\n");

    for (int i = 0; i < createdCount; i++) free(allProcesses[i]);
    return 0;
}