// Single file test.
// Hello world program.

void printf(const char* MsgFormat);

void foo(float in, int* out)
{
	return;
}

struct test_struct
{
	int foo;
	int bar;

	struct
	{
		int zil;
	} test_substruct;
};

int main(int argc, char** argv)
{
	int a[2];

	a[2] = 6;

	struct test_struct b = { 0 };
	b.foo = 4;

	b.test_substruct.zil = 5;

	printf("Hello, World !\n");
	return a * 2 / 2 - (4 / 2); // Compile-time expression resolution test.
}