// Single file test.
// Hello world program.

int foo(int a, int b)
{
	return a + b;
}

int main(int argc, char** argv)
{
	int a = 4, b = 2, c = 2;

	if (foo(a, b))
	{
		printf("Hello, World !\n");
	}

	return 0;
}