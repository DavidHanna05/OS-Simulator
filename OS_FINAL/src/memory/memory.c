#include "memory.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char memory[MEMORY_SIZE][MAX_LINE_LENGTH];

int allocateMemory(int instCount) {
    int memNeeded = 10 + instCount + 3;
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (memory[i][0] == '\0') {
            int contiguous = 1;
            for (int j = i + 1; j < i + memNeeded && j < MEMORY_SIZE; j++) {
                if (memory[j][0] == '\0') contiguous++;
                else break;
            }
            if (contiguous >= memNeeded) return i;
        }
    }
    return -1;
}

void freeMemory(PCB* process) {
    for (int i = process->memoryLowerBound; i <= process->memoryUpperBound; i++)
        memory[i][0] = '\0';
    printf("  [MEM] P%d freed mem[%d-%d]\n",
           process->processID,
           process->memoryLowerBound,
           process->memoryUpperBound);
}

void swapToDisk(PCB* process) {
    char path[256];
    snprintf(path, sizeof(path), "%sprocess%d.txt", DISK_DIR, process->processID);

    process->state  = READY;
    process->onDisk = 1;
    snprintf(memory[process->memoryLowerBound + 1], MAX_LINE_LENGTH, "state READY");
    snprintf(memory[process->memoryLowerBound + 9], MAX_LINE_LENGTH, "onDisk 1");

    FILE* f = fopen(path, "w");
    if (f == NULL) {
        printf("  [ERROR] Could not write P%d to disk\n", process->processID);
        return;
    }
    for (int i = process->memoryLowerBound; i <= process->memoryUpperBound; i++)
        fprintf(f, "%s\n", memory[i]);
    fclose(f);

    printf("  [DISK] P%d swapped to disk (%s)\n", process->processID, path);
    freeMemory(process);
}

void swapFromDisk(PCB* process, PCB** allProcesses, int processCount) {
    int oldLowerBound = process->memoryLowerBound;
    int instCount     = process->instructionEnd - oldLowerBound - 9;
    int memNeeded     = 10 + instCount + 3;

    int lowerBound = allocateMemory(instCount);

    if (lowerBound == -1) {
        PCB* victim = NULL;
        for (int i = 0; i < processCount; i++) {
            PCB* p = allProcesses[i];
            if (p->onDisk == 0 && p->state != RUNNING && p->state != FINISHED
                && p->processID != process->processID) {
                if (victim == NULL || p->arrivalTime < victim->arrivalTime)
                    victim = p;
            }
        }
        if (victim == NULL) {
            printf("  [ERROR] No victim available to swap in P%d\n", process->processID);
            return;
        }
        printf("  [MEM] No space for P%d — swapping P%d to disk\n",
               process->processID, victim->processID);
        swapToDisk(victim);
        lowerBound = allocateMemory(instCount);
        if (lowerBound == -1) {
            printf("  [ERROR] Still not enough memory for P%d\n", process->processID);
            return;
        }
    }

    int upperBound = lowerBound + memNeeded - 1;
    int instrEnd   = lowerBound + 10 + instCount - 1;

    for (int i = lowerBound; i <= upperBound; i++)
        memory[i][0] = '\0';

    process->memoryLowerBound = lowerBound;
    process->memoryUpperBound = upperBound;
    process->instructionEnd   = instrEnd;
    process->onDisk           = 0;

    char path[256];
    snprintf(path, sizeof(path), "%sprocess%d.txt", DISK_DIR, process->processID);

    FILE* f = fopen(path, "r");
    if (f == NULL) {
        printf("  [ERROR] Could not read P%d from disk\n", process->processID);
        return;
    }

    char line[MAX_LINE_LENGTH];
    int memIndex = lowerBound;
    while (fgets(line, MAX_LINE_LENGTH, f) != NULL && memIndex <= upperBound) {
        line[strcspn(line, "\r\n")] = '\0';
        strncpy(memory[memIndex], line, MAX_LINE_LENGTH);
        memIndex++;
    }
    fclose(f);

    // Delete the disk file now that the process is back in memory
    remove(path);

    while (memIndex <= upperBound)
        memory[memIndex++][0] = '\0';

    snprintf(memory[lowerBound + 0], MAX_LINE_LENGTH, "pid %d",      process->processID);
    snprintf(memory[lowerBound + 1], MAX_LINE_LENGTH, "state READY");
    snprintf(memory[lowerBound + 2], MAX_LINE_LENGTH, "pc %d",       process->programCounter);
    snprintf(memory[lowerBound + 3], MAX_LINE_LENGTH, "lower %d",    lowerBound);
    snprintf(memory[lowerBound + 4], MAX_LINE_LENGTH, "upper %d",    upperBound);
    snprintf(memory[lowerBound + 5], MAX_LINE_LENGTH, "instrEnd %d", instrEnd);
    snprintf(memory[lowerBound + 9], MAX_LINE_LENGTH, "onDisk 0");

    printf("  [DISK] P%d swapped in to mem[%d-%d]\n",
           process->processID, lowerBound, upperBound);
}

void printMemory() {
    printf("\n========== MEMORY STATE ==========\n");
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (memory[i][0] != '\0') printf("  [%2d] %s\n", i, memory[i]);
        else                       printf("  [%2d] ---\n", i);
    }
    printf("==================================\n\n");
}