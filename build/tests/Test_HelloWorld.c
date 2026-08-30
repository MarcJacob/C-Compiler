// Single file test.
// Hello world program.

int TestPrimitiveVal = 5;

int* TestPrimitivePointer = &TestPrimitiveVal;

int TestPrimitiveArray[5] = { 0, 1, 2, 3, 4 };
int TestPrimitiveMultiArray[4][5][6];

union TestStructure
{
	int a;
	float b;
	double c;
};

struct Test2
{
	union TestStructure A;
	int B;
};



int main(int argc, char** argv)
{
	printf("Hello, World !\n");

	return 0;
}