// Single file test.
// Hello world program.

int* test;


int foo(int, int);
int foo(int a, int b)
{
	return a + b;
}

struct TestType
{
	int x, y;

	struct
	{
		int anonymous_data;
	};

	struct TestType_Nested
	{
		int nested_data;
	} Test;
} *d;

int *a, b;

int main(int argc, char** argv)
{
	int a = 4, b = 2, c = 2;
	if (foo(a, b) + 3 * 4 + (2 - 1) && d->x == 2)
	{
		printf("Hello, World !\n");
	}

	return 0;
}