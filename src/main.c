#include "core.h"
#include "string_ansi.h"

int main(int argc, char** argv)
{
	printf("Hello, world !\n");

	struct CharBuffer_ANSI TestBuffer = LoadFileToBuffer_ANSI("../tests/Test_HelloWorld.c");

	return 0;
}
