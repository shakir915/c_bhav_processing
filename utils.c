#include "utils.h"
#include <stdio.h> // For printf

clock_t lastTime; // Definition of the global variable

void printTimeSpent(const char *tag) {
    if (tag != NULL && *tag != '\0') {
        double time_spent = (double)(clock() - lastTime) / CLOCKS_PER_SEC;
        printf("🕒 %s: %.3f seconds\n", tag, time_spent);
    }
    lastTime = clock();
}