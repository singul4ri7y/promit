/**
 * This header file contains reference of highly edited version of NetBSD's getline and getdelim
 * utility.
 * 
 * This utility should not be copied from the source code of Promit, as the source code is highly
 * modified just for Promit Project. I recommend using the source given below.
 *
 * Implementations can be found on: https://github.com/lattera/freebsd/blob/master/contrib/file/getline.c
 */

// Only for non-POSIX operating system such as Windows.


#ifndef _promit_utilites_
#define _promit_utilites_

#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "memory.h"

extern ssize_t getdelim(char**, size_t*, int, FILE*);
extern ssize_t getline(char**, size_t*, FILE*);

// This function is highly edited version of CS50's get_string function.
// Takes a string as input from Promit standard stdin.

// Implementation can be found on: https://github.com/cs50/libcs50/blob/main/src/cs50.c

// No prompting.

extern char* get_string();

// String to double.

extern double pstrtod(const char*);

#ifdef _WIN32
#include <conio.h>
#include <Windows.h>

// Minimal implementation of gettimeofday for Non-POSIX OS such as Windows.

extern int gettimeofday(struct timeval* tp, struct timeval* tzp);

#elif defined(__unix__)

#include <termios.h>
#include <unistd.h>
#include <sys/time.h>

// Minimalistic implementation of getch intented for POSIX like OS such as Linux.

void getch();

#endif

#endif
