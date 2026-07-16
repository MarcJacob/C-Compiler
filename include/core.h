// Core typedefs, function definitions and macros used everywhere in the project.

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
typedef int i32;

// Functions
