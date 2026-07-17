// Core typedefs, function definitions and macros used everywhere in the project.

// Standard I/O lib and string functions are used everywhere in the project.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Macros

// Simple assertion system with the possibility of passing a custom message.
#if _DEBUG
#define ASSERT_MSG(x, FailedMsgFormat, ...) if (!(x)) { printf("ASSERTION FAILED: (" #x "). " FailedMsgFormat, __VA_ARGS__); abort(); return; }
#define ASSERT(x) if (!(x)) { printf("ASSERTION FAILED: (" #x ")."); abort(); return; }
#else
#define ASSERT(x) // Empty macro.
#endif // _DEBUG

// Types

typedef unsigned long long ui64;
typedef unsigned int ui32;
typedef unsigned short ui16;
typedef unsigned char ui8;

typedef int i32;

// Functions
