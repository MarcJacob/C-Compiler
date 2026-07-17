// Core symbols for the Tokenizer stage.

#ifndef TOKENIZER_INCLUDED
#define TOKENIZER_INCLUDED

#include "compiler.h"

// Main orchestration structure, passed to most tokenizer functions to provide "global" functionality such as error reporting.
struct TokenizerProcess
{
	struct Vector* Tokens; // Vector type Token
	ui8 HasError; // Whether the tokenizer is currently in an error state.
};

// Emits an error token and sets the HasError flag.
void Tokenizer_Error(struct TokenizerProcess* Tokenizer, ui32 BufferLoc, const char* MsgFormat, ...);

#endif // TOKENIZER_INCLUDED