// Core symbols for the Tokenizer stage.

#ifndef TOKENIZER_INCLUDED
#define TOKENIZER_INCLUDED

#include "compiler.h"

// Main orchestration structure, passed to most tokenizer functions to provide "global" functionality such as error reporting.
struct TokenizerProcess
{
	// Input
	struct CharBuffer_ANSI* SourceBuffer; // Main input buffer of source code characters.

	// Output
	struct Vector* Tokens; // Vector type Token. Main output.
	ui8 HasError; // Whether the tokenizer is currently in an error state.
	struct
	{
		ui8 Code; // TODO: Consider possible error codes. Unused as of now.
		ui32 Location; // Index of character where error happened, if applicable.
		ErrorMessage Message;
	} Error;
};

// Sets the HasError flag on the Tokenizer process and fills in the error message.
void Tokenizer_Error(struct TokenizerProcess* Tokenizer, ui32 BufferLoc, const char* MsgFormat, ...);

#endif // TOKENIZER_INCLUDED