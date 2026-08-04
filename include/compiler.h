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
	TOKEN_KEYWORD, // "Keywords" include language primitive types, type modifiers, special operators and flow control operators.
	TOKEN_IDENTIFIER, // "Identifiers" include any non-primitive name found in the codebase, identifying a user-defined type, variable or function.
	TOKEN_SYMBOL, // "Symbols" include all one-to-two-character-long separators and operators.
	TOKEN_LITERAL_CHAR, // "Literal chars" include any single character (or character pair for escaped characters) between the ' delimitors.
	TOKEN_LITERAL_STRING, // "Literal strings" include any string of characters between the " delimitors.
	TOKEN_LITERAL_NUMBER_INT, // "Literal numbers" include any number not found inside a literal string, identifier or keyword.
	TOKEN_LITERAL_NUMBER_FLOAT, // "Literal numbers" include any number not found inside a literal string, identifier or keyword.
	TOKEN_LITERAL_NUMBER_DOUBLE, // "Literal numbers" include any number not found inside a literal string, identifier or keyword.
	TOKEN_COMMENT, // Currently unused.
};

// All possible values for a Symbol Token.
enum TOKEN_SYMBOL
{
	// Statements & expression building
	SYMBOL_SEMICOLON,				// ;
	SYMBOL_COMMA,					// ,
	SYMBOL_COLON,					// :
	SYMBOL_PARENTHESIS_OPEN,		// (
	SYMBOL_PARENTHESIS_CLOSE,		// )
	SYMBOL_BRACKET_OPEN,			// [
	SYMBOL_BRACKET_CLOSE,			// ]
	SYMBOL_BRACE_OPEN,				// {
	SYMBOL_BRACE_CLOSE,				// }

	// Operators
	SYMBOL_OP_ARROW,				// ->
	SYMBOL_OP_INCREMENT,			// ++
	SYMBOL_OP_ADD,					// +
	SYMBOL_OP_DECREMENT,			// --
	SYMBOL_OP_SUB,					// -
	SYMBOL_STAR,					// * (Used for both MULT and DEREF. Thanks C Standard !)
	SYMBOL_OP_DEREF = SYMBOL_STAR,				
	SYMBOL_OP_MULT = SYMBOL_STAR, 
	SYMBOL_OP_DIV,					// /
	SYMBOL_OP_MODULO,				// %

	SYMBOL_OP_AND,					// &&
	SYMBOL_OP_BITWISE_AND,			// &
	SYMBOL_OP_OR,					// ||
	SYNBOL_OP_BITWISE_OR,			// |
	SYMBOL_OP_BITWISE_XOR,			// ^
	SYMBOL_OP_RIGHT_SHIFT,			// >>
	SYMBOL_OP_GREATER_EQUAL,		// >=
	SYMBOL_OP_GREATER,				// >
	SYMBOL_OP_LEFT_SHIFT,			// <<
	SYMBOL_OP_LOWER_EQUAL,			// <=
	SYMBOL_OP_LOWER,				// <
	SYMBOL_OP_EQUAL,				// ==
	SYMBOL_OP_ASSIGN,				// =
	SYMBOL_OP_UNEQUAL,				// !=
	SYMBOL_OP_NOT,					// !

	SYMBOL_OP_BITWISE_REVERSE,		// ~

	SYMBOL_OP_ADD_ASSIGN,			// +=
	SYMBOL_OP_SUB_ASSIGN,			// -=
	SYMBOL_OP_MULT_ASSIGN,			// *=
	SYMBOL_OP_DIV_ASSIGN,			// /=
	SYMBOL_OP_MODULO_ASSIGN,		// %=
	SYMBOL_OP_BITWISE_AND_ASSIGN,	// &=
	SYMBOL_OP_BITWISE_OR_ASSIGN,	// |=
	SYMBOL_OP_BITWISE_XOR_ASSIGN,	// ^=
	SYMBOL_OP_LEFT_SHIFT_ASSIGN,	// <<=
	SYMBOL_OP_RIGHT_SHIFT_ASSIGN,	// >>=
};

// All possible values for a Keyword token.
enum TOKEN_KEYWORD
{
	// Primitive types
	KEYWORD_VOID,
	KEYWORD_CHAR,
	KEYWORD_SHORT,
	KEYWORD_INT,
	KEYWORD_FLOAT,
	KEYWORD_LONG,
	KEYWORD_DOUBLE,

	// Type modifiers
	KEYWORD_STATIC,
	KEYWORD_UNSIGNED,
	KEYWORD_STRUCT,
	KEYWORD_ENUM,
	KEYWORD_UNION,
	KEYWORD_VOLATILE,

	// Flow control & Block modifiers
	KEYWORD_IF,
	KEYWORD_ELSE,
	KEYWORD_FOR,
	KEYWORD_DO,
	KEYWORD_WHILE,
	KEYWORD_SWITCH,
	KEYWORD_CASE,
	KEYWORD_BREAK,
	KEYWORD_CONTINUE,
	KEYWORD_RETURN,

	// Primitive operators
	KEYWORD_SIZEOF,
};

#define IDENTIFIER_MAX_LENGTH (128)

struct Token
{
	enum TOKEN_TYPE Type; // Type of token this is.
	ui32 BufferLocation; // Location of token in character index.

	union
	{
		// TODO: ANSI string implementation.
		char* Identifier;

		enum TOKEN_SYMBOL Symbol;
		enum TOKEN_KEYWORD Keyword;

		// TODO: ANSI string implementation.
		char* LiteralString;

		union
		{
			i64 Integer;
			double Double;
			float Float;
		} LiteralNumber;

	} Val; // Main Value union, giving type-specific information about the token.
};

#endif // COMPILER_INCLUDED
