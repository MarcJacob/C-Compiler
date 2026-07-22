#include "tokenizer.h"
#include <stdarg.h>
// Main implementation file for the Tokenizer stage.

void Tokenizer_Run(struct TokenizerProcess* TokenizerProc)
{
	ASSERT(TokenizerProc != NULL);
	ASSERT(TokenizerProc->SourceBuffer != NULL);

	// Create root reader, then start the top-level iteration process.
	// At the root level we're only looking for symbol construction, meaning anything that starts with a data type / modifier.

	struct CharBufferReader_ANSI SourceReader = CreateBufferReader_ANSI(TokenizerProc->SourceBuffer);

	char NextSourceChar = CharBufferReader_ReadNext(&SourceReader);
	while (NextSourceChar != EOF)
	{
		// TODO: Process character...
		if (NextSourceChar == '\n')
		{
			printf("TOKENIZER - Reading character '\\n'.\n");
		}
		else
		{
			printf("TOKENIZER - Reading character '%c'.\n", NextSourceChar);
		}
		NextSourceChar = CharBufferReader_ReadNext(&SourceReader);
	}
}

void Tokenizer_Error(struct TokenizerProcess* Tokenizer, ui32 BufferLoc, const char* MsgFormat, ...)
{
	Tokenizer->HasError = 1;

	va_list args;
	va_start(args, MsgFormat);
	vsprintf_s(Tokenizer->Error.Message, sizeof(Tokenizer->Error.Message), MsgFormat, args);
	va_end(args);
}
