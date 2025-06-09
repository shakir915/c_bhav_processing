#ifndef UTILS_H
#define UTILS_H

#include <time.h>
#include <stdbool.h>

#define LOG_ENABLED false // Set to true for detailed logging

extern clock_t lastTime; // Make lastTime accessible by main.c for initialization

void printTimeSpent(const char *tag);

#endif // UTILS_H