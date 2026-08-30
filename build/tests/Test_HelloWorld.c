// Single file test.
// Hello world program.

int TestPrimitiveVal = 5;

int* TestPrimitivePointer = &TestPrimitiveVal;

int TestPrimitiveArray[5] = { 0, 1, 2, 3, 4 };
int TestPrimitiveMultiArray[4][5][6];

struct TestStructure
{
	int a : 1;
	int b : 2;
	int c : 3;
	int d : 4;
	int e, f;
};

int main(int argc, char** argv)
{
	printf("Hello, World !\n");

	return 0;
}