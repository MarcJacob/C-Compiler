// Central symbol definitions for major compiler stages and global cross-stages capabilities like positional error reporting.

#ifndef COMPILER_INCLUDED
#define COMPILER_INCLUDED

#include "core.h"
#include "vector.h"
#include "string_ansi.h"

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

// Main compiler process orchestration structure. Used to drive each stage one after another, starting from specific source files and ending with a single executable output file.
struct CompilerProcess
{
	struct Vector InputFiles; // Vector type CharBuffer_ANSI* 
	struct Vector OutputFiles; // Vector type CharBuffer_ANSI* 

	enum COMPILER_ERROR_CODE ErrorCode_Global; // Main error code of compiler has a whole.
	ui8 ErrorCode_Stage; // Optional extra error code filled in by the specific stage where the error happened.
	
	struct String_ANSI ErrorMsg; 
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
	SYMBOL_OP_BITWISE_AND,			// & (Used for both bitwise AND and Address Of. Thanks again.)
	SYMBOL_OP_ADDRESS_OF = SYMBOL_OP_BITWISE_AND,
	SYMBOL_OP_OR,					// ||
	SYMBOL_OP_BITWISE_OR,			// |
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

	SYMBOL_OP_STRUCT_DEREF,			// ->
	SYMBOL_OP_STRUCT_READ,			// .
};

static inline ui8 Symbol_IsLeftUnaryOperator(enum TOKEN_SYMBOL Symbol)
{
	switch (Symbol)
	{
		case SYMBOL_OP_INCREMENT:	// As Pre-increment
		case SYMBOL_OP_DECREMENT:	// As Pre-decrement
		case SYMBOL_OP_NOT:
		case SYMBOL_OP_BITWISE_REVERSE:
		case SYMBOL_OP_DEREF:
		case SYMBOL_OP_ADDRESS_OF:
			return 1;
		default:
			return 0;
	}
}

static inline ui8 Symbol_IsRightUnaryOperator(enum TOKEN_SYMBOL Symbol)
{
	switch (Symbol)
	{
		case SYMBOL_OP_INCREMENT: // As post-increment
		case SYMBOL_OP_DECREMENT: // As post-decrement
			return 1;
		default:
			return 0;
	}
}

static inline ui8 Symbol_IsUnaryOperator(enum TOKEN_SYMBOL Symbol)
{
	return Symbol_IsLeftUnaryOperator(Symbol) || Symbol_IsRightUnaryOperator(Symbol);
}

static inline ui8 Symbol_IsBinaryOperator(enum TOKEN_SYMBOL Symbol)
{
	switch (Symbol)
	{
		case SYMBOL_OP_ADD:
		case SYMBOL_OP_SUB:
		case SYMBOL_OP_MULT:
		case SYMBOL_OP_DIV:
		case SYMBOL_OP_MODULO:
		case SYMBOL_OP_AND:
		case SYMBOL_OP_BITWISE_AND:
		case SYMBOL_OP_OR:
		case SYMBOL_OP_BITWISE_OR:
		case SYMBOL_OP_BITWISE_XOR:
		case SYMBOL_OP_RIGHT_SHIFT:
		case SYMBOL_OP_GREATER_EQUAL:
		case SYMBOL_OP_GREATER:
		case SYMBOL_OP_LEFT_SHIFT:
		case SYMBOL_OP_LOWER_EQUAL:
		case SYMBOL_OP_LOWER:
		case SYMBOL_OP_EQUAL:
		case SYMBOL_OP_ASSIGN:
		case SYMBOL_OP_UNEQUAL:
		case SYMBOL_OP_ADD_ASSIGN:
		case SYMBOL_OP_SUB_ASSIGN:
		case SYMBOL_OP_MULT_ASSIGN:
		case SYMBOL_OP_DIV_ASSIGN:
		case SYMBOL_OP_MODULO_ASSIGN:
		case SYMBOL_OP_BITWISE_AND_ASSIGN:
		case SYMBOL_OP_BITWISE_OR_ASSIGN:
		case SYMBOL_OP_BITWISE_XOR_ASSIGN:
		case SYMBOL_OP_LEFT_SHIFT_ASSIGN:
		case SYMBOL_OP_RIGHT_SHIFT_ASSIGN:
		case SYMBOL_OP_STRUCT_DEREF:
		case SYMBOL_OP_STRUCT_READ:
			return 1;
		default:
			return 0;
	}
}

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
	KEYWORD_SIGNED,
	KEYWORD_UNSIGNED,
	KEYWORD_CONST,
	KEYWORD_VOLATILE,
	KEYWORD_STRUCT,
	KEYWORD_ENUM,
	KEYWORD_UNION,

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

inline ui8 Keyword_IsPrimitiveType(enum TOKEN_KEYWORD Keyword)
{
	return Keyword >= KEYWORD_VOID && Keyword <= KEYWORD_DOUBLE;
}


inline ui8 Keyword_IsTypeSpecifier(enum TOKEN_KEYWORD Keyword)
{
	return Keyword >= KEYWORD_STATIC && Keyword <= KEYWORD_VOLATILE;
}

#define IDENTIFIER_MAX_LENGTH (128)

struct Token
{
	enum TOKEN_TYPE Type; // Type of token this is.
	ui32 BufferLocation; // Location of token in character index.

	union
	{
		enum TOKEN_SYMBOL Symbol;
		enum TOKEN_KEYWORD Keyword;

		struct String_ANSI Identifier;
		struct String_ANSI LiteralString;
		char LiteralCharacter;

		union
		{
			i64 Integer;
			double Double;
			float Float;
		} LiteralNumber;

	} Val; // Main Value union, giving type-specific information about the token.
};

// Parser Stage

// All possible values for the type of an AST Node.
enum AST_NODE_TYPE
{
	AST_NODE_VARIABLE,		// Global, Local, Structure or Param Variable.
	AST_NODE_STRUCT,		// Structure definition or declaration.
	AST_NODE_ENUM,			// Enumeration definition or declaration.
	AST_NODE_FUNCTION,		// Function definition or declaration.
	AST_NODE_STATEMENT,	// "Executable" node found inside a block of some kind.
	AST_NODE_STATEMENT_BLOCK,			// Container statement for other statements.
	AST_NODE_STATEMENT_LEAF,		// "Leaf" statement executing an expression, a variable declaration / definition or a flow control.
	AST_NODE_STATEMENT_IF,			// Non-looping condition statement executing the next statement only if a condition expression returns > 0, or an else statement if specified.
	AST_NODE_STATEMENT_WHILE,			// Looping condition statement executing the next statement only if a condition expression returns > 0 and attempting re-entry.
	AST_NODE_STATEMENT_FOR,			// Looping condition similar to WHILE with specific Init and Post-Loop expression statements.
	AST_NODE_EXPRESSION,	// Expression with or without a compile-time result located inside instructions and variable definitions.
};

// Values for primitive data types + an extra value indicating the type is user-defined. 
enum DATATYPE
{
	DATATYPE_VOID,
	DATATYPE_CHAR,
	DATATYPE_SHORT,
	DATATYPE_INT32,
	DATATYPE_INT64,
	DATATYPE_FLOAT,
	DATATYPE_DOUBLE,

	DATATYPE_USER_DEFINED, // Indicates an Identifier should be read to ascertain the exact type.
};

// Flags modifying the behavior / definition of a datatype.
enum DATATYPE_FLAGS
{
	DATATYPE_IS_UNSIGNED = 1 << 0,
	DATATYPE_IS_STATIC = 1 << 1,
	DATATYPE_IS_CONST = 1 << 2,
	DATATYPE_IS_VOLATILE = 1 << 3,
	DATATYPE_IS_STRUCT = 1 << 4,
	DATATYPE_IS_UNION = 1 << 5,
	DATATYPE_IS_ENUM = 1 << 6,
};

// Definition for a value type associated to a variable or a function.
// Non-unique, can be equal to / compatible with other defs.
struct DatatypeDef
{
	enum DATATYPE Type;
	enum DATATYPE_FLAGS Flags;
	ui16 Size; // Total size of the type in bytes.

	struct String_ANSI TypeName; // String representation of the type / actual type name for USER_DEFINED type.
	ui8 PointerLevel; // How many pointer indirection layers this has, meaning if > 0, this is a pointer.
};

enum EXPRESSION_TYPE
{
	EXP_LITERAL_INT, // Expression is a literal whole number.
	EXP_LITERAL_FLOAT, // Expression is a literal floating-point number.
	EXP_LITERAL_DOUBLE, // Expression is a literal double-precision floating-point number.
	EXP_LITERAL_STRING, // Expression is a literal string.
	EXP_LITERAL_CHAR, // Expression is a literal character.

	EXP_VARIABLE, // Expression reads a variable value.

	EXP_OP, // Expression is a unary or binary operator applied over one or two operand sub-expressions located to either side.
	EXP_FUNCTION_CALL, // Expression is a function call's return value.
};

// Returns whether the passed type of expression is supposed to have sub-expressions.
static inline ui8 Expression_IsLeafType(enum EXPRESSION_TYPE Type)
{
	return Type < EXP_OP;
}

// Node composing an Abstract Syntax Tree.
struct AST_Node
{
	enum AST_NODE_TYPE Type;
	ui32 BufferLocation; // Source buffer location associated with this node.
	// TODO: Consider making this a full-blown token pointer / copy of the "main token" behind this node instead,
	// so as much information as possible can be kept.

	union
	{
		struct
		{
			struct DatatypeDef ReturnType;
			struct String_ANSI Name;
			struct Vector Params; // Vector type = AST_Node* (Parameter variables in order of declaration)
			struct AST_Node* Statements; // Root instruction block if this is the function definition.
		} Function;

		// Root type for any executable instruction, located inside a block.
		struct
		{
			union
			{
				struct
				{
					struct AST_Node* EntryCondition; // Expression node that should resolve to > 0 for initial entry into the block.

					struct AST_Node* ExecStatement; // Statement to be executed on successful entry.
					struct AST_Node* ExecInstruction_Else;	// Statement to be executed on entry failure.
				} If;
				
				struct
				{
					struct AST_Node* EntryCondition; // If non-NULL, expression node that should resolve to > 0 for initial entry into the block.
					struct AST_Node* LoopCondition; // Expression node that should resolve to > 0 for re-entry.

					struct AST_Node* ExecStatement; // Statement executed on each loop.
				} While;

				struct
				{
					struct AST_Node* InitExpression; // Initial expression statement to be ran regardless before initial entry is attempted.
					struct AST_Node* LoopCondition; // Expression statement that should resolve to > 0 for initial and repeated entry.
					struct AST_Node* PostLoopExpression; // Expression statement to be executed after each loop before re-entry is attempted.

					struct AST_Node* ExecStatement; // Statement executed on each loop.
				} For;

				// Container for an indefinite amount of sub-instructions.
				struct
				{
					struct Vector Statements; // Sub-instructions contained in the block, in order of declaration.
				} Block;
				
				// "Single statement" instruction types with no sub-instructions.

				struct AST_Node* Expression; // Root expression node representing an operating instruction.
				struct AST_Node* Variable; // Variable node representing a variable declaration / definition.
				struct AST_Node* Control; // "Flow Control" statement affecting the program's execution flow (goto, return, break, continue...).
			};

		} Statement;

		struct
		{
			struct DatatypeDef Type;
			struct String_ANSI Name;
			ui64 ArraySize; // If > 0, this variable is an array for whatever type is contains.

			struct AST_Node* Value; // For variable definitions, specifies the value expression to use for initialization. For arrays, the value expression for array size.

		} Variable;

		struct
		{
			struct DatatypeDef Type; // "Final" Datatype def for this structure. Also contains the structure's name.
			struct Vector Members; // Vector type = AST_Node*
		} Struct;

		struct
		{
			struct DatatypeDef ResultType; // Expected return type for this expression.
			enum EXPRESSION_TYPE Type; // Type of expression.

			ui8 CompileTimeResolvable; // Whether this expression has a compile-time-resolvable value (required for array sizes and such). Determined by Symbolizer.

			union
			{
				struct
				{
					struct AST_Node* LeftOperand; // Expression sub-node.
					struct AST_Node* RightOperand; // Expression sub-node.
					enum TOKEN_SYMBOL OperatorSymbol;
				} Op;

				struct
				{
					i64 Integer;
					float Float;
					double Double;
					char Character;
					struct String_ANSI String;
				} Literal;

				struct
				{
					struct String_ANSI Name;
				} Variable;

				struct
				{
					struct String_ANSI* FunctionName;
					struct Vector Params; // Vector of sub-expressions corresponding to expected function parameters.
				} FunctionCall;
			};

		} Expression;

	} Val;
};

#endif // COMPILER_INCLUDED
