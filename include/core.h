// Core typedefs, function definitions and macros used everywhere in the project.

#ifndef CORE_INCLUDED
#define CORE_INCLUDED

// Standard I/O lib and string functions are used everywhere in the project.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Macros

// Simple assertion system with the possibility of passing a custom message.
#if _DEBUG
#define ASSERT_MSG(x, FailedMsgFormat, ...) if (!(x)) { printf("ASSERTION FAILED: (" #x "). " FailedMsgFormat, __VA_ARGS__); abort(); }
#define ASSERT(x) if (!(x)) { printf("ASSERTION FAILED: (" #x ")."); abort(); }
#else
#define ASSERT(x) // Empty macro.
#endif // _DEBUG

// Types

typedef unsigned long long ui64;
typedef unsigned int ui32;
typedef unsigned short ui16;
typedef unsigned char ui8;

typedef long long i64;
typedef int i32;
typedef short i16;
typedef char i8;

// Functions

#endif // CORE_INCLUDED
