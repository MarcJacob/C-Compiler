#include "core.h"
#include "string_ansi.h"
#include "vector.h"

// Include compiler implementation.
#include "compiler.c"

int main(int argc, char** argv)
{
	printf("Hello, world !\n");

	struct CharBuffer_ANSI SourceCodeBuffer = LoadFileToBuffer_ANSI("../tests/Test_HelloWorld.c");

	struct CompilerProcess Compiler = { 0 };
	Compiler.InputFiles = Vector_Create(struct CharBuffer_ANSI*, 0);
	Vector_Push(Compiler.InputFiles, struct CharBuffer_ANSI*, &SourceCodeBuffer);

	Compiler_Run(&Compiler);

	// Catch and log error if any.
	if (Compiler.ErrorCode_Global != COMPILER_SUCCESS)
	{
		printf("Compilation failed.\n\tError: %s\n", Compiler.ErrorMsg);
		return Compiler.ErrorCode_Global;
	}

	return COMPILER_SUCCESS;
}
