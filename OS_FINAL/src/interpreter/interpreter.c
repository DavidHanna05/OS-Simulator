#include "interpreter.h"
#include "../systemcall/system_call.h"
#include "../memory/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char memory[MEMORY_SIZE][MAX_LINE_LENGTH];

static int mapInstruction(const char* word) {
    if (strcmp(word, "print")       == 0) return PRINT;
    if (strcmp(word, "assign")      == 0) return ASSIGN;
    if (strcmp(word, "writeFile")   == 0) return WRITE_FILE;
    if (strcmp(word, "readFile")    == 0) return READ_FILE;
    if (strcmp(word, "printFromTo") == 0) return PRINT_FROM_TO;
    if (strcmp(word, "semWait")     == 0) return SEM_WAIT;
    if (strcmp(word, "semSignal")   == 0) return SEM_SIGNAL;
    return UNKNOWN;
}

static void resolveArgument(PCB* process, const char* arg, char* outBuf, int bufSize) {
    if (strcmp(arg, "input") == 0) {
        sys_userInput(outBuf, bufSize);
        return;
    }
    if (strncmp(arg, "readFile", 8) == 0) {
        const char* fname    = arg + 9;
        const char* resolved = sys_readMemory(process, fname);
        if (resolved == NULL) resolved = fname;
        sys_readFile(resolved, outBuf, bufSize);
        return;
    }
    const char* val = sys_readMemory(process, arg);
    if (val != NULL) { strncpy(outBuf, val, bufSize); return; }
    strncpy(outBuf, arg, bufSize);
}

// ─── countInstructions ───────────────────────────────────────────────────────
// Counts non-empty lines — this IS the burst time, never passed in manually.
int countInstructions(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (f == NULL) { printf("  [ERROR] Cannot open \"%s\"\n", filename); return -1; }
    int count = 0;
    char line[MAX_LINE_LENGTH];
    while (fgets(line, MAX_LINE_LENGTH, f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) > 0) count++;
    }
    fclose(f);
    return count;
}

// ─── loadInstructions ────────────────────────────────────────────────────────
// lb+0 pid  lb+1 state  lb+2 pc  lb+3 lower  lb+4 upper  lb+5 instrEnd
// lb+6 arrival  lb+7 burst  lb+8 waiting  lb+9 onDisk
// lb+10..instrEnd = instructions    ub-2..ub = var1 var2 var3
void loadInstructions(PCB* process, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (f == NULL) { printf("  [ERROR] Cannot open \"%s\"\n", filename); return; }

    snprintf(memory[process->memoryLowerBound + 0], MAX_LINE_LENGTH, "pid %d",      process->processID);
    snprintf(memory[process->memoryLowerBound + 1], MAX_LINE_LENGTH, "state %s",    getStateName(process->state));
    snprintf(memory[process->memoryLowerBound + 2], MAX_LINE_LENGTH, "pc %d",       process->programCounter);
    snprintf(memory[process->memoryLowerBound + 3], MAX_LINE_LENGTH, "lower %d",    process->memoryLowerBound);
    snprintf(memory[process->memoryLowerBound + 4], MAX_LINE_LENGTH, "upper %d",    process->memoryUpperBound);
    snprintf(memory[process->memoryLowerBound + 5], MAX_LINE_LENGTH, "instrEnd %d", process->instructionEnd);
    snprintf(memory[process->memoryLowerBound + 6], MAX_LINE_LENGTH, "arrival %d",  process->arrivalTime);
    snprintf(memory[process->memoryLowerBound + 7], MAX_LINE_LENGTH, "burst %d",    process->burstTime);
    snprintf(memory[process->memoryLowerBound + 8], MAX_LINE_LENGTH, "waiting %d",  process->waitingTime);
    snprintf(memory[process->memoryLowerBound + 9], MAX_LINE_LENGTH, "onDisk %d",   process->onDisk);

    int memIndex = process->memoryLowerBound + 10;
    char line[MAX_LINE_LENGTH];
    while (fgets(line, MAX_LINE_LENGTH, f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;
        strncpy(memory[memIndex++], line, MAX_LINE_LENGTH);
    }
    fclose(f);

    snprintf(memory[process->memoryUpperBound - 2], MAX_LINE_LENGTH, "var1 ");
    snprintf(memory[process->memoryUpperBound - 1], MAX_LINE_LENGTH, "var2 ");
    snprintf(memory[process->memoryUpperBound    ], MAX_LINE_LENGTH, "var3 ");
}

// ─── createProcess ───────────────────────────────────────────────────────────
// Burst time = countInstructions(file). Swaps victims if memory is full.
PCB* createProcess(int processID, int arrivalTime,
                   const char* filename, PCB** allProcesses, int processCount) {

    int instCount = countInstructions(filename);
    if (instCount <= 0) {
        printf("  [ERROR] No instructions found for P%d in \"%s\"\n", processID, filename);
        return NULL;
    }
    int burstTime  = instCount;
    int lowerBound = allocateMemory(instCount);

    while (lowerBound == -1) {
        PCB* victim = NULL;
        for (int i = 0; i < processCount; i++) {
            PCB* p = allProcesses[i];
            if (p->onDisk == 0 && p->state != FINISHED)
                if (victim == NULL || p->arrivalTime < victim->arrivalTime)
                    victim = p;
        }
        if (victim == NULL) {
            printf("  [ERROR] No victim available to free memory for P%d\n", processID);
            return NULL;
        }
        printf("  [MEM] No space for P%d — swapping P%d to disk\n",
               processID, victim->processID);
        swapToDisk(victim);
        lowerBound = allocateMemory(instCount);
    }

    int upperBound     = lowerBound + 10 + instCount + 3 - 1;
    int instructionEnd = lowerBound + 10 + instCount - 1;

    PCB* process = (PCB*)malloc(sizeof(PCB));
    *process = createPCB(processID, arrivalTime, burstTime,
                         lowerBound, upperBound, instructionEnd);

    loadInstructions(process, filename);

    printf("  [NEW] P%d created | burst=%d | mem[%d-%d] | instrs mem[%d-%d] | vars mem[%d-%d]\n",
           processID, burstTime,
           lowerBound, upperBound,
           lowerBound + 10, instructionEnd,
           upperBound - 2, upperBound);

    return process;
}

// ─── executeInstruction ──────────────────────────────────────────────────────
// Returns: 1 = executed  0 = blocked (PC advanced, main charges 1 clock)
//          2 = already finished, no clock cost
int executeInstruction(PCB* process, Queue* readyQueue, Queue* blockedQueue,
                       PCB** allProcesses, int processCount) {

    if (process->onDisk == 1) {
        printf("  [DISK] P%d is on disk — swapping in\n", process->processID);
        swapFromDisk(process, allProcesses, processCount);
    }

    int memIndex = process->memoryLowerBound + 10 + process->programCounter;
    if (memIndex > process->instructionEnd) {
        printf("  [DONE] P%d completed all %d instructions\n",
               process->processID, process->burstTime);
        process->state = FINISHED;
        snprintf(memory[process->memoryLowerBound + 1], MAX_LINE_LENGTH,
                 "state %s", getStateName(process->state));
        freeMemory(process);
        return 2;
    }

    char line[MAX_LINE_LENGTH];
    strncpy(line, memory[memIndex], MAX_LINE_LENGTH);

    printf("  [EXEC] P%d | instr %d/%d | %s\n",
           process->processID,
           process->programCounter + 1,
           process->burstTime,
           line);

    char* instrWord = strtok(line, " ");
    char* arg1      = strtok(NULL, " ");
    char* arg2      = strtok(NULL, " ");
    char* arg3      = strtok(NULL, " ");

    char combinedArg2[MAX_LINE_LENGTH];
    if (arg2 != NULL && strcmp(arg2, "readFile") == 0 && arg3 != NULL) {
        snprintf(combinedArg2, MAX_LINE_LENGTH, "readFile %s", arg3);
        arg2 = combinedArg2;
    }

    int instrType = mapInstruction(instrWord);

    char resolvedArg1[MAX_LINE_LENGTH] = "";
    char resolvedArg2[MAX_LINE_LENGTH] = "";
    if (arg1 != NULL) resolveArgument(process, arg1, resolvedArg1, MAX_LINE_LENGTH);
    if (arg2 != NULL) resolveArgument(process, arg2, resolvedArg2, MAX_LINE_LENGTH);

    switch (instrType) {

        case PRINT:
            printf("  [OUT]  %s\n", resolvedArg1);
            break;

        case ASSIGN:
            if (strncmp(resolvedArg2, "[file not found]", 16) == 0) {
                printf("  [ERROR] P%d: cannot assign \"%s\" — file not found, skipping\n",
                       process->processID, arg1);
            } else {
                sys_writeMemory(process, arg1, resolvedArg2);
                printf("  [VAR]  P%d stored %s = %s\n", process->processID, arg1, resolvedArg2);
            }
            break;

        case WRITE_FILE:
            sys_writeFile(resolvedArg1, resolvedArg2);
            printf("  [FILE] Wrote \"%s\" to file \"%s\"\n", resolvedArg2, resolvedArg1);
            break;

        case READ_FILE: {
            char buf[MAX_LINE_LENGTH];
            sys_readFile(resolvedArg1, buf, MAX_LINE_LENGTH);
            printf("  [FILE] Read \"%s\" from file \"%s\"\n", buf, resolvedArg1);
            break;
        }

        case PRINT_FROM_TO:
            printf("  [OUT]  P%d printing %s to %s:\n",
                   process->processID, resolvedArg1, resolvedArg2);
            sys_printFromTo(atoi(resolvedArg1), atoi(resolvedArg2));
            break;

        case SEM_WAIT: {
            Mutex* mutex = getMutex(arg1);
            if (mutex == NULL) { printf("  [ERROR] Unknown mutex \"%s\"\n", arg1); break; }
            int acquired = sem_wait(mutex, process, readyQueue, blockedQueue);
            if (!acquired) {
                printf("  [SYNC] P%d blocked — \"%s\" held by P%d\n",
                       process->processID, arg1, mutex->owner->processID);
                process->programCounter++;
                snprintf(memory[process->memoryLowerBound + 2], MAX_LINE_LENGTH,
                         "pc %d", process->programCounter);
                snprintf(memory[process->memoryLowerBound + 1], MAX_LINE_LENGTH,
                         "state %s", getStateName(process->state));
                return 0;
            }
            printf("  [SYNC] P%d acquired mutex \"%s\"\n", process->processID, arg1);
            break;
        }

        case SEM_SIGNAL: {
            Mutex* mutex = getMutex(arg1);
            if (mutex == NULL) { printf("  [ERROR] Unknown mutex \"%s\"\n", arg1); break; }
            sem_signal(mutex, readyQueue, blockedQueue);
            printf("  [SYNC] P%d released mutex \"%s\"\n", process->processID, arg1);
            break;
        }

        case UNKNOWN:
        default:
            printf("  [ERROR] Unknown instruction \"%s\" in P%d\n",
                   instrWord, process->processID);
            break;
    }

    process->programCounter++;
    snprintf(memory[process->memoryLowerBound + 2], MAX_LINE_LENGTH,
             "pc %d", process->programCounter);
    return 1;
}