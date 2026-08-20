#include "parser.h"

#include "expression_parsing.c"

// Helpers

// Returns the next Token to be read without advancing the internal reading cursor.
// Returns NULL when out of tokens.
struct Token* Parser_PeekToken(struct ParserProcess* Parser)
{
	if (Parser->SourceTokens->Size <= Parser->TokenIndex) return NULL;

	return Vector_GetPtr(Parser->SourceTokens, Parser->TokenIndex);
}

// Returns the next Token to be read and advances the internal reading cursor.
// Returns NULL when out of tokens.
struct Token* Parser_NextToken(struct ParserProcess* Parser)
{
	if (Parser->SourceTokens->Size <= Parser->TokenIndex) return NULL;

	return Vector_GetPtr(Parser->SourceTokens, Parser->TokenIndex++);
}

// Used to report buffer location when encountering unexpected EOF.
ui32 Parser_GetLastTokenBufferLoc(struct ParserProcess* Parser)
{
	ASSERT(Parser->SourceTokens->Size > 0);

	return ((struct Token*)Vector_GetPtr(Parser->SourceTokens, Parser->SourceTokens->Size - 1))->BufferLocation;
}

struct AST_Node* AllocNewNode(enum AST_NODE_TYPE NodeType)
{
	struct AST_Node* NewNode = calloc(1, sizeof(struct AST_Node));
	ASSERT(NewNode != NULL);

	NewNode->Type = NodeType;
	return NewNode;
}

// Frees a node and its children recursively.
void FreeNode(struct AST_Node* Node)
{
	if (Node == NULL) return;

	switch (Node->Type)
	{
	default:
		break;
	case AST_NODE_EXPRESSION:
		if (Node->Val.Expression.Type == EXP_OP)
		{
			FreeNode(Node->Val.Expression.Op.LeftOperand);
			FreeNode(Node->Val.Expression.Op.RightOperand);
		}
		else if (Node->Val.Expression.Type == EXP_VARIABLE)
		{
			String_Free_ANSI(&Node->Val.Expression.Variable.Name);
		}
		else if (Node->Val.Expression.Type == EXP_FUNCTION_CALL)
		{
			String_Free_ANSI(&Node->Val.Expression.FunctionCall.FunctionName);
			FreeNodeVector(&Node->Val.Expression.FunctionCall.Params);
		}
		else if (Node->Val.Expression.Type == EXP_LITERAL_STRING)
		{
			String_Free_ANSI(&Node->Val.Expression.Literal.String);
		}
		break;
	case AST_NODE_VARIABLE:
		FreeNode(Node->Val.Variable.Value);
		String_Free_ANSI(&Node->Val.Variable.Name);
		break;
	case AST_NODE_FUNCTION:
		String_Free_ANSI(&Node->Val.Function.Name);
		FreeNodeVector(&Node->Val.Function.Params);
		FreeNode(Node->Val.Function.Statements);
		break;
	case AST_NODE_STRUCT:
		FreeNodeVector(&Node->Val.Struct.Members);
		break;
	case AST_NODE_STATEMENT_VAR:
		FreeNode(Node->Val.Statement.Variable);
		break;
	case AST_NODE_STATEMENT_EXP:
		FreeNode(Node->Val.Statement.Expression);
		break;
	case AST_NODE_STATEMENT_CONTROL:
		FreeNode(Node->Val.Statement.Control);
		break;
	case AST_NODE_STATEMENT_IF:
		FreeNode(Node->Val.Statement.If.EntryCondition);
		FreeNode(Node->Val.Statement.If.ExecStatement);
		FreeNode(Node->Val.Statement.If.ExecStatement_Else);
		break;
	case AST_NODE_STATEMENT_WHILE:
		FreeNode(Node->Val.Statement.While.EntryCondition);
		FreeNode(Node->Val.Statement.While.ExecStatement);
		FreeNode(Node->Val.Statement.While.LoopCondition);
		break;
	case AST_NODE_STATEMENT_FOR:
		FreeNode(Node->Val.Statement.For.InitExpression);
		FreeNode(Node->Val.Statement.For.LoopCondition);
		FreeNode(Node->Val.Statement.For.PostLoopExpression);
		FreeNode(Node->Val.Statement.For.ExecStatement);
		break;
	case AST_NODE_STATEMENT_BLOCK:
		FreeNodeVector(&Node->Val.Statement.Block.Statements);
		break;
	}

	free(Node);
}

void FreeNodeVector(struct Vector* NodeVec)
{
	ASSERT(NodeVec != NULL);

	for (int i = 0; i < NodeVec->Size; i++)
	{
		FreeNode(Vector_GetValueAt(*NodeVec, struct AST_Node*, i));
	}

	Vector_Destroy(NodeVec);
}

// AST Printing

static void PrintNode(struct AST_Node* Node, ui32 Depth);

static void PrintIndent(ui32 Depth)
{
	for (ui32 i = 0; i < Depth; i++) printf("  ");
}

// Prints an Expression node's specific data and, for operator / function call expressions, recurses into its sub-expressions.
static void PrintExpressionNode(struct AST_Node* Node, ui32 Depth)
{
	switch (Node->Val.Expression.Type)
	{
	case EXP_LITERAL_INT:
		printf("<LITERAL_INT: %lld>\n", Node->Val.Expression.Literal.Integer);
		break;
	case EXP_LITERAL_FLOAT:
		printf("<LITERAL_FLOAT: %f>\n", Node->Val.Expression.Literal.Float);
		break;
	case EXP_LITERAL_DOUBLE:
		printf("<LITERAL_DOUBLE: %lf>\n", Node->Val.Expression.Literal.Double);
		break;
	case EXP_LITERAL_STRING:
		printf("<LITERAL_STRING: \"%s\">\n", Node->Val.Expression.Literal.String.Str);
		break;
	case EXP_LITERAL_CHAR:
		printf("<LITERAL_CHAR: '%c'>\n", Node->Val.Expression.Literal.Character);
		break;
	case EXP_VARIABLE:
		printf("<VAR_READ: '%s'>\n", Node->Val.Expression.Variable.Name.Str);
		break;
	case EXP_OP:
		printf("<OP: '%s'>\n", Symbol_ToString(Node->Val.Expression.Op.OperatorSymbol));
		PrintNode(Node->Val.Expression.Op.LeftOperand, Depth + 1);
		PrintNode(Node->Val.Expression.Op.RightOperand, Depth + 1);
		break;
	case EXP_FUNCTION_CALL:
		printf("<FUNCTION_CALL: '%s'>\n", Node->Val.Expression.FunctionCall.FunctionName.Str);
		for (int i = 0; i < Node->Val.Expression.FunctionCall.Params.Size; i++)
			PrintNode(Vector_GetValueAt(Node->Val.Expression.FunctionCall.Params, struct AST_Node*, i), Depth + 1);
		break;
	}
}

// Prints a single AST node and recurses into its children, indenting each depth level by two spaces.
static void PrintNode(struct AST_Node* Node, ui32 Depth)
{
	if (Node == NULL) return;

	PrintIndent(Depth);

	switch (Node->Type)
	{
	case AST_NODE_VARIABLE:
		printf("<VARIABLE: '%s'>\n", Node->Val.Variable.Name.Str);
		PrintNode(Node->Val.Variable.Value, Depth + 1);
		break;
	case AST_NODE_STRUCT:
		printf("<STRUCT>\n");
		for (int i = 0; i < Node->Val.Struct.Members.Size; i++)
			PrintNode(Vector_GetValueAt(Node->Val.Struct.Members, struct AST_Node*, i), Depth + 1);
		break;
	case AST_NODE_ENUM:
		printf("<ENUM>\n");
		break;
	case AST_NODE_FUNCTION:
		printf("<FUNCTION: '%s'>\n", Node->Val.Function.Name.Str);
		for (int i = 0; i < Node->Val.Function.Params.Size; i++)
			PrintNode(Vector_GetValueAt(Node->Val.Function.Params, struct AST_Node*, i), Depth + 1);
		PrintNode(Node->Val.Function.Statements, Depth + 1);
		break;
	case AST_NODE_EXPRESSION:
		PrintExpressionNode(Node, Depth);
		break;
	case AST_NODE_STATEMENT_EXP:
		printf("<STATEMENT_EXP>\n");
		PrintNode(Node->Val.Statement.Expression, Depth + 1);
		break;
	case AST_NODE_STATEMENT_VAR:
		printf("<STATEMENT_VAR>\n");
		PrintNode(Node->Val.Statement.Variable, Depth + 1);
		break;
	case AST_NODE_STATEMENT_CONTROL:
		printf("<STATEMENT_CONTROL>\n");
		PrintNode(Node->Val.Statement.Control, Depth + 1);
		break;
	case AST_NODE_STATEMENT_BLOCK:
		printf("<BLOCK>\n");
		for (int i = 0; i < Node->Val.Statement.Block.Statements.Size; i++)
			PrintNode(Vector_GetValueAt(Node->Val.Statement.Block.Statements, struct AST_Node*, i), Depth + 1);
		break;
	case AST_NODE_STATEMENT_IF:
		printf("<IF>\n");
		PrintNode(Node->Val.Statement.If.EntryCondition, Depth + 1);
		PrintNode(Node->Val.Statement.If.ExecStatement, Depth + 1);
		PrintNode(Node->Val.Statement.If.ExecStatement_Else, Depth + 1);
		break;
	case AST_NODE_STATEMENT_WHILE:
		printf("<WHILE>\n");
		PrintNode(Node->Val.Statement.While.EntryCondition, Depth + 1);
		PrintNode(Node->Val.Statement.While.LoopCondition, Depth + 1);
		PrintNode(Node->Val.Statement.While.ExecStatement, Depth + 1);
		break;
	case AST_NODE_STATEMENT_FOR:
		printf("<FOR>\n");
		PrintNode(Node->Val.Statement.For.InitExpression, Depth + 1);
		PrintNode(Node->Val.Statement.For.LoopCondition, Depth + 1);
		PrintNode(Node->Val.Statement.For.PostLoopExpression, Depth + 1);
		PrintNode(Node->Val.Statement.For.ExecStatement, Depth + 1);
		break;
	}
}

void Parser_PrintTree(struct ParserProcess* Parser)
{
	ASSERT(Parser != NULL);
	ASSERT(Parser->RootNodes != NULL);

	for (int i = 0; i < Parser->RootNodes->Size; i++)
	{
		PrintNode(Vector_GetValueAt(*Parser->RootNodes, struct AST_Node*, i), 0);
	}
}

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
		Flags |= DATATYPE_IS_STATIC;
		NextToken = Parser_NextToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

	// Check for const-ness. (TODO: Check in any order with static, volatile and other specifiers...)
	if (NextToken->Val.Keyword == KEYWORD_CONST)
	{
		Flags |= DATATYPE_IS_CONST;
		NextToken = Parser_NextToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
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

struct AST_Node* ParseBlockStatementNode(struct ParserProcess* Parser)
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
		if (BlockNode != NULL) FreeNode(BlockNode);
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

struct AST_Node* ParseConditionalStatementNode(struct ParserProcess* Parser)
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
		if (InstructionNode != NULL) FreeNode(InstructionNode);
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

	// Parse the condition expression, specifically placing it in a parenthesis scope.
	NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;
	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected '(' token.");
		goto PARSE_FAIL;
	}

	// Parse condition expression until matching closing parenthesis.
	struct AST_Node* ConditionNode = ParseExpressionNode(Parser, SYMBOL_PARENTHESIS_CLOSE);
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
			InstructionNode->Val.Statement.If.ExecStatement_Else = ParseStatementNode(Parser);
			if (InstructionNode->Val.Statement.If.ExecStatement_Else == NULL)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Error while parsing else instruction.");
				goto PARSE_FAIL;
			}
		}
	}

	return InstructionNode;
}

struct AST_Node* ParseForStatementNode(struct ParserProcess* Parser)
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
		if (InstructionNode != NULL) FreeNode(InstructionNode);
		return NULL;
	}

	// Enclose the three expressions into a parenthesis scope. The first two are parsed up until encountering a semicolon, the third the closing parenthesis.
	NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;
	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected '(' token.");
		goto PARSE_FAIL;
	}

	InstructionNode = AllocNewNode(AST_NODE_STATEMENT_FOR);
	InstructionNode->BufferLocation = NextToken->BufferLocation;

	// Parse init, condition and post-loop expressions.
	InstructionNode->Val.Statement.For.InitExpression = ParseExpressionNode(Parser, SYMBOL_SEMICOLON);
	if (Parser->HasError) goto PARSE_FAIL;
	InstructionNode->Val.Statement.For.LoopCondition = ParseExpressionNode(Parser, SYMBOL_SEMICOLON);	
	if (Parser->HasError) goto PARSE_FAIL;
	InstructionNode->Val.Statement.For.PostLoopExpression = ParseExpressionNode(Parser, SYMBOL_PARENTHESIS_CLOSE);
	if (Parser->HasError) goto PARSE_FAIL;

	InstructionNode->Val.Statement.For.ExecStatement = ParseStatementNode(Parser);
	if (InstructionNode->Val.Statement.For.ExecStatement == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected instruction following FOR instruction.");
		goto PARSE_FAIL;
	}
	return InstructionNode;
}

struct AST_Node* ParseSwitchStatementNode(struct ParserProcess* Parser)
{
	// TODO: Parse switch statement.
	// - Keyword check, expression.
	// - Braces.
	// - Go through every instruction in the block, ignore cases.
	// - Case / default keywords followed by a compile-time expression, : then link each to instruction.

	Parser_Error(Parser, Parser_PeekToken(Parser)->BufferLocation, "Switch instruction parsing not implemented.");
	return NULL;
}

struct AST_Node* ParseStatementNode(struct ParserProcess* Parser)
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
		if (InstructionNode != NULL) FreeNode(InstructionNode);
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
		InstructionNode = ParseExpressionNode(Parser, SYMBOL_SEMICOLON);
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
	int TokenStartIndex = Parser->TokenIndex;

	struct AST_Node* VarNode = NULL;

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing variable.");
	PARSE_FAIL:
		if (VarNode != NULL) FreeNode(VarNode);
		return 0;
	}

	VarNode = AllocNewNode(AST_NODE_VARIABLE);
	VarNode->BufferLocation = NextToken->BufferLocation;

	if (!ParseDatatypeDef(Parser, &VarNode->Val.Variable.Type, 0))
	{
		goto PARSE_FAIL;
	}

	NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	if (NextToken->Type != TOKEN_IDENTIFIER)
	{
		goto PARSE_FAIL;
	}

	VarNode->Val.Variable.Name = String_Copy_ANSI(NextToken->Val.Identifier);
	
	NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	if (Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		// Parse as simple declaration, which requires nothing else.
	}
	else if (Token_IsSymbol(NextToken, SYMBOL_OP_ASSIGN))
	{
		// Parse variable value definition.
		VarNode->Val.Variable.Value = ParseExpressionNode(Parser, SYMBOL_SEMICOLON);

	}
	else if (Token_IsSymbol(NextToken, SYMBOL_BRACKET_OPEN))
	{
		// Parse as array definition.

		VarNode->Val.Variable.Value = ParseExpressionNode(Parser, SYMBOL_BRACKET_CLOSE);
		VarNode->Val.Variable.IsArray = 1;

		goto PARSE_FAIL;
	}

	Vector_Push(*Parser->RootNodes, struct AST_Node*, VarNode);
	return 1;
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
		if (FunctionNode != NULL) FreeNode(FunctionNode);
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

	FunctionNode->Val.Function.Name = String_Copy_ANSI(IdentifierToken->Val.Identifier);
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
		ParamNode->Val.Variable.Name = String_Copy_ANSI(NextToken->Val.Identifier);
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
