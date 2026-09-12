// Single file test.
// Hello world program.

void printf(const char* MsgFormat);

void foo(float in, int* out)
{
	return;
}

int main(int argc, char** argv)
{
	long long a = 5;

	printf("Hello, World !\n");
	return a * 2 / 2 - (4 / 2); // Compile-time expression resolution test.
}