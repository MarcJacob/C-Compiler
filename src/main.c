#include "core.h"
#include "string_ansi.h"
#include "vector.h"

int main(int argc, char** argv)
{
	printf("Hello, world !\n");

	struct CharBuffer_ANSI TestBuffer = LoadFileToBuffer_ANSI("../tests/Test_HelloWorld.c");

	struct Vector IntVec = Vector_Create(int, 12);
	Vector_Push(IntVec, int, 1);
	Vector_Push(IntVec, int, 2);

	printf("Value of IntVec[2] = %d\n", Vector_GetValueAt(IntVec, int, 2));

	return 0;
}
