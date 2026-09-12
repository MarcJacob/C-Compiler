// Single file test.
// Hello world program.

void printf(const char* MsgFormat);

int main(int argc, char** argv)
{
	long long a = 5;

	printf("Hello, World !\n");
	return a * 2 / 2 - (4 / 2); // Compile-time expression resolution test.
}