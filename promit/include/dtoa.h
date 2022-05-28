/**
 * Header file for dtoa.c generated for Project Promit.
 * 
 * This header file can be used in other projects to use 
 * double_to_ascii functionalities.
 */

#ifndef _promit_dtoa_
#define _promit_dtoa_

#pragma once

// Uncomment it if you need to use strtod function. Though it should
// be available through C standard header files.

// double strtod(const char* s00, char** se);

// dtoa

char* dtoa(double d, int mode, int ndigits,
            int* decept, int* sign, char** rve);

void freedtoa(char* s);

#define DTOA_SHORTEST 0

#endif