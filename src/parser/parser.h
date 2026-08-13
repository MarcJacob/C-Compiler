// Core Symbols for Parser stage.

#ifndef PARSER_INCLUDED
#define PARSER_INCLUDED

#include "compiler.h"

// Main orchestration structure, passed to most parser functions to provide "global" functionality such as error reporting.
struct ParserProcess
{
	// Input
	struct Vector* SourceTokens; // Vector type = struct Token
	ui32 TokenIndex; // Index of next token to be read.

	// Output
	struct Vector* RootNodes; // Vector type = struct AST_Node*. Main output.

	ui8 HasError; // Whether the Parser is currently in an error state.
	struct
	{
		ui32 Location; // Index of character where error happened, if applicable.
		struct String_ANSI Message;
	} Error;
};

// Sets the HasError flag on the Parser process and fills in the error message.
void Parser_Error(struct ParserProcess* Parser, ui32 BufferLoc, const char* MsgFormat, ...);

#endif // PARSER_INCLUDED
