#include "tokenizer.h"
// Main implementation file for the Tokenizer stage.

void Tokenizer(struct CompilerProcess* Compiler, struct CharBuffer_ANSI* InputCharacters, struct Vector* OutTokenVec)
{
	// Initialize process.
	struct TokenizerProcess Tokenizer = { 0 };
	Tokenizer.Tokens = OutTokenVec;

	Tokenizer_Error(&Tokenizer, 0, "Tokenizer not implemented.");

	// ... TODO: Tokenizer !!
}

void Tokenizer_Error(struct TokenizerProcess* Tokenizer, ui32 BufferLoc, const char* MsgFormat, ...)
{
	// TODO: Emit Error token.
	Tokenizer->HasError = 1;
}
