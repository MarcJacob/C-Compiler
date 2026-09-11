// Single file test.
// Hello world program.

int TestPrimitiveVal = 5;

int* TestPrimitivePointer = &TestPrimitiveVal;

int TestPrimitiveArray[5] = { 0, 1, 2, 3, 4 };
int TestPrimitiveMultiArray[4][5][6];

struct Test2* ForwardDeclarationTest;

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

enum TestEnum
{
	VAL_A,
	VAL_B,
	VAL_C = 55,
	VAL_D,
};

int a, a, a, a, a;
int a = VAL_A;

int test_arr[VAL_C];

typedef TestEnum TypedefTest;

extern int extern_func_test(int, float, double);

int main(int argc, char** argv)
{
	int test_local_var = 8;

	TypedefTest Typedefed_Var;

	struct struct_in_func
	{
		int foo;
	} bar;

	{
		int test_local_var_subscope = 2;

		int mega_deep_func(double);
	}

	int test(int foo, int bar);

	printf("Hello, World !\n");

	test_local_var++;
	bar.foo = test_local_var;

	return 0;
}