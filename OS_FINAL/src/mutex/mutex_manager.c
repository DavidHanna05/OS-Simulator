#include "mutex_manager.h"
#include <stdio.h>
#include <string.h>

// The single global instance of the mutex manager.
// Every file that includes mutex_manager.h can access this.
MutexManager mutexManager;

void initMutexManager(void) { // initialises all 3 mutexes to their free state. Call this once at OS startup before any process runs.
    initMutex(&mutexManager.userOutput);
    initMutex(&mutexManager.userInput);
    initMutex(&mutexManager.file);
    printf("All 3 mutexes initialised (userOutput, userInput, file).\n");
}

/* 
getMutex:
Translates the resource name string from an instruction into a pointer to the actual Mutex struct.

Why a function instead of accessing mutexManager.userOutput directly?
Because the interpreter receives the resource name as a raw string (e.g. "userOutput") and needs a clean way to get the right mutex
without a chain of if/else spread across multiple files. */

Mutex* getMutex(const char* name) { 
    if (strcmp(name, "userOutput") == 0) return &mutexManager.userOutput;
    if (strcmp(name, "userInput")  == 0) return &mutexManager.userInput;
    if (strcmp(name, "file")       == 0) return &mutexManager.file;

    printf("ERROR: Unknown resource name \"%s\".\n", name);
    return NULL;
}


void printAllMutexes(void) { // prints the state of all 3 mutexes in a human-readable format.
    printMutex("userOutput", &mutexManager.userOutput);
    printMutex("userInput",  &mutexManager.userInput);
    printMutex("file",       &mutexManager.file);
}