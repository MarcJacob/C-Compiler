// Single file test.
// Hello world program.

void printf(const char* MsgFormat);

void foo(float in, int* out)
{
	return;
}

int main(int argc, char** argv)
{
	int a = 5;

	a = 6;

	printf("Hello, World !\n");
	return a * 2 / 2 - (4 / 2); // Compile-time expression resolution test.
}