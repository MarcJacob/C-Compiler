
#ifndef TOKENS_INCLUDED
#define TOKENS_INCLUDED

#include "core.h"

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
	SYMBOL_AMB_COLON,				// :
	SYMBOL_PARENTHESIS_OPEN,		// (
	SYMBOL_PARENTHESIS_CLOSE,		// )
	SYMBOL_BRACKET_OPEN,			// [
	SYMBOL_BRACKET_CLOSE,			// ]
	SYMBOL_BRACE_OPEN,				// {
	SYMBOL_BRACE_CLOSE,				// }

	// Operators

	SYMBOL_OP_TERNARY_BRANCH,		// ?
	SYMBOL_OP_TERNARY_DELIM,		// : (De-ambiguated during the parsing process)
	SYMBOL_OP_BITCOUNT_ASSIGN,		// : (De-ambiguated during the parsing process)
	SYMBOL_OP_COMMA,				// ,
	SYMBOL_OP_ARRAY_ACCESS,			// [...] (Special operator constructed entirely during parsing.
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
	SYMBOL_OP_STRUCT_ACCESS,		// .
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
	case SYMBOL_OP_TERNARY_BRANCH:
	case SYMBOL_OP_TERNARY_DELIM:
	case SYMBOL_OP_ARRAY_ACCESS: // Special operator - Left operand is accessed address, right operand is index expression. Uses dedicated parsing function.
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
	case SYMBOL_OP_STRUCT_ACCESS:
	case SYMBOL_OP_COMMA:
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
	case SYMBOL_OP_AMB_STAR:
		return SYMBOL_OP_DEREF;
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
	{ SYMBOL_OP_COMMA, "," },
	{ SYMBOL_AMB_COLON, ":" },
	{ SYMBOL_PARENTHESIS_OPEN, "(" },
	{ SYMBOL_PARENTHESIS_CLOSE, ")" },
	{ SYMBOL_BRACKET_OPEN, "[" },
	{ SYMBOL_BRACKET_CLOSE, "]" },
	{ SYMBOL_BRACE_OPEN, "{" },
	{ SYMBOL_BRACE_CLOSE, "}" },

	{ SYMBOL_OP_TERNARY_BRANCH, "?" },
	{ SYMBOL_OP_TERNARY_DELIM, ":" },
	{ SYMBOL_OP_ARRAY_ACCESS, "index" },
	{ SYMBOL_OP_STRUCT_ACCESS, "." },
	{ SYMBOL_OP_STRUCT_DEREF, "->" },
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
	KEYWORD_EXTERN,
	KEYWORD_SIGNED,
	KEYWORD_UNSIGNED,
	KEYWORD_CONST,
	KEYWORD_VOLATILE,
	KEYWORD_STRUCT,
	KEYWORD_ENUM,
	KEYWORD_UNION,
	KEYWORD_TYPEDEF,

	// Special Statements 
	KEYWORD_IF,
	KEYWORD_ELSE,
	KEYWORD_FOR,
	KEYWORD_DO,
	KEYWORD_WHILE,
	KEYWORD_SWITCH,

	// Control keywords
	KEYWORD_CASE,
	KEYWORD_BREAK,
	KEYWORD_CONTINUE,
	KEYWORD_RETURN,
	KEYWORD_GOTO,

	// Special operators 
	KEYWORD_SIZEOF, // Converted to SYMBOL_OP_SIZEOF during parsing.
};

struct KeywordToStringPair
{
	enum TOKEN_KEYWORD Keyword;
	const char* String;
};

// Case-sensitive matching table for keyword token values and their source string equivalent.
static const struct KeywordToStringPair KEYWORD_TO_STRING_TABLE[] =
{
	{ KEYWORD_VOID, "void" },
	{ KEYWORD_CHAR, "char" },
	{ KEYWORD_SHORT, "short" },
	{ KEYWORD_INT, "int" },
	{ KEYWORD_FLOAT, "float" },
	{ KEYWORD_LONG, "long" },
	{ KEYWORD_DOUBLE, "double" },

	{ KEYWORD_STATIC, "static" },
	{ KEYWORD_EXTERN, "extern" },
	{ KEYWORD_SIGNED, "signed" },
	{ KEYWORD_UNSIGNED, "unsigned" },
	{ KEYWORD_CONST, "const" },
	{ KEYWORD_VOLATILE, "volatile" },
	{ KEYWORD_STRUCT, "struct" },
	{ KEYWORD_ENUM, "enum" },
	{ KEYWORD_UNION, "union" },
	{ KEYWORD_TYPEDEF, "typedef" },

	{ KEYWORD_IF, "if" },
	{ KEYWORD_ELSE, "else" },
	{ KEYWORD_FOR, "for" },
	{ KEYWORD_DO, "do" },
	{ KEYWORD_WHILE, "while" },
	{ KEYWORD_SWITCH, "switch" },

	{ KEYWORD_CASE, "case" },
	{ KEYWORD_BREAK, "break" },
	{ KEYWORD_CONTINUE, "continue" },
	{ KEYWORD_RETURN, "return" },
	{ KEYWORD_GOTO, "goto" },

	{ KEYWORD_SIZEOF, "sizeof" },
};

// Returns the string representation of a Keyword token value (eg. KEYWORD_RETURN -> "return"), or NULL if not found.
static inline const char* Keyword_ToString(enum TOKEN_KEYWORD Keyword)
{
	static const int TableSize = sizeof(KEYWORD_TO_STRING_TABLE) / sizeof(struct KeywordToStringPair);
	for (int i = 0; i < TableSize; i++)
	{
		if (KEYWORD_TO_STRING_TABLE[i].Keyword == Keyword) return KEYWORD_TO_STRING_TABLE[i].String;
	}
	return NULL;
}

#define IDENTIFIER_MAX_LENGTH (128)

struct Token
{
	enum TOKEN_TYPE Type; // Type of token this is.
	ui32 BufferLocation; // Location of token in character index.

	// Main Value union, giving type-specific information about the token.
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
	};
};

static inline ui8 Token_IsSymbol(struct Token* Token, enum TOKEN_SYMBOL SymbolMatch)
{
	ASSERT(Token != NULL);
	return Token->Type == TOKEN_SYMBOL && Token->Symbol == SymbolMatch;
}

static inline ui8 Token_IsKeyword(struct Token* Token, enum TOKEN_KEYWORD KeywordMatch)
{
	ASSERT(Token != NULL);
	return Token->Type == TOKEN_KEYWORD && Token->Keyword == KeywordMatch;
}

#endif // TOKENS_INCLUDED
