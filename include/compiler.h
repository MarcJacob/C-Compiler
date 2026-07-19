// Central symbol definitions for major compiler stages and global cross-stages capabilities like positional error reporting.

#ifndef COMPILER_INCLUDED
#define COMPILER_INCLUDED

#include "core.h"
#include "vector.h"

// Compiler

// All global error codes the compiler can report.
enum COMPILER_ERROR_CODE
{
	COMPILER_SUCCESS, // Compilation successful.
	COMPILER_FATAL_ERROR, // Fatal, compiler-wide error due to a fault in implementation.
	COMPILER_GENERAL_ERROR, // General input/user-related error outside any particular stage.
	COMPILER_TOKENIZER_STAGE_ERROR, // Error during the Tokenizer stage. Read Stage error code as a TOKENIZER_ERROR_CODE enum value.
	COMPILER_PARSER_STAGE_ERROR, // Error during the Parser stage. Read Stage error code as a PARSER_ERROR_CODE enum value.
	CCOMPILER_SYMBOL_SOLVER_STAGE_ERROR, // Error during the Symbol Solver stage. Read Stage error code as a SYMBOL_SOLVER_ERROR_CODE enum value.
};

typedef char ErrorMessage[256]; // TODO: Replace with proper ANSI string implementation.

// Main compiler process orchestration structure. Used to drive each stage one after another, starting from specific source files and ending with a single executable output file.
struct CompilerProcess
{
	struct Vector InputFiles; // Vector type CharBuffer_ANSI* 
	struct Vector OutputFiles; // Vector type CharBuffer_ANSI* 

	enum COMPILER_ERROR_CODE ErrorCode_Global; // Main error code of compiler has a whole.
	ui8 ErrorCode_Stage; // Optional extra error code filled in by the specific stage where the error happened.
	
	ErrorMessage ErrorMsg; 
};

// Begins compilation process from the contents of the Compiler Process structure.
// At the minimum, a single Source File must be specified.
void Compiler_Run(struct CompilerProcess* Compiler);

// Tokenizer stage

// Main enumeration of supported token types.
enum TOKEN_TYPE
{
	TOKEN_ERROR, // Special token type used to locate error(s) that happened during the tokenizer stage.
	TOKEN_IDENTIFIER,
	TOKEN_SYMBOL,
	TOKEN_OPERATOR,
	TOKEN_COMMENT,
	TOKEN_LITERAL_STRING,
	TOKEN_LITERAL_NUMBER,
};

struct Token
{
	enum TOKEN_TYPE Type; // Type of token this is.
	ui32 BufferLocation; // Location of token in character index.

	union
	{
		ErrorMessage Error;

		// TODO: ANSI string implementation.
		ui8 Identifier;
		char Symbol;
		char Operator;
		// TODO: ANSI string implementation.
		ui8 Comment;
		// TODO: ANSI string implementation.
		ui8 LiteralString;
		i64 LiteralNumber;

	} Val; // Main Value union, giving type-specific information about the token.
};

#endif // COMPILER_INCLUDED
