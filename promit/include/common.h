#ifndef _promit_common_
#define _promit_common_

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION

#if defined(__GNUC__) || defined(__CLANG__)
#define __maybe_unused __attribute__((unused))
#define __fallthrough  __attribute__((fallthrough))
#define __unreachable  __builtin_unreachable()
#endif

#ifdef __MACH__
#warning "MacOS support is still unstable."
#endif  // __MACH__

#endif
