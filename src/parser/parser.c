#include "parser.h"


// Helpers

// Returns the next Token to be read without advancing the internal reading cursor.
// Returns NULL when out of tokens.
static inline struct Token* Parser_PeekToken(struct ParserProcess* Parser)
{
	if (Parser->SourceTokens->Size <= Parser->TokenIndex) return NULL;

	return Vector_GetPtr(Parser->SourceTokens, Parser->TokenIndex);
}

// Returns the next Token to be read and advances the internal reading cursor.
// Returns NULL when out of tokens.
static inline struct Token* Parser_NextToken(struct ParserProcess* Parser)
{
	if (Parser->SourceTokens->Size <= Parser->TokenIndex) return NULL;

	return Vector_GetPtr(Parser->SourceTokens, Parser->TokenIndex++);
}

// Used to report buffer location when encountering unexpected EOF.
static inline ui32 Parser_GetLastTokenBufferLoc(struct ParserProcess* Parser)
{
	ASSERT(Parser->SourceTokens->Size > 0);

	return ((struct Token*)Vector_GetPtr(Parser->SourceTokens, Parser->SourceTokens->Size - 1))->BufferLocation;
}

static inline struct AST_Node* AllocNewNode(enum AST_NODE_TYPE NodeType)
{
	struct AST_Node* NewNode = calloc(1, sizeof(struct AST_Node));
	ASSERT(NewNode != NULL);

	NewNode->Type = NodeType;
	return NewNode;
}

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

#define POINTER_SIZE (_WIN64 ? 8 : 4)

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Void() 
{
	struct DatatypeDef Def = { 0 };
	Def.Size = 0;
	Def.Type = DATATYPE_VOID;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Char() 
{
	struct DatatypeDef Def = { 0 };
	Def.Size = 1;
	Def.Type = DATATYPE_CHAR;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Short()
{
	struct DatatypeDef Def = { 0 };
	Def.Size = 2;
	Def.Type = DATATYPE_SHORT;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Int32() 
{
	struct DatatypeDef Def = { 0 };
	Def.Size = 4;
	Def.Type = DATATYPE_INT32;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Int64() 
{
	struct DatatypeDef Def = { 0 };
	Def.Size = 8;
	Def.Type = DATATYPE_INT64;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Float() 
{
	struct DatatypeDef Def = { 0 };
	Def.Size = 4;
	Def.Type = DATATYPE_FLOAT;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Double() 
{
	struct DatatypeDef Def = { 0 };
	Def.Size = 8;
	Def.Type = DATATYPE_DOUBLE;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_String() 
{
	struct DatatypeDef Def = { 0 };
	Def.PointerLevel = 1;
	Def.Size = POINTER_SIZE;
	Def.Type = DATATYPE_CHAR;
	return Def;
}

// Attempts to parse the next few tokens into a DatatypeDef structure.
// AllowVoid determines whether non-pointer void type is considered valid.
static ui8 ParseDatatypeDef(struct ParserProcess* Parser, struct DatatypeDef* OutDatatypeDef,
	ui8 AllowVoid);

// Statement Parser functions.

static struct AST_Node* ParseBlockStatementNode(struct ParserProcess* Parser);
static struct AST_Node* ParseConditionalStatementNode(struct ParserProcess* Parser);
static struct AST_Node* ParseForStatementNode(struct ParserProcess* Parser);
static struct AST_Node* ParseSwitchStatementNode(struct ParserProcess* Parser);
static struct AST_Node* ParseExpressionStatementNode(struct ParserProcess* Parser, ui8 ParenthesisLevel);

static struct AST_Node* ParseStatementNode(struct ParserProcess* Parser);

// Root Parser functions

// Attempts to parse a new AST, covering a Global Variable symbol declaration and optionally its definition.
ui8 ParseGlobal_Variable(struct ParserProcess* Parser);

// Attempts to parse a new AST, covering a Function symbol declaration and optionally its definition.
ui8 ParseGlobal_Function(struct ParserProcess* Parser);

// Attempts to parse a new AST, covering a Struct declaration and optionally its definition.
ui8 ParseGlobal_Struct(struct ParserProcess* Parser);

// Error Handling

// Emits an Error into the ParserProcess structure. From there on the Parser Process should finish as soon as possible.
void Parser_Error(struct ParserProcess* Parser, ui32 BufferLoc, const char* Format, ...);

// Main Parser Process function. Turns the SourceTokens vector within the Process into a set of Abstract Syntax Trees (ASTs) whose roots will be put
// in the RootNodes vector in the Process.
void Parser_Run(struct ParserProcess* Parser)
{
	ASSERT(Parser->SourceTokens != NULL);
	ASSERT(Parser->RootNodes != NULL);

	while (Parser_PeekToken(Parser) != NULL)
	{
		// Attempt to parse AST Node trees in an arbitrary order.
		if (	ParseGlobal_Function(Parser)
			||	ParseGlobal_Struct(Parser)
			||	ParseGlobal_Variable(Parser))
		{
			// Successfully parsed node tree.
		}
		else
		{
			break;
		}
	}
}

void Parser_Error(struct ParserProcess* Parser, ui32 BufferLoc, const char* Format, ...)
{
	if (Parser->HasError) return; // For now this means we follow a "most specific error only" model.
	// Later we may want to bubble up the entire "Error hierarchy".

	Parser->HasError = 1;
	Parser->Error.Location = BufferLoc;

	va_list args;
	va_start(args, Format);
	Parser->Error.Message = String_CreateFormatV_ANSI(Format, args);
	va_end(args);
}


static ui8 ParseDatatypeDef(struct ParserProcess* Parser, struct DatatypeDef* OutDatatypeDef,
	ui8 AllowVoid)
{
	ui32 StartIndex = Parser->TokenIndex;

	// Zero out output immediately.
	memset(OutDatatypeDef, 0, sizeof(*OutDatatypeDef));

	struct Token* NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL)
	{
		goto PARSE_FAIL_EOF;
	}

	// Check value of first token. It needs to be a primitive type or one of the user defined type keywords (struct, union, enum).
	// TODO: Support using identifiers for typedef'd types.
	if (NextToken->Type != TOKEN_KEYWORD
		|| (!Keyword_IsPrimitiveType(NextToken->Val.Keyword)
			&& !Keyword_IsTypeSpecifier(NextToken->Val.Keyword)
			&& NextToken->Val.Keyword != KEYWORD_STRUCT
			&& NextToken->Val.Keyword != KEYWORD_UNION
			&& NextToken->Val.Keyword != KEYWORD_ENUM))
	{
		// Not a type.
		goto PARSE_FAIL;
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF when parsing data type.");
	PARSE_FAIL:
		Parser->TokenIndex = StartIndex;
		memset(OutDatatypeDef, 0, sizeof(*OutDatatypeDef));
		return 0;
	}

	enum DATATYPE_FLAGS Flags = 0;

	// Check for static-ness.
	if (NextToken->Val.Keyword == KEYWORD_STATIC)
	{
		Flags = DATATYPE_IS_STATIC;
		NextToken = Parser_NextToken(Parser);
		if (NextToken == NULL)
		{
			goto PARSE_FAIL_EOF;
		}
	}

	// Handle structured / enumerated types...
	if (NextToken->Val.Keyword == KEYWORD_STRUCT)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Struct parsing is unimplemented.");
		goto PARSE_FAIL;
	}
	if (NextToken->Val.Keyword == KEYWORD_UNION)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Union parsing is unimplemented.");
		goto PARSE_FAIL;
	}
	if (NextToken->Val.Keyword == KEYWORD_ENUM)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Enum parsing is unimplemented.");
		goto PARSE_FAIL;
	}

	// ... Otherwise we are parsing a primitive type.

	// Determine volatility.
	if (NextToken->Val.Keyword == KEYWORD_VOLATILE)
	{
		Flags |= DATATYPE_IS_VOLATILE;
		NextToken = Parser_NextToken(Parser);
		if (NextToken == NULL)
		{
			goto PARSE_FAIL_EOF;
		}
	}

	// Determine signage.
	Flags |= DATATYPE_IS_UNSIGNED * (NextToken->Val.Keyword == KEYWORD_UNSIGNED);
	ui8 SignKeywordPresent = 0;
	if (NextToken->Val.Keyword == KEYWORD_SIGNED
		|| NextToken->Val.Keyword == KEYWORD_UNSIGNED)
	{
		SignKeywordPresent = 1;
		NextToken = Parser_NextToken(Parser);
		if (NextToken == NULL)
		{
			goto PARSE_FAIL_EOF;
		}

		// If signage keyword is present and next token isn't a primitive type, then the signage alone represents a 32 bits integer.
		if (NextToken->Type != TOKEN_KEYWORD || !Keyword_IsPrimitiveType(NextToken->Val.Keyword))
		{
			*OutDatatypeDef = GetPrimitiveDatatypeDef_Int32();
		}
	}

	// Next determine size and broad type, if the next token is another keyword.
	if (NextToken->Type == TOKEN_KEYWORD)
	{
		switch (NextToken->Val.Keyword)
		{
		case KEYWORD_VOID:
			if (SignKeywordPresent)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier combination.");
				goto PARSE_FAIL;
			}
			
			*OutDatatypeDef = GetPrimitiveDatatypeDef_Void();
			break;
		case KEYWORD_CHAR:
			*OutDatatypeDef = GetPrimitiveDatatypeDef_Char();
			break;
		case KEYWORD_SHORT:
			*OutDatatypeDef = GetPrimitiveDatatypeDef_Short();
			break;
		case KEYWORD_INT:
			*OutDatatypeDef = GetPrimitiveDatatypeDef_Int32();
			break;
		case KEYWORD_LONG:
			*OutDatatypeDef = GetPrimitiveDatatypeDef_Int64();
			break;
		case KEYWORD_FLOAT:
			if (SignKeywordPresent)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier combination.");
				goto PARSE_FAIL;
			}
			*OutDatatypeDef = GetPrimitiveDatatypeDef_Float();
			break;
		case KEYWORD_DOUBLE:
			if (SignKeywordPresent)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier combination.");
				goto PARSE_FAIL;
			}
			*OutDatatypeDef = GetPrimitiveDatatypeDef_Double();
			break;
		default:
			Parser_Error(Parser, NextToken->BufferLocation, "Unexpected keyword.");
			goto PARSE_FAIL;
		}

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL)
		{
			goto PARSE_FAIL_EOF;
		}

		// Specifically in the case of LONG, move it back to 32 bits if only a single long keyword is present.
		if (OutDatatypeDef->Type == DATATYPE_INT64)
		{
			if (NextToken->Type != TOKEN_KEYWORD || NextToken->Val.Keyword != KEYWORD_LONG)
			{
				// Retrograde back to INT32. TODO: Handle long double.
				*OutDatatypeDef = GetPrimitiveDatatypeDef_Int32();
			}
			else
			{
				// Otherwise skip over the second long token.
				NextToken = Parser_NextToken(Parser);
			}
		}
	}

	// Parse pointer levels.
	while (Token_IsSymbol(NextToken, SYMBOL_STAR))
	{
		Parser_NextToken(Parser);
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL)
		{
			goto PARSE_FAIL_EOF;
		}

		OutDatatypeDef->PointerLevel++;
		OutDatatypeDef->Size = POINTER_SIZE;
	}

	// Check specific error case - non-pointer VOID type.
	if (!AllowVoid && OutDatatypeDef->Type == DATATYPE_VOID && OutDatatypeDef->PointerLevel == 0)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier combination.");
		goto PARSE_FAIL;
	}

	// Apply flags and return.
	OutDatatypeDef->Flags = Flags;

PARSE_SUCCESS:
	return 1;
}

static struct AST_Node* ParseBlockStatementNode(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* BlockNode = NULL;

	struct Token* NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing block.");
	PARSE_FAIL:
		Parser->TokenIndex = StartTokenIndex;
		if (BlockNode != NULL) free(BlockNode);
		return NULL;
	}

	if (!Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected '{' token.");
		goto PARSE_FAIL;
	}

	BlockNode = AllocNewNode(AST_NODE_STATEMENT_BLOCK);
	BlockNode->BufferLocation = NextToken->BufferLocation;
	BlockNode->Val.Statement.Block.Statements = Vector_Create(struct AST_Node*, 0);

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	if (!Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE))
	{
		// Parse instructions on loop. Any instruction other than sub-blocks should be separated by ';' tokens.

		struct AST_Node* NextInstruction = ParseStatementNode(Parser);
		while (NextInstruction != NULL)
		{
			Vector_Push(BlockNode->Val.Statement.Block.Statements, struct AST_Node*, NextInstruction);

			NextToken = Parser_PeekToken(Parser);
			if (NextToken == NULL) goto PARSE_FAIL_EOF;
			if (Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE)) break;

			NextInstruction = ParseStatementNode(Parser);
		}

		if (Parser->HasError)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Error while parsing instructions block.");
			goto PARSE_FAIL;
		}
	}

	NextToken = Parser_NextToken(Parser);

	if (!Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected '}' token");
		goto PARSE_FAIL;
	}

	return BlockNode;
}

static struct AST_Node* ParseConditionalStatementNode(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* InstructionNode = NULL;

	struct Token* NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing conditional instruction.");
	PARSE_FAIL:
		Parser->TokenIndex = StartTokenIndex;
		if (InstructionNode != NULL) free(InstructionNode);
		return NULL;
	}

	ui8 IsWhile = Token_IsKeyword(NextToken, KEYWORD_WHILE);
	if (	!IsWhile
		&&	!Token_IsKeyword(NextToken, KEYWORD_IF))
	{
		// Not a conditional block.
		goto PARSE_FAIL;
	}

	InstructionNode = AllocNewNode(IsWhile ? AST_NODE_STATEMENT_WHILE : AST_NODE_STATEMENT_IF);
	InstructionNode->BufferLocation = NextToken->BufferLocation;

	// Look for an opening parenthesis, a valid expression node, then a closing parenthesis.
	NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;
	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected '(' token.");
		goto PARSE_FAIL;
	}

	struct AST_Node* ConditionNode = ParseExpressionStatementNode(Parser, 0);
	if (ConditionNode == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse conditional block expression.");
		goto PARSE_FAIL;
	}

	if (IsWhile)
	{
		InstructionNode->Val.Statement.While.LoopCondition = ConditionNode;
	}
	else
	{
		InstructionNode->Val.Statement.If.EntryCondition = ConditionNode;
	}

	NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;
	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected ')' token.");
		goto PARSE_FAIL;
	}

	struct AST_Node* ExecNode = ParseStatementNode(Parser);
	if (ExecNode == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse conditional instruction.");
		goto PARSE_FAIL;
	}

	if (IsWhile)
	{
		InstructionNode->Val.Statement.While.ExecStatement = ExecNode;
	}
	else
	{
		InstructionNode->Val.Statement.If.ExecStatement = ExecNode;

		// Check for an else instruction.
		NextToken = Parser_PeekToken(Parser);
		if (Token_IsKeyword(NextToken, KEYWORD_ELSE))
		{
			Parser_NextToken(Parser);
			InstructionNode->Val.Statement.If.ExecInstruction_Else = ParseStatementNode(Parser);
			if (InstructionNode->Val.Statement.If.ExecInstruction_Else == NULL)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Error while parsing else instruction.");
				goto PARSE_FAIL;
			}
		}
	}

	return InstructionNode;
}

static struct AST_Node* ParseForStatementNode(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* InstructionNode = NULL;

	struct Token* NextToken = Parser_NextToken(Parser);
	if (!Token_IsKeyword(NextToken, KEYWORD_FOR))
	{
		// Not a for instruction.
		goto PARSE_FAIL;
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing For instruction.");
	PARSE_FAIL:
		Parser->TokenIndex = StartTokenIndex;
		if (InstructionNode != NULL) free(InstructionNode);
		return NULL;
	}

	// Look for an opening parenthesis, a valid expression node, then a closing parenthesis.
	NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;
	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected '(' token.");
		goto PARSE_FAIL;
	}

	InstructionNode = AllocNewNode(AST_NODE_STATEMENT_FOR);
	InstructionNode->BufferLocation = NextToken->BufferLocation;

	// First statement is initial and goes first in the instructions vector.
	InstructionNode->Val.Statement.For.InitExpression = ParseExpressionStatementNode(Parser, 0);

	NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;
	if (!Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected ';' token.");
		goto PARSE_FAIL;
	}

	InstructionNode->Val.Statement.For.LoopCondition = ParseExpressionStatementNode(Parser, 0);	

	NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;
	if (!Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected ';' token.");
		goto PARSE_FAIL;
	}

	InstructionNode->Val.Statement.For.PostLoopExpression = ParseExpressionStatementNode(Parser, 0);

	NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;
	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected ')' token.");
		goto PARSE_FAIL;
	}

	InstructionNode->Val.Statement.For.ExecStatement = ParseStatementNode(Parser);
	if (InstructionNode->Val.Statement.For.ExecStatement == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected instruction following FOR instruction.");
		goto PARSE_FAIL;
	}
	return InstructionNode;
}

static struct AST_Node* ParseSwitchStatementNode(struct ParserProcess* Parser)
{
	// TODO: Parse switch statement.
	// - Keyword check, expression.
	// - Braces.
	// - Go through every instruction in the block, ignore cases.
	// - Case / default keywords followed by a compile-time expression, : then link each to instruction.

	Parser_Error(Parser, Parser_PeekToken(Parser)->BufferLocation, "Switch instruction parsing not implemented.");
	return NULL;
}

static struct AST_Node* ParseExpressionStatementNode(struct ParserProcess* Parser, ui8 ParenthesisLevel)
{
	// Expression parsing: Recursive approach with "base cases" being:
	// - Literal values
	// - Identifiers
	// Outside those cases, the expression always (outside of function calls) defers to parsing:
	// - A left operand, if any.
	// - An operator.
	// - A right operand, if any.
	// The exact operator is determined with the corresponding symbol and sometimes with the presence / absence of a left / right operand.
	// In the case of function calls, we read a series of expressions separated by commas as parameters.

	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* ExpressionNode = NULL;

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing Expression statement.");
	PARSE_FAIL:
		Parser->TokenIndex = StartTokenIndex;
		if (ExpressionNode != NULL) free(ExpressionNode);
		return NULL;
	}

	ExpressionNode = AllocNewNode(AST_NODE_EXPRESSION);
	ExpressionNode->BufferLocation = NextToken->BufferLocation;

	// Handle base cases (Literals & Identifiers).
	if (NextToken->Type == TOKEN_LITERAL_CHAR)
	{
		ExpressionNode->Val.Expression.Type = EXP_LITERAL_CHAR;
		ExpressionNode->Val.Expression.Literal.Character = NextToken->Val.LiteralCharacter;

		ExpressionNode->Val.Expression.ResultType = GetPrimitiveDatatypeDef_Char();

		Parser_NextToken(Parser);
		return ExpressionNode;
	}
	if (NextToken->Type == TOKEN_LITERAL_STRING)
	{
		ExpressionNode->Val.Expression.Type = EXP_LITERAL_STRING;
		ExpressionNode->Val.Expression.Literal.String = NextToken->Val.LiteralString;

		ExpressionNode->Val.Expression.ResultType = GetPrimitiveDatatypeDef_String();

		Parser_NextToken(Parser);
		return ExpressionNode;
	}
	if (NextToken->Type == TOKEN_LITERAL_NUMBER_INT)
	{
		ExpressionNode->Val.Expression.Type = EXP_LITERAL_INT;
		ExpressionNode->Val.Expression.Literal.Integer = NextToken->Val.LiteralNumber.Integer;

		ExpressionNode->Val.Expression.ResultType = GetPrimitiveDatatypeDef_Int64();

		Parser_NextToken(Parser);
		return ExpressionNode;
	}
	if (NextToken->Type == TOKEN_LITERAL_NUMBER_FLOAT)
	{
		ExpressionNode->Val.Expression.Type = EXP_LITERAL_FLOAT;
		ExpressionNode->Val.Expression.Literal.FloatingPoint = NextToken->Val.LiteralNumber.Float;

		ExpressionNode->Val.Expression.ResultType = GetPrimitiveDatatypeDef_Float();

		Parser_NextToken(Parser);
		return ExpressionNode;
	}
	if (NextToken->Type == TOKEN_LITERAL_NUMBER_DOUBLE)
	{
		ExpressionNode->Val.Expression.Type = EXP_LITERAL_FLOAT;
		ExpressionNode->Val.Expression.Literal.FloatingPoint = NextToken->Val.LiteralNumber.Double;

		ExpressionNode->Val.Expression.ResultType = GetPrimitiveDatatypeDef_Double();

		Parser_NextToken(Parser);
		return ExpressionNode;
	}

	Parser_Error(Parser, NextToken->BufferLocation, "Non-literal expression parsing is unimplemented.");
	goto PARSE_FAIL;
}

static struct AST_Node* ParseStatementNode(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* InstructionNode = NULL;

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing block.");
	PARSE_FAIL:
		Parser->TokenIndex = StartTokenIndex;
		if (InstructionNode != NULL) free(InstructionNode);
		return NULL;
	}

	if (Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN))
	{
		InstructionNode = ParseBlockStatementNode(Parser);
	}
	else if (Token_IsKeyword(NextToken, KEYWORD_IF)
		|| Token_IsKeyword(NextToken, KEYWORD_WHILE))
	{
		InstructionNode = ParseConditionalStatementNode(Parser);
	}
	else if (Token_IsKeyword(NextToken, KEYWORD_FOR))
	{
		InstructionNode = ParseForStatementNode(Parser);
	}
	else if (Token_IsKeyword(NextToken, KEYWORD_SWITCH))
	{
		InstructionNode = ParseSwitchStatementNode(Parser);
	}
	else
	{
		InstructionNode = ParseExpressionStatementNode(Parser, 0);
	}

	if (InstructionNode == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse Statement node.");
		goto PARSE_FAIL;
	}

	return InstructionNode;
}

ui8 ParseGlobal_Variable(struct ParserProcess* Parser)
{
	// UNIMPLEMENTED.
	return 0;
}

ui8 ParseGlobal_Function(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* FunctionNode = NULL;

	// Attempt to parse a specific sequence:
	// - A return type
	// - An identifier
	// - Open parenthesis symbol
	// - 0 or more comma-separated variable declarations
	// - Closing parenthesis symbol
	// - Either a semicolon or a block

	struct DatatypeDef ReturnType;
	if (!ParseDatatypeDef(Parser, &ReturnType, 1))
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing function.");
	PARSE_FAIL:
		Parser->TokenIndex = StartTokenIndex;
		if (FunctionNode != NULL) free(FunctionNode);
		return 0;
	}
	struct Token* NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	struct Token* IdentifierToken = NextToken;
	if (IdentifierToken->Type != TOKEN_IDENTIFIER)
	{
		goto PARSE_FAIL;
	}

	NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{
		goto PARSE_FAIL;
	}

	// At this point this MUST be a function, so any failure is an error case.
	FunctionNode = AllocNewNode(AST_NODE_FUNCTION);
	FunctionNode->BufferLocation = IdentifierToken->BufferLocation;

	FunctionNode->Val.Function.Name = IdentifierToken->Val.Identifier;
	FunctionNode->Val.Function.ReturnType = ReturnType;
	FunctionNode->Val.Function.Params = Vector_Create(struct AST_Node*, 0);


	// Look for parameters.

	struct DatatypeDef ParamType;
	while (ParseDatatypeDef(Parser, &ParamType, 0))
	{
		NextToken = Parser_NextToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
		if (NextToken->Type != TOKEN_IDENTIFIER)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Unexpected token while parsing function parameter.");
			goto PARSE_FAIL;
		}

		struct AST_Node* ParamNode = AllocNewNode(AST_NODE_VARIABLE);
		ParamNode->BufferLocation = NextToken->BufferLocation;
		ParamNode->Val.Variable.Name = NextToken->Val.Identifier;
		ParamNode->Val.Variable.Type = ParamType;

		Vector_Push(FunctionNode->Val.Function.Params, struct AST_Node*, ParamNode);
		
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
		if (Token_IsSymbol(NextToken, SYMBOL_COMMA))
		{
			// Parse next param...
			NextToken = Parser_NextToken(Parser);
			continue;
		}

		// Done parsing params.
		break;
	}

	NextToken = Parser_NextToken(Parser);

	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected closing parenthesis when parsing function prototype.");
		goto PARSE_FAIL;
	}

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;
	if (Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		// Parse as declaration. Nothing else to be done other than consuming the token.
		NextToken = Parser_NextToken(Parser);
	}
	else if (Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN))
	{
		// Parse as definition.
		// First parse a whole block, then go through it and collect any internal variable declarations
		FunctionNode->Val.Function.Statements = ParseBlockStatementNode(Parser);
	}
	else
	{
		// Unexpected symbol.
		Parser_Error(Parser, NextToken->BufferLocation, "Unexpected token when parsing function.");
		goto PARSE_FAIL;
	}

	// Parse successful. Add to parser output.
	Vector_Push(*Parser->RootNodes, struct AST_Node*, FunctionNode);
	return 1;
}

ui8 ParseGlobal_Struct(struct ParserProcess* Parser)
{
	// UNIMPLEMENTED.
	return 0;
}
