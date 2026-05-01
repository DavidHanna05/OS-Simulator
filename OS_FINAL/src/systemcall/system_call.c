#include "system_call.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DISK_DIR "disk/"

extern char memory[MEMORY_SIZE][MAX_LINE_LENGTH];

static int isEmptySlot(const char* slot) {
    return strlen(slot) == 0 ||
           strcmp(slot, "var1 ") == 0 ||
           strcmp(slot, "var2 ") == 0 ||
           strcmp(slot, "var3 ") == 0;
}

// 1. Read first line of disk/<filename> into outBuf
void sys_readFile(const char* filename, char* outBuf, int bufSize) {
    char path[256];
    snprintf(path, sizeof(path), "%s%s", DISK_DIR, filename);
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        printf("  [ERROR] File \"%s\" not found on disk — skipping read\n", filename);
        fflush(stdout);
        strncpy(outBuf, "[file not found]", bufSize);
        return;
    }
    if (fgets(outBuf, bufSize, f) != NULL) {
        outBuf[strcspn(outBuf, "\n")] = '\0';
    } else {
        strncpy(outBuf, "[empty file]", bufSize);
    }
    fclose(f);
}

// 2. Create/overwrite disk/<filename> with data
void sys_writeFile(const char* filename, const char* data) {
    char path[256];
    snprintf(path, sizeof(path), "%s%s", DISK_DIR, filename);
    FILE* f = fopen(path, "w");
    if (f == NULL) {
        return;
    }
    fprintf(f, "%s\n", data);
    fclose(f);
}

// 3. Print data to the screen
void sys_print(const char* data) {
    printf("         %s\n", data);
}

// 4. Prompt user and read input from keyboard
void sys_userInput(char* outBuf, int bufSize) {
    printf("  Enter value: ");
    fflush(stdout);
    if (fgets(outBuf, bufSize, stdin) != NULL) {
        outBuf[strcspn(outBuf, "\n")] = '\0';
        outBuf[strcspn(outBuf, "\r")] = '\0';
    } else {
        strncpy(outBuf, "", bufSize);
    }
}

// 5. Find variable in process memory
const char* sys_readMemory(PCB* process, const char* varName) {
    int varStart = process->memoryUpperBound - 2;
    for (int i = varStart; i <= process->memoryUpperBound; i++) {
        if (isEmptySlot(memory[i])) continue;
        char temp[MAX_LINE_LENGTH];
        strncpy(temp, memory[i], MAX_LINE_LENGTH);
        char* storedName  = strtok(temp, " ");
        char* storedValue = strtok(NULL, " ");
        if (storedName != NULL && storedValue != NULL) {
            if (strcmp(storedName, varName) == 0) {
                return memory[i] + strlen(varName) + 1;
            }
        }
    }
    return NULL;
}

// 6. Store variable in process memory
void sys_writeMemory(PCB* process, const char* varName, const char* value) {
    int varStart = process->memoryUpperBound - 2;
    for (int i = varStart; i <= process->memoryUpperBound; i++) {
        char temp[MAX_LINE_LENGTH];
        strncpy(temp, memory[i], MAX_LINE_LENGTH);
        char* storedName = strtok(temp, " ");
        if (storedName != NULL && strcmp(storedName, varName) == 0) {
            snprintf(memory[i], MAX_LINE_LENGTH, "%s %s", varName, value);
            return;
        }
        if (isEmptySlot(memory[i])) {
            snprintf(memory[i], MAX_LINE_LENGTH, "%s %s", varName, value);
            return;
        }
    }
}

// 7. Print all integers from start to end inclusive
void sys_printFromTo(int start, int end) {
    for (int i = start; i <= end; i++) {
        printf("         %d\n", i);
    }
}