#include "parser.h"

#include "expression_parsing.c"

// Helpers

// Returns the next Token without advancing the internal reading cursor.
// Returns NULL when out of tokens.
struct Token* Parser_PeekToken(struct ParserProcess* Parser)
{
	if (Parser->SourceTokens->Size <= Parser->TokenIndex) return NULL;

	return Vector_GetPtr(Parser->SourceTokens, Parser->TokenIndex);
}

// Returns the next token and advances the internal reading cursor past it.
// Returns NULL when out of tokens.
struct Token* Parser_ConsumeToken(struct ParserProcess* Parser)
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
		else if (Node->Val.Expression.Type == EXP_VAR_ACCESS)
		{
			String_Free_ANSI(&Node->Val.Expression.Variable.Name);
		}
		else if (Node->Val.Expression.Type == EXP_FUNC_CALL)
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
		FreeDatatypeDef(&Node->Val.Variable.Type);
		String_Free_ANSI(&Node->Val.Variable.Name);
		break;
	case AST_NODE_FUNCTION:
		String_Free_ANSI(&Node->Val.Function.Name);
		FreeNodeVector(&Node->Val.Function.Params);
		FreeNode(Node->Val.Function.Statements);
		FreeDatatypeDef(&Node->Val.Function.ReturnType);
		break;
	case AST_NODE_STRUCT:
		String_Free_ANSI(&Node->Val.Struct.Type.TypeName);
		FreeNodeVector(&Node->Val.Struct.Members);
		break;
	case AST_NODE_STATEMENT_EXP:
		FreeNode(Node->Val.Statement.Expression);
		break;
	case AST_NODE_STATEMENT_CONTROL:
		FreeNode(Node->Val.Statement.Control.Expression);
		// Don't free the Target StatementNode since it's not "owned" by this node.
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
	case AST_NODE_STATEMENT_VAR_DEC:
		FreeDatatypeDef(&Node->Val.Statement.VarDeclaration.Type);
		FreeNodeVector(&Node->Val.Statement.VarDeclaration.VarExpressions);
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

// Prints a datatype's name followed by one '*' character per pointer level (eg. "int**" for a pointer to pointer to int).
static void PrintDatatypeName(const struct DatatypeDef* Datatype)
{
	printf("%s", Datatype_GetName(Datatype));
	for (ui8 i = 0; i < Datatype->PointerLevel; i++) printf("*");
}

// Prints a named header for a sub-node of a complex statement (e.g. an IF's CONDITION / THEN / ELSE) before printing the node itself one level deeper.
// Skipped entirely if Node is NULL, so optional sub-nodes don't leave a dangling, empty-looking header.
static void PrintLabeledNode(const char* Label, struct AST_Node* Node, ui32 Depth)
{
	if (Node == NULL) return;

	PrintIndent(Depth);
	printf("[%s]\n", Label);
	PrintNode(Node, Depth + 1);
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
	case EXP_VAR_ACCESS:
		printf("<VAR_ACCESS: '%s' : ", Node->Val.Expression.Variable.Name.Str);
		PrintDatatypeName(&Node->Val.Expression.ResultType);
		printf(">\n");
		break;
	case EXP_OP:
		printf("<OP: '%s'>\n", Symbol_ToString(Node->Val.Expression.Op.OperatorSymbol));
		PrintNode(Node->Val.Expression.Op.LeftOperand, Depth + 1);
		PrintNode(Node->Val.Expression.Op.RightOperand, Depth + 1);
		break;
	case EXP_FUNC_CALL:
		printf("<FUNCTION_CALL: '%s' : ", Node->Val.Expression.FunctionCall.FunctionName.Str);
		PrintDatatypeName(&Node->Val.Expression.ResultType);
		printf(">\n");
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
	case AST_NODE_STRUCT:
		printf("<STRUCT: '%s'>\n", Node->Val.Struct.Type.TypeName.Str);
		for (int i = 0; i < Node->Val.Struct.Members.Size; i++)
			PrintNode(Vector_GetValueAt(Node->Val.Struct.Members, struct AST_Node*, i), Depth + 1);
		break;
	case AST_NODE_ENUM:
		printf("<ENUM>\n");
		break;
	case AST_NODE_FUNCTION:
		printf("<FUNCTION: '%s' : ", Node->Val.Function.Name.Str);
		PrintDatatypeName(&Node->Val.Function.ReturnType);
		printf(">\n");
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
	case AST_NODE_STATEMENT_CONTROL:
		printf("<%s>\n", Keyword_ToString(Node->Val.Statement.Control.Keyword));
		PrintNode(Node->Val.Statement.Control.Expression, Depth + 1);
		break;
	case AST_NODE_STATEMENT_BLOCK:
		if (Node->Val.Statement.Block.Statements.Size > 0)
		{
			printf("<BLOCK>\n");
			for (int i = 0; i < Node->Val.Statement.Block.Statements.Size; i++)
				PrintNode(Vector_GetValueAt(Node->Val.Statement.Block.Statements, struct AST_Node*, i), Depth + 1);
			PrintIndent(Depth);
			printf("</BLOCK>\n");
		}
		else
		{
			printf("<EMPTY BLOCK>\n");
		}
		break;
	case AST_NODE_STATEMENT_IF:
		printf("<IF>\n");
		PrintLabeledNode("CONDITION", Node->Val.Statement.If.EntryCondition, Depth + 1);
		PrintLabeledNode("THEN", Node->Val.Statement.If.ExecStatement, Depth + 1);
		PrintLabeledNode("ELSE", Node->Val.Statement.If.ExecStatement_Else, Depth + 1);
		break;
	case AST_NODE_STATEMENT_WHILE:
		printf("<WHILE>\n");
		PrintLabeledNode("ENTRY_CONDITION", Node->Val.Statement.While.EntryCondition, Depth + 1);
		PrintLabeledNode("LOOP_CONDITION", Node->Val.Statement.While.LoopCondition, Depth + 1);
		PrintLabeledNode("BODY", Node->Val.Statement.While.ExecStatement, Depth + 1);
		break;
	case AST_NODE_STATEMENT_FOR:
		printf("<FOR>\n");
		PrintLabeledNode("INIT", Node->Val.Statement.For.InitExpression, Depth + 1);
		PrintLabeledNode("CONDITION", Node->Val.Statement.For.LoopCondition, Depth + 1);
		PrintLabeledNode("POST", Node->Val.Statement.For.PostLoopExpression, Depth + 1);
		PrintLabeledNode("BODY", Node->Val.Statement.For.ExecStatement, Depth + 1);
		break;
	default:
		printf("<UNKNOWN NODE TYPE>\n");
		break;
	}
}

void Parser_PrintTree(struct ParserProcess* Parser)
{
	ASSERT(Parser != NULL);
	ASSERT(Parser->RootNodes != NULL);

	printf("\n===== PARSER OUTPUT =====\n\n");

	for (int i = 0; i < Parser->RootNodes->Size; i++)
	{
		PrintNode(Vector_GetValueAt(*Parser->RootNodes, struct AST_Node*, i), 0);
	}
}

// Root Parser functions

// Attempts to parse a new AST, covering a Global Variable symbol declaration and optionally its definition.
ui8 ParseGlobal_VarDeclarationStatement(struct ParserProcess* Parser);

// Attempts to parse a new AST, covering a Function symbol declaration and optionally its definition.
ui8 ParseGlobal_Function(struct ParserProcess* Parser);

// Attempts to parse a new AST, covering a Struct declaration and optionally its definition.
ui8 ParseGlobal_Struct(struct ParserProcess* Parser);

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
			||	ParseGlobal_VarDeclarationStatement(Parser))
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
	// Temp: Assert on any parser error while the parser is in active development.
	ASSERT(0);

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

	struct Token* NextToken = Parser_PeekToken(Parser);
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
		NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume "static"
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

	// Check for const-ness.
	if (NextToken->Val.Keyword == KEYWORD_CONST)
	{
		Flags |= DATATYPE_IS_CONST;
		NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume "const"
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

	// Determine volatility.
	if (NextToken->Val.Keyword == KEYWORD_VOLATILE)
	{
		Flags |= DATATYPE_IS_VOLATILE;
		NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume "volatile"
		if (NextToken == NULL)
		{
			goto PARSE_FAIL_EOF;
		}
	}

	// Handle structured / enumerated types...
	if (NextToken->Val.Keyword == KEYWORD_STRUCT)
	{
		Flags |= DATATYPE_IS_STRUCTURED;
		OutDatatypeDef->Type = DATATYPE_USER_DEFINED;
		NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume "struct"
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
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

	// Determine signage.
	Flags |= DATATYPE_IS_UNSIGNED * (NextToken->Val.Keyword == KEYWORD_UNSIGNED);
	ui8 SignKeywordPresent = 0;
	if (NextToken->Val.Keyword == KEYWORD_SIGNED
		|| NextToken->Val.Keyword == KEYWORD_UNSIGNED)
	{
		if (Flags & (DATATYPE_IS_STRUCTURED | DATATYPE_IS_ENUM))
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier combination.");
			goto PARSE_FAIL;
		}

		SignKeywordPresent = 1;
		NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume signed / unsigned.
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

	// Next determine size and broad type.
	if (NextToken->Type == TOKEN_KEYWORD)
	{
		if (Flags & (DATATYPE_IS_STRUCTURED | DATATYPE_IS_ENUM))
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier combination.");
			goto PARSE_FAIL;
		}

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

		NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume type keyword.
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
				NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume second "long".
			}
		}
	}
	else if (Flags & (DATATYPE_IS_STRUCTURED | DATATYPE_IS_ENUM))
	{
		if (NextToken->Type == TOKEN_IDENTIFIER)
		{
			OutDatatypeDef->TypeName = String_Copy_ANSI(NextToken->Val.Identifier);
			NextToken = Parser_ConsumeToken(Parser), Parser_PeekToken(Parser); // Consume user defined type identifier.
		}
		// Otherwise leave as anonymous structured / enum type.
	}
	else
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Missing type specifier.");
		goto PARSE_FAIL;
	}

	// Check specific error case - non-pointer VOID type, unless it's explicitly allowed.
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

// Recursively deconstructs a main expression node into its constituent comma-separated variable declaration nodes,
// by breaking apart (and freeing) all "top-level" comma operator nodes it finds.
// The produced declaration expression (chains) follow the format: <OP DEREF>(0.. N)<VAR ACCESS>(<OP ASSIGN><EXP>)().. N).
// Once this is called the passed in ExpressionNode should be considered discarded and NOT re-used !
void ExtractVarDeclarationExpressions(struct ParserProcess* Parser, struct AST_Node* ExpressionNode, struct Vector* OutVarNodes)
{
	ASSERT(ExpressionNode != NULL);
	ASSERT(OutVarNodes != NULL);

	// Recursive case.
	if (ExpressionNode->Val.Expression.Op.OperatorSymbol == SYMBOL_OP_COMMA)
	{
		ExtractVarDeclarationExpressions(Parser, ExpressionNode->Val.Expression.Op.LeftOperand, OutVarNodes);
		if (Parser->HasError) return;
		ExtractVarDeclarationExpressions(Parser, ExpressionNode->Val.Expression.Op.RightOperand, OutVarNodes);
		if (Parser->HasError) return;

		// ... Then get rid of the entry node (after detaching it from its operands).
		ExpressionNode->Val.Expression.Op.LeftOperand = NULL;
		ExpressionNode->Val.Expression.Op.RightOperand = NULL;
		FreeNode(ExpressionNode);
		return;
	}

	// Base cases.

	struct AST_Node* VarNode = AllocNewNode(AST_NODE_VARIABLE);
	VarNode->BufferLocation = ExpressionNode->BufferLocation;

	// The cases below will seek to find the Identifier node for the variable declaration expression,
	// which can either be a VAR_ACCESS (from which the variable name is extracted) or a NOP (indicating a anonymous variable).
	struct AST_Node* IdentifierExpressionNode = ExpressionNode;

	if (IdentifierExpressionNode->Val.Expression.Type == EXP_OP
		&& IdentifierExpressionNode->Val.Expression.Op.OperatorSymbol == SYMBOL_OP_ASSIGN)
	{	
		IdentifierExpressionNode = IdentifierExpressionNode->Val.Expression.Op.LeftOperand;
		while (IdentifierExpressionNode->Val.Expression.Type == EXP_OP
			&& IdentifierExpressionNode->Val.Expression.Op.OperatorSymbol == SYMBOL_OP_DEREF)
		{
			IdentifierExpressionNode = IdentifierExpressionNode->Val.Expression.Op.RightOperand;
		}
	}
	else if ((ExpressionNode->Val.Expression.Type == EXP_OP
			&& ExpressionNode->Val.Expression.Op.OperatorSymbol == SYMBOL_OP_DEREF))
	{	
		IdentifierExpressionNode = ExpressionNode->Val.Expression.Op.RightOperand;
		while (IdentifierExpressionNode->Val.Expression.Type == EXP_OP
			&& IdentifierExpressionNode->Val.Expression.Op.OperatorSymbol == SYMBOL_OP_DEREF)
		{
			IdentifierExpressionNode = IdentifierExpressionNode->Val.Expression.Op.RightOperand;
		}
	}
	
	if (IdentifierExpressionNode->Val.Expression.Type != EXP_VAR_ACCESS
		&& IdentifierExpressionNode->Val.Expression.Type != EXP_NOP)
	{
		Parser_Error(Parser, ExpressionNode->BufferLocation, "Invalid variable declaration expression format.");
		return;
	}
	
	Vector_PushPtr(OutVarNodes, &ExpressionNode);
}

// Attempts to parse a Var Declaration statement node, up until reaching the provided end symbol.
struct AST_Node* ParseVariableDeclarationStatementNode(struct ParserProcess* Parser, enum TOKEN_SYMBOL EndSymbol)
{
	int TokenStartIndex = Parser->TokenIndex;

	struct AST_Node* VarDecNode = NULL;

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing variable.");
	PARSE_FAIL:
		if (VarDecNode != NULL) FreeNode(VarDecNode);
		return NULL;
	}

	// Get the data type to assign to the returned variable declaration node.
	// If it fails, then we know we're not dealing with a variable, but it could be something else
	// hence no error is output.
	struct DatatypeDef VarType;
	if (!ParseDatatypeDef(Parser, &VarType, 0))
	{
		goto PARSE_FAIL;
	}

	// Parse main declaration expression.
	struct AST_Node* VarExpression = ParseExpressionNode(Parser, EndSymbol);
	if (VarExpression == NULL) goto PARSE_FAIL; // Failed because of error or because there was no expression to parse at all (struct declaration ?)

	if (VarExpression->Val.Expression.Type == EXP_FUNC_CALL)
	{
		// This is a function declaration !
		// Abandon now without emitting an error and let the Function Parsing process take over.
		goto PARSE_FAIL;
	}

	// Now take the main expression and break it down into its constituent declaration / definition expressions.
	struct Vector DecExpressions = Vector_Create(struct AST_Node*, 1);
	ExtractVarDeclarationExpressions(Parser, VarExpression, &DecExpressions);

	// Construct Variable Declaration StatementNode node and return it.
	VarDecNode = AllocNewNode(AST_NODE_STATEMENT_VAR_DEC);
	VarDecNode->BufferLocation = NextToken->BufferLocation;
	VarDecNode->Val.Statement.VarDeclaration.Type = VarType;
	VarDecNode->Val.Statement.VarDeclaration.VarExpressions = DecExpressions;

	return VarDecNode;
}

static void GetAllStatements(struct AST_Node* RootStatement, struct Vector* Out);

static void GetAllStatements_Block(struct AST_Node* BlockStatement, struct Vector* Out)
{
	ASSERT(BlockStatement != NULL);
	ASSERT(BlockStatement->Type == AST_NODE_STATEMENT_BLOCK);
	ASSERT(Out != NULL);

	Vector_Push(*Out, struct AST_Node*, BlockStatement);

	for (int i = 0; i < BlockStatement->Val.Statement.Block.Statements.Size; i++)
	{
		GetAllStatements(Vector_GetValueAt(BlockStatement->Val.Statement.Block.Statements, struct AST_Node*, i), Out);
	}
}

static void GetAllStatements_Conditional(struct AST_Node* ConditionalStatement, struct Vector* Out)
{
	ASSERT(ConditionalStatement != NULL);
	ASSERT(ConditionalStatement->Type == AST_NODE_STATEMENT_IF
	|| ConditionalStatement->Type == AST_NODE_STATEMENT_WHILE);
	ASSERT(Out != NULL);

	Vector_Push(*Out, struct AST_Node*, ConditionalStatement);

	if (ConditionalStatement->Type == AST_NODE_STATEMENT_IF)
	{
		GetAllStatements(ConditionalStatement->Val.Statement.If.ExecStatement, Out);

		if (ConditionalStatement->Val.Statement.If.ExecStatement_Else != NULL)
		{
			GetAllStatements(ConditionalStatement->Val.Statement.If.ExecStatement_Else, Out);
		}
	}
	else
	{
		GetAllStatements(ConditionalStatement->Val.Statement.While.ExecStatement, Out);
	}
}

static void GetAllStatements_For(struct AST_Node* ForStatement, struct Vector* Out)
{
	ASSERT(ForStatement != NULL);
	ASSERT(ForStatement->Type == AST_NODE_STATEMENT_FOR);
	ASSERT(Out != NULL);

	Vector_Push(*Out, struct AST_Node*, ForStatement);

	GetAllStatements(ForStatement->Val.Statement.For.ExecStatement, Out);
 }

// Recursively collects all sub-statements from a given statement into Out.
static void GetAllStatements(struct AST_Node* RootStatement, struct Vector* Out)
{
	ASSERT(RootStatement != NULL);
	ASSERT(Out != NULL);

	// Switch on the statement type to call the correct statements collection function.
	switch (RootStatement->Type)
	{
	case AST_NODE_STATEMENT_BLOCK:
		GetAllStatements_Block(RootStatement, Out);
		break;
	case AST_NODE_STATEMENT_IF:
	case AST_NODE_STATEMENT_WHILE:
		GetAllStatements_Conditional(RootStatement, Out);
		break;
	case AST_NODE_STATEMENT_FOR:
		GetAllStatements_For(RootStatement, Out);
		break;
	default:
		Vector_Push(*Out, struct AST_Node*, RootStatement);
	}
}

struct AST_Node* ParseBlockStatementNode(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* BlockNode = NULL;

	struct Token* NextToken = Parser_ConsumeToken(Parser);
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
		
	// Check for empty block.
	if (Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE))
	{
		NextToken = Parser_ConsumeToken(Parser);
		return BlockNode;
	}

	// Parse statements on loop. Any instruction other than sub-blocks should be separated by ';' tokens.
	struct AST_Node* Statement = ParseStatementNode(Parser);
	while (!Parser->HasError && Statement != NULL)
	{
		Vector_Push(BlockNode->Val.Statement.Block.Statements, struct AST_Node*, Statement);

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
		if (Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE))
		{
			Parser_ConsumeToken(Parser);
			break;
		}

		Statement = ParseStatementNode(Parser);
	}

	if (Parser->HasError)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Error while parsing instructions block.");
		goto PARSE_FAIL;
	}

	return BlockNode;
}

// Special wrapper for ParseStatementNode that disallows var declaration statements which wouldn't make sense
// as single statements whose execution depends on a condition.
struct AST_Node* ParseDependentStatementNode(struct ParserProcess* Parser)
{
	struct AST_Node* StatementNode = ParseStatementNode(Parser);
	if (StatementNode == NULL) return NULL;

	// Do not allow a var declaration statement.
	if (StatementNode->Type == AST_NODE_STATEMENT_VAR_DEC)
	{
		Parser_Error(Parser, StatementNode->BufferLocation, "Dependent statement cannot be a declaration.");
		FreeNode(StatementNode);
		return NULL;
	}

	return StatementNode;
}

struct AST_Node* ParseConditionalStatementNode(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* StatementNode = NULL;

	struct Token* NextToken = Parser_ConsumeToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing conditional instruction.");
	PARSE_FAIL:
		Parser->TokenIndex = StartTokenIndex;
		if (StatementNode != NULL) FreeNode(StatementNode);
		return NULL;
	}

	ui8 IsWhile = Token_IsKeyword(NextToken, KEYWORD_WHILE);
	if (	!IsWhile
		&&	!Token_IsKeyword(NextToken, KEYWORD_IF))
	{
		// Not a conditional block.
		goto PARSE_FAIL;
	}

	StatementNode = AllocNewNode(IsWhile ? AST_NODE_STATEMENT_WHILE : AST_NODE_STATEMENT_IF);
	StatementNode->BufferLocation = NextToken->BufferLocation;

	// Parse the condition expression, specifically placing it in a parenthesis scope.
	NextToken = Parser_ConsumeToken(Parser);
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
		StatementNode->Val.Statement.While.LoopCondition = ConditionNode;
	}
	else
	{
		StatementNode->Val.Statement.If.EntryCondition = ConditionNode;
	}

	struct AST_Node* ExecNode = ParseDependentStatementNode(Parser);

	if (ExecNode == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse conditional instruction.");
		goto PARSE_FAIL;
	}

	if (IsWhile)
	{
		StatementNode->Val.Statement.While.ExecStatement = ExecNode;

		// Go over all sub-statements of this while loop and link up any break and continue statement that isn't already linked up to something.
		struct Vector Substatements = Vector_Create(struct AST_Node*, 8);
		GetAllStatements(StatementNode->Val.Statement.While.ExecStatement, &Substatements);

		for (int i = 0; i < Substatements.Size; i++)
		{
			struct AST_Node* Substatement = Vector_GetValueAt(Substatements, struct AST_Node*, i);
			ASSERT(Substatement != NULL);

			if (Substatement->Type != AST_NODE_STATEMENT_CONTROL) continue;
			enum TOKEN_KEYWORD Kwd = Substatement->Val.Statement.Control.Keyword;

			if (Kwd != KEYWORD_CONTINUE && Kwd != KEYWORD_BREAK) continue;
			if (Substatement->Val.Statement.Control.TargetStatement != NULL) continue; // Already linked to a sub-loop.

			Substatement->Val.Statement.Control.TargetStatement = StatementNode;
		}

		Vector_Destroy(&Substatements);
	}
	else
	{
		StatementNode->Val.Statement.If.ExecStatement = ExecNode;

		// Check for an else instruction.
		NextToken = Parser_PeekToken(Parser);
		if (Token_IsKeyword(NextToken, KEYWORD_ELSE))
		{
			Parser_ConsumeToken(Parser);
			StatementNode->Val.Statement.If.ExecStatement_Else = ParseDependentStatementNode(Parser);

			if (StatementNode->Val.Statement.If.ExecStatement_Else == NULL)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Error parsing else dependent statement");
				goto PARSE_FAIL;
			}
		}
	}

	return StatementNode;
}

struct AST_Node* ParseForStatementNode(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* StatementNode = NULL;

	struct Token* NextToken = Parser_ConsumeToken(Parser);
	if (!Token_IsKeyword(NextToken, KEYWORD_FOR))
	{
		// Not a for instruction.
		goto PARSE_FAIL;
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing For instruction.");
	PARSE_FAIL:
		Parser->TokenIndex = StartTokenIndex;
		if (StatementNode != NULL) FreeNode(StatementNode);
		return NULL;
	}

	// Enclose the three expressions into a parenthesis scope. The first two are parsed up until encountering a semicolon, the third the closing parenthesis.
	NextToken = Parser_ConsumeToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;
	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected '(' token.");
		goto PARSE_FAIL;
	}

	StatementNode = AllocNewNode(AST_NODE_STATEMENT_FOR);
	StatementNode->BufferLocation = NextToken->BufferLocation;

	// Parse init, condition and post-loop expressions.
	StatementNode->Val.Statement.For.InitExpression = ParseExpressionNode(Parser, SYMBOL_SEMICOLON);
	if (Parser->HasError) goto PARSE_FAIL;
	StatementNode->Val.Statement.For.LoopCondition = ParseExpressionNode(Parser, SYMBOL_SEMICOLON);	
	if (Parser->HasError) goto PARSE_FAIL;
	StatementNode->Val.Statement.For.PostLoopExpression = ParseExpressionNode(Parser, SYMBOL_PARENTHESIS_CLOSE);
	if (Parser->HasError) goto PARSE_FAIL;

	StatementNode->Val.Statement.For.ExecStatement = ParseDependentStatementNode(Parser);
	if (StatementNode->Val.Statement.For.ExecStatement == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected statement following for loop declaration.");
		goto PARSE_FAIL;
	}

	// Go over all sub-statements of this while loop and link up any break and continue statement that isn't already linked up to something.
	struct Vector Substatements = Vector_Create(struct AST_Node*, 8);
	GetAllStatements(StatementNode->Val.Statement.For.ExecStatement, &Substatements);

	for (int i = 0; i < Substatements.Size; i++)
	{
		struct AST_Node* Substatement = Vector_GetValueAt(Substatements, struct AST_Node*, i);
		ASSERT(Substatement != NULL);

		if (Substatement->Type != AST_NODE_STATEMENT_CONTROL) continue;
		enum TOKEN_KEYWORD Kwd = Substatement->Val.Statement.Control.Keyword;

		if (Kwd != KEYWORD_CONTINUE && Kwd != KEYWORD_BREAK) continue;
		if (Substatement->Val.Statement.Control.TargetStatement != NULL) continue; // Already linked to a sub-loop.

		Substatement->Val.Statement.Control.TargetStatement = StatementNode;
	}

	Vector_Destroy(&Substatements);
	return StatementNode;
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

struct AST_Node* ParseControlStatementNode(struct ParserProcess* Parser)
{
	// Parse a control keyword.
	int StartTokenIndex = Parser->TokenIndex;

	struct AST_Node* ControlStatementNode = NULL;

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing Control StatementNode.");
	PARSE_FAIL:
		if (ControlStatementNode != NULL) FreeNode(ControlStatementNode);
		Parser->TokenIndex = StartTokenIndex;
		return NULL;
	}

	ControlStatementNode = AllocNewNode(AST_NODE_STATEMENT_CONTROL);
	ControlStatementNode->BufferLocation = NextToken->BufferLocation;

	switch (NextToken->Val.Keyword)
	{
	case KEYWORD_GOTO:
		Parser_Error(Parser, NextToken->BufferLocation, "goto keyword parsing is unimplemented.");
		goto PARSE_FAIL;
	case KEYWORD_CASE:
		Parser_Error(Parser, NextToken->BufferLocation, "case keyword parsing is unimplemented.");
		goto PARSE_FAIL;
	case KEYWORD_BREAK:
	case KEYWORD_CONTINUE:
	case KEYWORD_RETURN:
		NextToken = Parser_ConsumeToken(Parser);
		break;
	default:
		goto PARSE_FAIL; // Not a control keyword.
	}

	ControlStatementNode->Val.Statement.Control.Keyword = NextToken->Val.Keyword;

	// Parse following expression. No expression is expected for BREAK and CONTINUE. For RETURN, whatever is parsed gets assigned and will be checked by Symbolizer.
	struct AST_Node* ExpressionNode = ParseExpressionNode(Parser, SYMBOL_SEMICOLON);
	if (Parser->HasError)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Error while parsing control keyword expression.");
		goto PARSE_FAIL;
	}

	if (NextToken->Val.Keyword == KEYWORD_RETURN)
	{
		ControlStatementNode->Val.Statement.Control.Expression = ExpressionNode;
	}
	else if (ExpressionNode != NULL)
	{
		Parser_Error(Parser, ExpressionNode->BufferLocation, "Unexpected expression following keyword.");
		FreeNode(ExpressionNode);
		goto PARSE_FAIL;
	}

	return ControlStatementNode;
}

// Parses a single statement of any type.
struct AST_Node* ParseStatementNode(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* StatementNode = NULL;

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing block.");
		return 0;
	}

	if (Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN))
	{
		StatementNode = ParseBlockStatementNode(Parser);
	}
	else if (Token_IsKeyword(NextToken, KEYWORD_IF)
		|| Token_IsKeyword(NextToken, KEYWORD_WHILE))
	{
		StatementNode = ParseConditionalStatementNode(Parser);
	}
	else if (Token_IsKeyword(NextToken, KEYWORD_FOR))
	{
		StatementNode = ParseForStatementNode(Parser);
	}
	else if (Token_IsKeyword(NextToken, KEYWORD_SWITCH))
	{
		StatementNode = ParseSwitchStatementNode(Parser);
	}
	else if (
		Token_IsKeyword(NextToken, KEYWORD_BREAK)
		|| Token_IsKeyword(NextToken, KEYWORD_CONTINUE)
		|| Token_IsKeyword(NextToken, KEYWORD_RETURN)
		|| Token_IsKeyword(NextToken, KEYWORD_CASE)
		|| Token_IsKeyword(NextToken, KEYWORD_GOTO))
	{
		StatementNode = ParseControlStatementNode(Parser);
	}
	else
	{
		// Attempt to parse a var declaration node.
		StatementNode = ParseVariableDeclarationStatementNode(Parser, SYMBOL_SEMICOLON);
		if (StatementNode == NULL)
		{
			// ... Otherwise continue on to parsing a free-standing expression.
			StatementNode = ParseExpressionNode(Parser, SYMBOL_SEMICOLON);
		}
	}

	if (Parser->HasError || StatementNode == NULL)
	{
		if (!Parser->HasError) Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse statement.");
		if (StatementNode != NULL) FreeNode(StatementNode);
		return NULL;
	}
	return StatementNode;
}

ui8 ParseGlobal_VarDeclarationStatement(struct ParserProcess* Parser)
{
	struct AST_Node* VarNode = ParseVariableDeclarationStatementNode(Parser, SYMBOL_SEMICOLON);
	if (VarNode == NULL)
	{
		return 0;
	}

	Vector_PushPtr(Parser->RootNodes, &VarNode);
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
	// - Variable declarations nodes up until a closing parenthesis is reached
	// - Either a semicolon or a block

	struct DatatypeDef ReturnType;
	if (!ParseDatatypeDef(Parser, &ReturnType, 1))
	{
		goto PARSE_FAIL;
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing function.");
	PARSE_FAIL:
		Parser->TokenIndex = StartTokenIndex;
		if (FunctionNode != NULL) FreeNode(FunctionNode);
		return 0;
	}
	struct Token* NextToken = Parser_ConsumeToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	struct Token* IdentifierToken = NextToken;
	if (IdentifierToken->Type != TOKEN_IDENTIFIER)
	{
		goto PARSE_FAIL;
	}

	NextToken = Parser_ConsumeToken(Parser);
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
	struct AST_Node* ParamVarNode = ParseVariableDeclarationStatementNode(Parser, SYMBOL_OP_COMMA);

	while (ParamVarNode != NULL)
	{
		ASSERT(ParamVarNode->Val.Statement.VarDeclaration.VarExpressions.Size == 1);
		// Add to function's parameters collection.
		Vector_Push(FunctionNode->Val.Function.Params, struct AST_Node*, ParamVarNode);

		// Parse next parameter.
		ParamVarNode = ParseVariableDeclarationStatementNode(Parser, SYMBOL_OP_COMMA);
	}

	// Expect a closing parenthesis.
	NextToken = Parser_ConsumeToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;
	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected closing parenthesis in function declaration.");
		goto PARSE_FAIL;
	}

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;
	if (Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		// Parse as declaration. Nothing else to be done other than consuming the token.
		NextToken = Parser_ConsumeToken(Parser);
	}
	else if (Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN))
	{
		// Parse as definition.
		// First parse a whole block, then go through it and collect any internal variable declarations
		FunctionNode->Val.Function.Statements = ParseBlockStatementNode(Parser);

		// Go over all sub-statements of this while loop and link up any break and continue statement that isn't already linked up to something.
		struct Vector Substatements = Vector_Create(struct AST_Node*, 8);
		GetAllStatements(FunctionNode->Val.Function.Statements, &Substatements);

		for (int i = 0; i < Substatements.Size; i++)
		{
			struct AST_Node* Substatement = Vector_GetValueAt(Substatements, struct AST_Node*, i);
			ASSERT(Substatement != NULL);

			if (Substatement->Type != AST_NODE_STATEMENT_CONTROL) continue;
			enum TOKEN_KEYWORD Kwd = Substatement->Val.Statement.Control.Keyword;

			if (Kwd != KEYWORD_RETURN) continue;
			ASSERT(Substatement->Val.Statement.Control.TargetStatement == NULL); // If a return statement is already linked to something, things are very very wrong...

			Substatement->Val.Statement.Control.TargetStatement = FunctionNode;
		}

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

// Attempts to parse a struct type declaration/definition (named or anonymous), optionally followed by inline
// variable declaration(s) of that type. Fills OutInlineVars with any inline variable nodes produced.
// Returns NULL, if this isn't a struct declaration or definition at all.
// Check Parser->HasError after a NULL return to tell a real parse error apart from a simple non-match.
// Sets OutInlineVarDecNode to pointer to newly allocated inline variable declaration statement accompanying the structure, if any. 
static struct AST_Node* ParseStructNode(struct ParserProcess* Parser, struct AST_Node** OutInlineVarDecNode)
{
	ASSERT(OutInlineVarDecNode != NULL);

	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* StructNode = NULL;

	struct Token* StartToken = Parser_PeekToken(Parser);

	// Let the generic datatype parser handle the "struct Name" / "struct" (anonymous) prefix
	struct DatatypeDef StructType;
	if (!ParseDatatypeDef(Parser, &StructType, 0))
	{
		goto PARSE_FAIL;
	}

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing struct.");
	PARSE_FAIL:
		Parser->TokenIndex = StartTokenIndex;
		if (StructNode != NULL) FreeNode(StructNode);
		if (*OutInlineVarDecNode != NULL) FreeNode(*OutInlineVarDecNode);
		else String_Free_ANSI(&StructType.TypeName);
		return NULL;
	}

	if (!(StructType.Flags & DATATYPE_IS_STRUCTURED))
	{
		// Not a struct type.
		goto PARSE_FAIL;
	}

	if (!Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN) && !Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		// Neither a body nor a bare declaration follows, so this is probably a variable declaration.
		goto PARSE_FAIL;
	}

	// From here on this is unambiguously a struct declaration/definition, so any weirdness is a hard error.
	if (StructType.Flags & DATATYPE_IS_VOLATILE)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "A struct declaration cannot be marked volatile.");
		goto PARSE_FAIL;
	}
	if (StructType.PointerLevel != 0)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Unexpected pointer level in struct declaration.");
		goto PARSE_FAIL;
	}

	StructNode = AllocNewNode(AST_NODE_STRUCT);
	StructNode->BufferLocation = StartToken->BufferLocation;
	StructNode->Val.Struct.Type = StructType;
	StructNode->Val.Struct.Members = Vector_Create(struct AST_Node*, 0);

	if (Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		if (StructType.TypeName.Length == 0)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Anonymous struct requires a body.");
			goto PARSE_FAIL;
		}

		Parser_ConsumeToken(Parser); // Consume ';'.
		return StructNode;
	}

	Parser_ConsumeToken(Parser); // Consume '{'.

	// Parse members until the closing brace is reached. Each member slot may itself be a nested sub-structure.
	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	while (!Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE))
	{
		// First try parsing a substructure, then a variable.
		struct AST_Node* SubInlineVarDecNode = NULL;
		struct AST_Node* SubStructNode = ParseStructNode(Parser, &SubInlineVarDecNode);
		if (Parser->HasError)
		{
			goto PARSE_FAIL;
		}

		if (SubStructNode != NULL) // Member is a substructure.
		{
			Vector_PushPtr(&StructNode->Val.Struct.Members, &SubStructNode);	
			if (SubInlineVarDecNode != NULL)
			{
				Vector_Push(StructNode->Val.Struct.Members, struct AST_Node*, SubInlineVarDecNode);	
			}	
		}
		else // Member is a var declaration statement.
		{
			struct AST_Node* MemberVarDecNode = ParseVariableDeclarationStatementNode(Parser, SYMBOL_SEMICOLON);
			Vector_Push(StructNode->Val.Struct.Members, struct AST_Node*, MemberVarDecNode);
		}

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

	Parser_ConsumeToken(Parser); // Consume '}'.

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	// Inline variable declaration(s).
	struct DatatypeDef InlineVarType = StructNode->Val.Struct.Type;

	struct AST_Node* InlineVarExpression = ParseExpressionNode(Parser, SYMBOL_SEMICOLON);
	if (InlineVarExpression == NULL)
	{
		goto PARSE_FAIL;
	}

	if (InlineVarExpression == NULL) return StructNode;

	// If an expression was parsed after the struct declaration,
	// attempt to interpret it as a var declaration expression.

	struct Vector VarDecExpressions = Vector_Create(struct AST_Node*, 1);
	ExtractVarDeclarationExpressions(Parser, InlineVarExpression, &VarDecExpressions);
	if (Parser->HasError)
	{
		return NULL;
	}

	struct AST_Node* InlineVarDec = AllocNewNode(AST_NODE_STATEMENT_VAR_DEC);
	InlineVarDec->BufferLocation = NextToken->BufferLocation;
	InlineVarDec->Val.Statement.VarDeclaration.Type = StructType;
	InlineVarDec->Val.Statement.VarDeclaration.VarExpressions = VarDecExpressions;

	*OutInlineVarDecNode = InlineVarDec;
	return StructNode;
}

ui8 ParseGlobal_Struct(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;

	struct AST_Node* InlineVarDecNode = NULL;
	struct AST_Node* StructNode = ParseStructNode(Parser, &InlineVarDecNode);
	if (StructNode == NULL)
	{
		return 0;
	}

	if (StructNode->Val.Struct.Type.TypeName.Length == 0 && InlineVarDecNode == NULL)
	{
		Parser_Error(Parser, StructNode->BufferLocation, "Invalid anonymous struct declaration. Add a type name, or inline variable declarations using it.");
		Parser->TokenIndex = StartTokenIndex;
		return 0;
	}

	Vector_PushPtr(Parser->RootNodes, &StructNode);
	Vector_PushPtr(Parser->RootNodes, &InlineVarDecNode);
	return 1;
}
