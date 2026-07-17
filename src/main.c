#include "core.h"
#include "string_ansi.h"
#include "vector.h"

// Include compiler implementation.
#include "compiler.c"

int main(int argc, char** argv)
{
	printf("Hello, world !\n");

	struct CharBuffer_ANSI TestBuffer = LoadFileToBuffer_ANSI("../tests/Test_HelloWorld.c");

	return 0;
}
