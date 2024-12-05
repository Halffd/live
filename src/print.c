#include "print.h"
#include <stdarg.h>
#include <stdlib.h>

#ifdef LOGFILE
FILE *logFile;

// Function to initialize the log file
void initLogFile() {
    logFile = fopen(LOGFILE, "a");
    if (!logFile) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
}

// Function to close the log file
void closeLogFile() {
    if (logFile) {
        fclose(logFile);
    }
}

// Function to log messages to the log file
void logToFile(const char *format, ...) {
    if (logFile) {
        va_list args;
        va_start(args, format);
        vfprintf(logFile, format, args);
        va_end(args);
    }
}
#endif

void prints(const char *format, const char *end, const char *delim, ...) {
    va_list args;
    va_start(args, delim);
    
    int first = 1; // To manage delimiters

    while (*format) {
        if (!first) {
            printf("%s", delim); // Print delimiter before subsequent arguments
            logToFile("%s", delim); // Log delimiter
        }

        switch (*format) {
            case 's': { // String
                const char *str = va_arg(args, const char *);
                printf("%s", str);
                logToFile("%s", str); // Log string
                break;
            }
            case 'd': { // Integer
                int i = va_arg(args, int);
                printf("%d", i);
                logToFile("%d", i); // Log integer
                break;
            }
            case 'f': { // Float
                double f = va_arg(args, double);
                printf("%f", f);
                logToFile("%f", f); // Log float
                break;
            }
            case 'c': { // Character
                int c = va_arg(args, int); // char is promoted to int
                printf("%c", c);
                logToFile("%c", c); // Log character
                break;
            }
            default:
                printf("Unknown format specifier: %c", *format);
                logToFile("Unknown format specifier: %c", *format); // Log error
                break;
        }
        
        first = 0; // After the first argument
        format++; // Move to the next format specifier
    }

    printf("%s", end); // Print end string
    logToFile("%s", end); // Log end string
    va_end(args);
}

// Simple printing without delimiters
void print(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    prints(format, "\n", "", args); // Call prints with newline as end
    va_end(args);
}

// Shortcut functions for printing single arguments of various types
void print1s(const char *str) {
    prints("s", "\n", "", str);
}

void print1d(int i) {
    prints("d", "\n", "", i);
}

void print1f(double f) {
    prints("f", "\n", "", f);
}

void print1c(char c) {
    prints("c", "\n", "", c);
}