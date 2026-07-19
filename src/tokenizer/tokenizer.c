#include "tokenizer.h"
#include <stdarg.h>
// Main implementation file for the Tokenizer stage.

void Tokenizer_Run(struct TokenizerProcess* TokenizerProc)
{
	// Initialize process.

	Tokenizer_Error(TokenizerProc, 0, "Tokenizer not implemented.");

	// ... TODO: Tokenizer !!
}

void Tokenizer_Error(struct TokenizerProcess* Tokenizer, ui32 BufferLoc, const char* MsgFormat, ...)
{
	Tokenizer->HasError = 1;

	va_list args;
	va_start(args, MsgFormat);
	vsprintf_s(Tokenizer->Error.Message, sizeof(Tokenizer->Error.Message), MsgFormat, args);
	va_end(args);
}
