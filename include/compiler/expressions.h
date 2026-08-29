
#ifndef EXPRESSIONS_INCLUDED
#define EXPRESSIONS_INCLUDED

#include "core.h"
#include "tokens.h"
#include "type_signature.h"

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
	// The ultimate, untouchable Array Access special operator.
	{
		{
			SYMBOL_OP_ARRAY_ACCESS,
		}, 0 // So chained array accesses apply over the combination of all preceding accesses.
	},

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
			SYMBOL_OP_STRUCT_ACCESS,
			SYMBOL_OP_STRUCT_DEREF,
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

	// The lonely Comma, the Great Separator, the Eternal Splitter.
	{
		{
			SYMBOL_OP_COMMA,
		}, 1
	},

	// Ternary operators, the sort of inseparable top-level couple a C programmer can never find
	{
		{
			SYMBOL_OP_TERNARY_BRANCH,
			SYMBOL_OP_TERNARY_DELIM,
		}, 1
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
static inline i8 Symbol_CompareOpPrecedence(enum TOKEN_SYMBOL OpA, enum TOKEN_SYMBOL OpB)
{
	ui8 APrec, BPrec;
	Symbol_GetOpParseRules(OpA, &APrec, NULL);
	Symbol_GetOpParseRules(OpB, &BPrec, NULL);

	return APrec == BPrec ? 0 : (APrec < BPrec ? 1 : -1);
}

static inline ui8 Symbol_IsOpLeftToRightAssociative(enum TOKEN_SYMBOL Op)
{
	ui8 RightToLeft;
	Symbol_GetOpParseRules(Op, NULL, &RightToLeft);

	return RightToLeft == 0;
}

enum EXPRESSION_TYPE
{
	EXP_NOP,			// Expression is uninitialized or just does nothing and is only here as a "flagged spot" in execution.

	EXP_LITERAL_INT,	// Expression is a literal whole number.
	EXP_LITERAL_FLOAT,	// Expression is a literal floating-point number.
	EXP_LITERAL_DOUBLE, // Expression is a literal double-precision floating-point number.
	EXP_LITERAL_STRING, // Expression is a literal string.
	EXP_LITERAL_CHAR,	// Expression is a literal character.

	EXP_VAR_ACCESS,		// Expression accesses a variable value (for reading or writing).

	EXP_OP,				// Expression is a unary or binary operator applied over one or two operand sub-expressions located to either side.
	EXP_FUNC_CALL,		// Expression is a function call's return value.
	EXP_OP_SIZEOF,		// Special operator expression for sizeof.
	EXP_OP_CAST,		// Special operator expression for casts.
};

// Returns whether the passed type of expression is supposed to have sub-expressions.
static inline ui8 Expression_IsLeafType(enum EXPRESSION_TYPE Type)
{
	return Type < EXP_OP;
}

struct AST_Node; // Forward declaration so Pre-Integration expressions can reference AST Nodes.

// Expression tree structure combining operators and operands until reaching "leaf expressions".
// First parsed during Parsing, then retained within Integrated Program Tree to emit instructions from.
// Any reference to a AST_Node has to be replaced by something else over integration !
struct Expression
{
	struct TypeSignature* ResultType; // Expected return type for this expression.
	enum EXPRESSION_TYPE Type; // Type of expression.
	ui32 BufferLocation; // Location of the expression in its source buffer. NOTE: Not sure this really belongs here... Done for the AST_Node -> Expression refactor.

	ui8 ParenthesisLevel; // How "deep" inside parenthesis this expression is located. 

	union
	{
		struct
		{
			struct Expression* LeftOperand;
			struct Expression* RightOperand;
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

		// Special expression type referencing a AST Sub-node.
		// Will be resolved to a literal int node during integration.
		struct
		{
			struct Expression* Operand;	// Expression to resolve to get to a Type Signature we want the size of.
		} Sizeof;

		struct
		{
			struct Expression* Operand;
			struct TypeSignature* TypeSignature;
		} Cast;
	};

};

// Frees an expression node / tree.
// NOTE: Migrate this to a proper "Expressions" file independent from parser.
void FreeExpression(struct Expression* Expression);

static inline struct Expression* AllocExpression()
{
	struct Expression* New = calloc(1, sizeof(struct Expression));
	ASSERT(New != NULL);

	return New;
}

// Recursively prints expression to standard output.
// NOTE: Currently implemented in parser_logging.c because some expressions can refer to AST_Node structures.
// However, this is not ideal as later expressions will also carry Integration process data. We'll need variants of the same function
// depending on if the expression is printed in the context of an AST or an ProgramTree.
void PrintExpression(struct Expression* Expression, ui32 Depth);

#endif // EXPRESSIONS_INCLUDED
