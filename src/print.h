#ifndef PRINT_H
#define PRINT_H

#include <stdio.h>

// Define LOGFILE to specify the path for the log file
// Uncomment the following line and specify the path to enable logging
#define LOGFILE "log.txt"

// Function prototypes
void initLogFile();
void closeLogFile();
void logToFile(const char *format, ...);
void prints(const char *format, const char *end, const char *delim, ...);
void print(const char *format, ...);
void print1s(const char *str);
void print1d(int i);
void print1f(double f);
void print1c(char c);

#endif // PRINT_H