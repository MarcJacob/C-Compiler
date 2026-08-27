// Single file test.
// Hello world program.

extern int extern_func(int a);

//float cast_test = (float)5;
int sizeof_test = sizeof(int****);

typedef int* (*int_func_ptr_array)(float)[10];
typedef int (*int_func_ptr)(float, float);

int arr[5][10];

float* cast_test = (float*)2;
int_func_ptr funcPtr = (int_func_ptr)4;

struct struct_test
{
	int member_a;
	float member_b;
	int_func_ptr member_c;

	enum nested_enum_test
	{
		MEMBER_A,
		MEMBER_B
	} enumerated_member;
};

union union_test
{
	int member_a;
	float member_b;
	int_func_ptr member_c;
};

enum enum_test
{
	MEMBER_A,
	MEMBER_B,
	MEMBER_C = 44,
};

int main(int argc, char** argv)
{
	int ternary_test = ((argc > 0) ? 1 : 0) + 1;
	int a = 4, b = 2, c = 2 + sizeof(argc);


	if (foo((float)a, b) + (&a + 1)[a + 5] + 3 * 4 + (2 - 1) && d->x == 2 && sizeof(int(*)(float)) > 4)
	{
		printf("Hello, World !\n");
	}

	return 0;
}