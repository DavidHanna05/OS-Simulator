#ifndef MUTEX_MANAGER_H
#define MUTEX_MANAGER_H

#include "mutex.h"

typedef struct {
    Mutex userOutput; // controls the screen — only one process can print at a time
    Mutex userInput; // controls the keyboard — only one process can read from stdin at a time
    Mutex file; // controls file read/write — only one process can read/write files at a time
} MutexManager;

// The one global instance — defined in mutex_manager.c
// Declared here so any file that includes this header can use it.
extern MutexManager mutexManager;

void initMutexManager(void); // initialises all 3 mutexes to their free state. Call this once at OS startup before any process runs.

Mutex* getMutex(const char* name); // looks up a mutex by name string and returns a pointer to it, or NULL if the name is unrecognised.


void printAllMutexes(void); // prints the state of all 3 mutexes in a human-readable format.

#endif