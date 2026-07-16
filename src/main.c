#include "core.h"
#include "string_ansi.h"

int main(int argc, char** argv)
{
	printf("Hello, world !\n");

	struct CharBuffer_ANSI TestBuffer = LoadFileToBuffer_ANSI("../tests/Test_HelloWorld.c");
	struct CharBufferReader_ANSI TestBufferReader = CreateBufferReader_ANSI(&TestBuffer);

	printf("Test Buffer starts with '//' = %d", CharBufferReader_ReadNextExpected(&TestBufferReader, "//"));
	return 0;
}
