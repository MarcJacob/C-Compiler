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
// SYMBOL_OP_* values are used to indicate operators.
// SYMBOL_OP_AMB_* values are ambiguous operator symbols that are disambiguated into their final value during parsing.
// TODO: Consider using a completely different enumeration for "parsed operators" instead.
enum TOKEN_SYMBOL
{
	SYMBOL_NONE = 0,

	// Statements & expression delimitors.
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
	// TODO: Add ternary.
	SYMBOL_OP_ARROW,				// ->
	SYMBOL_OP_AMB_INCREMENT,		// ++
	SYMBOL_OP_PRE_INCREMENT,		// ++ (De-ambiguated during the parsing process)
	SYMBOL_OP_POST_INCREMENT,		// ++ (De-ambiguated during the parsing process)
	SYMBOL_OP_ADD,					// +
	SYMBOL_OP_AMB_DECREMENT,		// --
	SYMBOL_OP_PRE_DECREMENT,		// -- (De-ambiguated during the parsing process)
	SYMBOL_OP_POST_DECREMENT,		// -- (De-ambiguated during the parsing process)
	SYMBOL_OP_AMB_MINUS,			// -
	SYMBOL_OP_SUB,					// - (De-ambiguated during the parsing process)
	SYMBOL_OP_NEGATE,				// - (De-ambiguated during the parsing process)
	SYMBOL_OP_AMB_STAR,				// * 
	SYMBOL_OP_DEREF,				// * (De-ambiguated during the parsing process)
	SYMBOL_OP_MULT,					// * (De-ambiguated during the parsing process)
	SYMBOL_OP_DIV,					// /
	SYMBOL_OP_MOD,					// %

	SYMBOL_OP_AND,					// &&
	SYMBOL_OP_AMB_AMP,				// &  
	SYMBOL_OP_BITWISE_AND,			// & (De-ambiguated during the parsing process)
	SYMBOL_OP_ADDRESS_OF,			// & (De-ambiguated during the parsing process)
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
	SYMBOL_OP_MOD_ASSIGN,			// %=
	SYMBOL_OP_BITWISE_AND_ASSIGN,	// &=
	SYMBOL_OP_BITWISE_OR_ASSIGN,	// |=
	SYMBOL_OP_BITWISE_XOR_ASSIGN,	// ^=
	SYMBOL_OP_LEFT_SHIFT_ASSIGN,	// <<=
	SYMBOL_OP_RIGHT_SHIFT_ASSIGN,	// >>=

	SYMBOL_OP_STRUCT_DEREF,			// ->
	SYMBOL_OP_STRUCT_READ,			// .
};

// Returns 1 if the passed symbol corresponds to a left-unary operator.
// Note: Handles ambiguous operators. Don't forget to de-ambiguate them afterwards.
static inline ui8 Symbol_IsLeftUnaryOp(enum TOKEN_SYMBOL Symbol)
{
	switch (Symbol)
	{
		case SYMBOL_OP_AMB_INCREMENT:
		case SYMBOL_OP_PRE_INCREMENT:
		case SYMBOL_OP_AMB_DECREMENT:
		case SYMBOL_OP_PRE_DECREMENT:
		case SYMBOL_OP_NOT:
		case SYMBOL_OP_BITWISE_REVERSE:
		case SYMBOL_OP_AMB_MINUS:
		case SYMBOL_OP_NEGATE:
		case SYMBOL_OP_AMB_STAR:
		case SYMBOL_OP_DEREF:
		case SYMBOL_OP_AMB_AMP:
		case SYMBOL_OP_ADDRESS_OF:
			return 1;
		default:
			return 0;
	}
}

// Returns 1 if the passed symbol corresponds to a right-unary operator.
// Note: Handles ambiguous operators. Don't forget to de-ambiguate them afterwards.
static inline ui8 Symbol_IsRightUnaryOp(enum TOKEN_SYMBOL Symbol)
{
	switch (Symbol)
	{
		case SYMBOL_OP_AMB_INCREMENT:
		case SYMBOL_OP_POST_INCREMENT:
		case SYMBOL_OP_AMB_DECREMENT:
		case SYMBOL_OP_POST_DECREMENT:
			return 1;
		default:
			return 0;
	}
}

static inline ui8 Symbol_IsUnaryOp(enum TOKEN_SYMBOL Symbol)
{
	return Symbol_IsLeftUnaryOp(Symbol) || Symbol_IsRightUnaryOp(Symbol);
}

static inline ui8 Symbol_IsBinaryOp(enum TOKEN_SYMBOL Symbol)
{
	switch (Symbol)
	{
		case SYMBOL_OP_ADD:
		case SYMBOL_OP_AMB_MINUS:
		case SYMBOL_OP_SUB:
		case SYMBOL_OP_AMB_STAR:
		case SYMBOL_OP_MULT:
		case SYMBOL_OP_DIV:
		case SYMBOL_OP_MOD:
		case SYMBOL_OP_AND:
		case SYMBOL_OP_AMB_AMP:
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
		case SYMBOL_OP_MOD_ASSIGN:
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

static inline ui8 Symbol_IsOp(enum TOKEN_SYMBOL Symbol)
{
	return Symbol_IsBinaryOp(Symbol) || Symbol_IsUnaryOp(Symbol);
}

static inline enum TOKEN_SYMBOL Symbol_DeambiguateLeftUnaryOp(enum TOKEN_SYMBOL Op)
{
	ASSERT(Symbol_IsLeftUnaryOp(Op));
	switch (Op)
	{
	case SYMBOL_OP_AMB_AMP:
		return SYMBOL_OP_ADDRESS_OF;
	case SYMBOL_OP_AMB_INCREMENT:
		return SYMBOL_OP_PRE_INCREMENT;
	case SYMBOL_OP_AMB_DECREMENT:
		return SYMBOL_OP_PRE_DECREMENT;
	case SYMBOL_OP_AMB_MINUS:
		return SYMBOL_OP_NEGATE;
	default:
		return Op;
	}
}

static inline enum TOKEN_SYMBOL Symbol_DeambiguateRightUnaryOp(enum TOKEN_SYMBOL Op)
{
	ASSERT(Symbol_IsRightUnaryOp(Op));
	switch (Op)
	{
	case SYMBOL_OP_AMB_INCREMENT:
		return SYMBOL_OP_POST_INCREMENT;
	case SYMBOL_OP_AMB_DECREMENT:
		return SYMBOL_OP_POST_DECREMENT;
	default:
		return Op;
	}
}

static inline enum TOKEN_SYMBOL Symbol_DeambiguateBinaryOp(enum TOKEN_SYMBOL Op)
{
	ASSERT(Symbol_IsBinaryOp(Op));
	switch (Op)
	{
	case SYMBOL_OP_AMB_AMP:
		return SYMBOL_OP_BITWISE_AND;
	case SYMBOL_OP_AMB_MINUS:
		return SYMBOL_OP_SUB;
	case SYMBOL_OP_AMB_STAR:
		return SYMBOL_OP_MULT;
	default:
		return Op;
	}
}

#define OPERATOR_PARSE_RULE_GROUP_MAX_SIZE (16)
// Describes the associativity rule of a group of operators, and the fact that
// the operators in the group have the same level of precedence.
// Used in the Parse Rules Table, with groups coming first being of a higher precedence level
// than those coming after.
struct OperatorParseRulesGroup
{
	enum TOKEN_SYMBOL Op[OPERATOR_PARSE_RULE_GROUP_MAX_SIZE];
	ui8 IsRightToLeftAssociative;
};

static struct OperatorParseRulesGroup OPERATOR_PARSE_RULES_TABLE[] =
{
	// Unaries
	{
		{
			SYMBOL_OP_PRE_INCREMENT,
			SYMBOL_OP_POST_INCREMENT,
			SYMBOL_OP_NEGATE,
			SYMBOL_OP_NOT,
			SYMBOL_OP_BITWISE_REVERSE,
			SYMBOL_OP_DEREF,
			SYMBOL_OP_ADDRESS_OF,
		}, 1
	},

	// Arithmetic groups
	{
		{
			SYMBOL_OP_MULT,
			SYMBOL_OP_DIV,
			SYMBOL_OP_MOD,
		}, 0
	},
	{
		{
			SYMBOL_OP_ADD,
			SYMBOL_OP_SUB,
		}, 0
	},

	// Shifts
	{
		{
			SYMBOL_OP_LEFT_SHIFT,
			SYMBOL_OP_RIGHT_SHIFT,
		}, 0
	},

	// Inequalities
	{
		{
			SYMBOL_OP_LOWER,
			SYMBOL_OP_LOWER_EQUAL,
			SYMBOL_OP_GREATER,
			SYMBOL_OP_GREATER_EQUAL,
		}, 0
	},

	// Equalities
	{
		{
			SYMBOL_OP_EQUAL,
			SYMBOL_OP_UNEQUAL,
		}, 0
	},

	// Bitwise Ops
	{
		{
			SYMBOL_OP_BITWISE_AND,
		}, 0
	},
	{
		{
			SYMBOL_OP_BITWISE_XOR,
		}, 0
	},
	{
		{
			SYMBOL_OP_BITWISE_OR,
		}, 0
	},

	// Booleans
	{
		{
			SYMBOL_OP_AND,
		}, 0
	},
	{
		{
			SYMBOL_OP_OR,
		}, 0
	},

	// Assignments
	{
		{
			SYMBOL_OP_ASSIGN,
			SYMBOL_OP_ADD_ASSIGN,
			SYMBOL_OP_SUB_ASSIGN,
			SYMBOL_OP_MULT_ASSIGN,
			SYMBOL_OP_DIV_ASSIGN,
			SYMBOL_OP_MOD_ASSIGN,
			SYMBOL_OP_BITWISE_AND_ASSIGN,
			SYMBOL_OP_BITWISE_OR_ASSIGN,
			SYMBOL_OP_BITWISE_XOR_ASSIGN,
			SYMBOL_OP_LEFT_SHIFT_ASSIGN,
			SYMBOL_OP_RIGHT_SHIFT_ASSIGN,
		}, 1
	},

	// The lonely Comma
	{
		{
			SYMBOL_COMMA,
		}, 0
	},
};

static inline void Symbol_GetOpParseRules(enum TOKEN_SYMBOL Op, ui8* OutPrecedenceLevel, ui8* OutIsRightToLeft)
{
	ASSERT_MSG(Symbol_IsOp(Op), "Attempted to get operator parse rules for a non-operator symbol.");
	ASSERT(OutPrecedenceLevel != NULL || OutIsRightToLeft != NULL);

	// Find symbol in the rules table. Its precedence level corresponds to the index of its group.

	static const ui8 RULES_TABLE_SIZE = sizeof(OPERATOR_PARSE_RULES_TABLE) / sizeof(struct OperatorParseRulesGroup);
	for (ui8 GroupIndex = 0; GroupIndex < RULES_TABLE_SIZE; GroupIndex++)
	{
		for (ui8 OpIndex = 0; OpIndex < OPERATOR_PARSE_RULE_GROUP_MAX_SIZE; OpIndex++)
		{
			if (OPERATOR_PARSE_RULES_TABLE[GroupIndex].Op[OpIndex] == Op)
			{
				if (OutPrecedenceLevel != NULL) *OutPrecedenceLevel = GroupIndex;
				if (OutIsRightToLeft != NULL) *OutIsRightToLeft = OPERATOR_PARSE_RULES_TABLE[GroupIndex].IsRightToLeftAssociative;
				return;
			}
			if (OPERATOR_PARSE_RULES_TABLE[GroupIndex].Op[OpIndex] == SYMBOL_NONE)
			{
				break;
			}
		}
	}

	ASSERT_MSG(0, "Missing operator in rules table.");
}

// Returns 0 if the operators are of the same precedence level, 1 if A > B, -1 if B < A.
static inline ui8 Symbol_CompareOpPrecedence(enum TOKEN_SYMBOL OpA, enum TOKEN_SYMBOL OpB)
{
	ui8 APrec, BPrec;
	Symbol_GetOpParseRules(OpA, &APrec, NULL);
	Symbol_GetOpParseRules(OpB, &BPrec, NULL);

	return APrec == BPrec ? 0 : (APrec > BPrec ? 1 : -1);
}

static inline ui8 Symbol_IsOpLeftToRightAssociative(enum TOKEN_SYMBOL Op)
{
	ui8 RightToLeft;
	Symbol_GetOpParseRules(Op, NULL, &RightToLeft);

	return !RightToLeft;
}

struct SymbolToStringPair
{
	enum TOKEN_SYMBOL Symbol;
	const char* String;
};

// Longer symbols are listed before any shorter symbol they share a prefix with (eg. "--" before "-"),
// since the Tokenizer's symbol parsing takes the first match found in this table.
static const struct SymbolToStringPair SYMBOL_TO_STRING_TABLE[] =
{
	{ SYMBOL_SEMICOLON, ";" },
	{ SYMBOL_COMMA, "," },
	{ SYMBOL_COLON, ":" },
	{ SYMBOL_PARENTHESIS_OPEN, "(" },
	{ SYMBOL_PARENTHESIS_CLOSE, ")" },
	{ SYMBOL_BRACKET_OPEN, "[" },
	{ SYMBOL_BRACKET_CLOSE, "]" },
	{ SYMBOL_BRACE_OPEN, "{" },
	{ SYMBOL_BRACE_CLOSE, "}" },

	{ SYMBOL_OP_ARROW, "->" },
	{ SYMBOL_OP_AMB_INCREMENT, "++" },
	{ SYMBOL_OP_PRE_INCREMENT, "pre_inc" },
	{ SYMBOL_OP_POST_INCREMENT, "post_inc" },
	{ SYMBOL_OP_ADD_ASSIGN, "+=" },
	{ SYMBOL_OP_ADD, "+" },
	{ SYMBOL_OP_AMB_DECREMENT, "--" },
	{ SYMBOL_OP_PRE_DECREMENT, "post_dec" },
	{ SYMBOL_OP_POST_DECREMENT, "post_dec" },
	{ SYMBOL_OP_SUB_ASSIGN, "-=" },
	{ SYMBOL_OP_AMB_MINUS, "-" },
	{ SYMBOL_OP_NEGATE, "neg" },
	{ SYMBOL_OP_SUB, "sub" },
	{ SYMBOL_OP_MULT_ASSIGN, "*=" },
	{ SYMBOL_OP_AMB_STAR, "*" },
	{ SYMBOL_OP_MULT, "mult" },
	{ SYMBOL_OP_DEREF, "deref" },
	{ SYMBOL_OP_DIV_ASSIGN, "/=" },
	{ SYMBOL_OP_DIV, "/" },
	{ SYMBOL_OP_MOD_ASSIGN, "%=" },
	{ SYMBOL_OP_MOD, "%" },

	{ SYMBOL_OP_AND, "&&" },
	{ SYMBOL_OP_BITWISE_AND_ASSIGN, "&=" },
	{ SYMBOL_OP_AMB_AMP, "&" },
	{ SYMBOL_OP_AND, "and" },
	{ SYMBOL_OP_ADDRESS_OF, "address_of" },
	{ SYMBOL_OP_OR, "||" },
	{ SYMBOL_OP_BITWISE_OR_ASSIGN, "|=" },
	{ SYMBOL_OP_BITWISE_OR, "|" },
	{ SYMBOL_OP_BITWISE_XOR_ASSIGN, "^=" },
	{ SYMBOL_OP_BITWISE_XOR, "^" },

	{ SYMBOL_OP_RIGHT_SHIFT_ASSIGN, ">>=" },
	{ SYMBOL_OP_RIGHT_SHIFT, ">>" },
	{ SYMBOL_OP_GREATER_EQUAL, ">=" },
	{ SYMBOL_OP_GREATER, ">" },
	{ SYMBOL_OP_LEFT_SHIFT_ASSIGN, "<<=" },
	{ SYMBOL_OP_LEFT_SHIFT, "<<" },
	{ SYMBOL_OP_LOWER_EQUAL, "<=" },
	{ SYMBOL_OP_LOWER, "<" },
	{ SYMBOL_OP_EQUAL, "==" },
	{ SYMBOL_OP_ASSIGN, "=" },
	{ SYMBOL_OP_UNEQUAL, "!=" },
	{ SYMBOL_OP_NOT, "!" },

	{ SYMBOL_OP_BITWISE_REVERSE, "~" },
};

// Returns the string representation of a Symbol token value (eg. SYMBOL_OP_ADD -> "+"), or NULL if not found.
static inline const char* Symbol_ToString(enum TOKEN_SYMBOL Symbol)
{
	static const int TableSize = sizeof(SYMBOL_TO_STRING_TABLE) / sizeof(struct SymbolToStringPair);
	for (int i = 0; i < TableSize; i++)
	{
		if (SYMBOL_TO_STRING_TABLE[i].Symbol == Symbol) return SYMBOL_TO_STRING_TABLE[i].String;
	}
	return NULL;
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

static inline ui8 Token_IsSymbol(struct Token* Token, enum TOKEN_SYMBOL SymbolMatch)
{
	ASSERT(Token != NULL);
	return Token->Type == TOKEN_SYMBOL && Token->Val.Symbol == SymbolMatch;
}

static inline ui8 Token_IsKeyword(struct Token* Token, enum TOKEN_KEYWORD KeywordMatch)
{
	ASSERT(Token != NULL);
	return Token->Type == TOKEN_KEYWORD && Token->Val.Keyword == KeywordMatch;
}

// Parser Stage

// All possible values for the type of an AST Node.
enum AST_NODE_TYPE
{
	AST_NODE_VARIABLE,				// Global, Local, Structure or Param Variable.
	AST_NODE_STRUCT,				// Structure definition or declaration.
	AST_NODE_ENUM,					// Enumeration definition or declaration.
	AST_NODE_FUNCTION,				// Function definition or declaration.
	AST_NODE_EXPRESSION,			// Expression with or without a compile-time result located inside instructions and variable definitions.
	AST_NODE_STATEMENT_EXP,			// A single statement node executing an expression tree.
	AST_NODE_STATEMENT_VAR,			// A single statement initializing a variable.
	AST_NODE_STATEMENT_CONTROL,		// A single statement executing a flow control keyword (return, break, continue...)
	AST_NODE_STATEMENT_BLOCK,		// Container statement for other statements.
	AST_NODE_STATEMENT_IF,			// Non-looping condition statement executing the next statement only if a condition expression returns > 0, or an else statement if specified.
	AST_NODE_STATEMENT_WHILE,		// Looping condition statement executing the next statement only if a condition expression returns > 0 and attempting re-entry.
	AST_NODE_STATEMENT_FOR,			// Looping condition similar to WHILE with specific Init and Post-Loop expression statements.
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

	EXP_VARIABLE, // Expression accesses a variable value.

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
					struct AST_Node* ExecStatement_Else;	// Statement to be executed on entry failure.
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
			ui8 IsArray;

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
			ui8 ParenthesisLevel; // How "deep" inside parenthesis this expression is located. 

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
					struct String_ANSI FunctionName;
					struct Vector Params; // Vector of sub-expressions corresponding to expected function parameters.
				} FunctionCall;
			};

		} Expression;

	} Val;
};

#endif // COMPILER_INCLUDED
