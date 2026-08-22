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
		else if (Node->Val.Expression.Type == EXP_VAR_ACCESS)
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
	case AST_NODE_STATEMENT_EXP:
		FreeNode(Node->Val.Statement.Expression);
		break;
	case AST_NODE_STATEMENT_CONTROL:
		FreeNode(Node->Val.Statement.Control.Expression);
		// Don't free the Target Statement since it's not "owned" by this node.
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
		printf("<VAR_ACCESS: '%s' : %s>\n", Node->Val.Expression.Variable.Name.Str, Datatype_GetName(&Node->Val.Expression.ResultType));
		break;
	case EXP_OP:
		printf("<OP: '%s'>\n", Symbol_ToString(Node->Val.Expression.Op.OperatorSymbol));
		PrintNode(Node->Val.Expression.Op.LeftOperand, Depth + 1);
		PrintNode(Node->Val.Expression.Op.RightOperand, Depth + 1);
		break;
	case EXP_FUNCTION_CALL:
		printf("<FUNCTION_CALL: '%s' : %s>\n", Node->Val.Expression.FunctionCall.FunctionName.Str, Datatype_GetName(&Node->Val.Expression.ResultType));
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
		printf("<VAR_DEC: '%s' : %s>\n", Node->Val.Variable.Name.Str, Datatype_GetName(&Node->Val.Variable.Type));
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
		printf("<FUNCTION: '%s' : %s>\n", Node->Val.Function.Name.Str, Datatype_GetName(&Node->Val.Function.ReturnType));
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
ui8 ParseGlobal_Variables(struct ParserProcess* Parser);

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
			||	ParseGlobal_Variables(Parser))
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

	// Check for const-ness.
	if (NextToken->Val.Keyword == KEYWORD_CONST)
	{
		Flags |= DATATYPE_IS_CONST;
		NextToken = Parser_NextToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

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

	// Handle structured / enumerated types...
	if (NextToken->Val.Keyword == KEYWORD_STRUCT)
	{
		Flags |= DATATYPE_IS_STRUCT;
		OutDatatypeDef->Type = DATATYPE_USER_DEFINED;
		NextToken = Parser_NextToken(Parser);
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
		if (Flags & (DATATYPE_IS_STRUCT | DATATYPE_IS_UNION | DATATYPE_IS_ENUM))
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier combination.");
			goto PARSE_FAIL;
		}

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
		if (Flags & (DATATYPE_IS_STRUCT | DATATYPE_IS_UNION | DATATYPE_IS_ENUM))
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
	else if (NextToken->Type == TOKEN_IDENTIFIER
		&& Flags & (DATATYPE_IS_STRUCT | DATATYPE_IS_UNION | DATATYPE_IS_ENUM))
	{
		OutDatatypeDef->TypeName = String_Copy_ANSI(NextToken->Val.Identifier);
	}
	else if (Flags & (DATATYPE_IS_STRUCT | DATATYPE_IS_UNION | DATATYPE_IS_ENUM))
	{
		Flags |= DATATYPE_IS_ANONYMOUS;
	}
	else
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Missing type specifier.");
		goto PARSE_FAIL;
	}

	// Parse pointer levels, unless this is an anonymous struct.
	// (Since the token is consumed and the symbol information is discarded, no need for deambiguation...
	while (Token_IsSymbol(NextToken, SYMBOL_OP_AMB_STAR)
		|| Token_IsSymbol(NextToken, SYMBOL_OP_DEREF)) // ... but still support using DEREF in case the token source was not built from text).
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

// Recursively parses variable declaration nodes from a passed expression, so the "top level" comma operators
// may be interpreted as separators between multiple declarations instead of their standard in-expression role.
// If successful the entry Expression Node will either be freed or part of a variable node, so consider it discarded !
void ParseVariableDeclarationNodes_FromExp(struct ParserProcess* Parser, 
	struct DatatypeDef* Datatype, struct AST_Node* ExpressionNode, struct Vector* OutVarNodes)
{
	ASSERT(Datatype != NULL);
	ASSERT(ExpressionNode != NULL);
	ASSERT(OutVarNodes != NULL);

	// Recursive case.
	if (ExpressionNode->Val.Expression.Op.OperatorSymbol == SYMBOL_OP_COMMA)
	{
		ParseVariableDeclarationNodes_FromExp(Parser, Datatype, ExpressionNode->Val.Expression.Op.LeftOperand, OutVarNodes);
		if (Parser->HasError) return;
		ParseVariableDeclarationNodes_FromExp(Parser, Datatype, ExpressionNode->Val.Expression.Op.RightOperand, OutVarNodes);
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
	if (ExpressionNode->Val.Expression.Type == EXP_VAR_ACCESS)
	{
		// Variable declaration. Copy the name into the variable node itself.
		VarNode->Val.Variable.Name = String_Copy_ANSI(ExpressionNode->Val.Expression.Variable.Name);
		// Get rid of the node.
		FreeNode(ExpressionNode);
	}
	else if (ExpressionNode->Val.Expression.Type == EXP_OP
		&& ExpressionNode->Val.Expression.Op.OperatorSymbol == SYMBOL_OP_ASSIGN
		&& ExpressionNode->Val.Expression.Op.LeftOperand->Val.Expression.Type == EXP_VAR_ACCESS)
	{
		// Variable definition.
		// Copy the name from the expression but leave the full expression in as the variable node's value node.
		VarNode->Val.Variable.Name = String_Copy_ANSI(ExpressionNode->Val.Expression.Op.LeftOperand->Val.Expression.Variable.Name);
		VarNode->Val.Variable.Value = ExpressionNode;
	}
	else if (0 /* && ExpressionNode->Val.Expression.Type == EXP_OP
		&& ExpressionNode->Val.Expression.Op.OperatorSymbol == SYMBOL_OP_ARRAY_ACCESS */)
	{
		// ... TODO Array definition.
	}
	else
	{
		Parser_Error(Parser, ExpressionNode->BufferLocation, "Unexpected expression format in var declaration.");
		return;
	}

	VarNode->Val.Variable.Type = *Datatype;

	Vector_PushPtr(OutVarNodes, &VarNode);
}

// Attempts to parse a vector of variable declaration nodes along with a possible value expression,
// bound by the passed end symbol.
// Check for parser errors after calling.
struct Vector ParseVariableDeclarationNodes(struct ParserProcess* Parser, enum TOKEN_SYMBOL EndSymbol)
{
	int TokenStartIndex = Parser->TokenIndex;

	struct Vector VarNodes = Vector_Create(struct AST_Node*, 1);

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing variable.");
	PARSE_FAIL:
		FreeNodeVector(&VarNodes);
		return VarNodes;
	}

	// Get the data type to assign to the returned variable declaration nodes.
	// If it fails, then we know we're not dealing with a variable, but it could be something else
	// hence no error is output.
	struct DatatypeDef VarType;
	if (!ParseDatatypeDef(Parser, &VarType, 0))
	{
		goto PARSE_FAIL;
	}

	// Parse main declaration expression.
	struct AST_Node* VarExpression = ParseExpressionNode(Parser, EndSymbol);

	if (VarExpression->Val.Expression.Type == EXP_FUNCTION_CALL)
	{
		// This is a function declaration !
		// Abandon now without emitting an error and let the Function Parsing process take over.
		goto PARSE_FAIL;
	}

	// Now take the expression and start parsing declarations from it.
	ParseVariableDeclarationNodes_FromExp(Parser, &VarType, VarExpression, &VarNodes);

	return VarNodes;
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
		
	// Check for empty block.
	if (Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE))
	{
		NextToken = Parser_NextToken(Parser);
		return BlockNode;
	}

	// Parse statements on loop. Any instruction other than sub-blocks should be separated by ';' tokens.
	struct Vector OutStatements = Vector_New(1, sizeof(struct AST_Node*));
	ParseStatementNodes(Parser, &OutStatements);
	while (!Parser->HasError && OutStatements.Size > 0)
	{
		Vector_Append(&BlockNode->Val.Statement.Block.Statements, &OutStatements);

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
		if (Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE))
		{
			Parser_NextToken(Parser);
			break;
		}

		OutStatements.Size = 0; // Empty vector. TODO: Add a "Move" version of "Append" to formalize the fact we're emptying a vector into another.
		ParseStatementNodes(Parser, &OutStatements);
	}

	if (Parser->HasError)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Error while parsing instructions block.");
		goto PARSE_FAIL;
	}

	return BlockNode;
}

// Special variant of the Statements parsing function that only accepts a single, non-var-declaration node
// at a time.
struct AST_Node* ParseDependentStatementNode(struct ParserProcess* Parser)
{
	// TODO: Doing it this way feels wrong. The ONLY way multiple nodes are returned from a single ParseStatementNodes
	// call is if it's a multi-declaration. Maybe it should go back to always returning one node, and the "var_dec" node should just
	// contain pre-checked expressions instead of a single variable's name and value.

	struct Vector Statements = Vector_New(0, sizeof(struct AST_Node*));
	ParseStatementNodes(Parser, &Statements);

	// Only accept a single statement (or a block).
	if (Statements.Size > 1)
	{
		Parser_Error(Parser, Vector_GetValueAt(Statements, struct AST_Node*, 0)->BufferLocation, "Compound statement disallowed when dependent.");
		Vector_Destroy(&Statements);
		return NULL;
	}
	else if (Statements.Size == 0)
	{
		Parser_Error(Parser, Parser_PeekToken(Parser)->BufferLocation, "Error while parsing dependent statement.");
		return NULL;
	}

	struct AST_Node* StatementNode = Vector_GetValueAt(Statements, struct AST_Node*, 0);
	Vector_Destroy(&Statements);

	return StatementNode;
}

struct AST_Node* ParseConditionalStatementNode(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* StatementNode = NULL;

	struct Token* NextToken = Parser_NextToken(Parser);
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
			Parser_NextToken(Parser);
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

	struct Token* NextToken = Parser_NextToken(Parser);
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
	NextToken = Parser_NextToken(Parser);
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
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing Control Statement.");
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
		NextToken = Parser_NextToken(Parser);
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

// Parses the next full statement which may be made up of multiple nodes.
// Returns the number of parsed statement nodes. If 0, check for errors on parser.
ui8 ParseStatementNodes(struct ParserProcess* Parser, struct Vector* OutStatements)
{
	ASSERT(OutStatements);

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
		// Special case: since variable declarations are allowed among statements, support it here.
		// This means that whatever calls this function has to check that the return result is in fact a statement
		// if they mean to access statement-only data.
		struct Vector VarNodes = ParseVariableDeclarationNodes(Parser, SYMBOL_SEMICOLON);
		if (VarNodes.Size > 0)
		{
			Vector_Append(OutStatements, &VarNodes);
			ui8 VarCount = VarNodes.Size;
			Vector_Destroy(&VarNodes);
			return VarCount;
		}
		else if (!Parser->HasError)
		{
			// ... Otherwise continue on to parsing a free-standing expression.
			StatementNode = ParseExpressionNode(Parser, SYMBOL_SEMICOLON);
		}
	}

	if (Parser->HasError || StatementNode == NULL) return 0;

	// Add parsed single statement to out vector.
	Vector_PushPtr(OutStatements, &StatementNode);
	return 1;
}

ui8 ParseGlobal_Variables(struct ParserProcess* Parser)
{
	struct Vector VarNodes = ParseVariableDeclarationNodes(Parser, SYMBOL_SEMICOLON);
	if (VarNodes.Size == 0)
	{
		Vector_Destroy(&VarNodes);
		return 0;
	}

	Vector_Append(Parser->RootNodes, &VarNodes);
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
	struct Vector OutVars = ParseVariableDeclarationNodes(Parser, SYMBOL_OP_COMMA);
	ASSERT(OutVars.Size < 2); // Because COMMA is set as the end symbol, we should never get more than 1 node at a time.

	while (OutVars.Size > 0)
	{
		struct AST_Node* ParamVarNode = Vector_GetValueAt(OutVars, struct AST_Node*, 0);
		ASSERT(ParamVarNode != NULL);

		// Ensure that this is a declaration (no value expression).
		if (ParamVarNode->Val.Variable.Value != NULL)
		{
			Parser_Error(Parser, ParamVarNode->BufferLocation, "Unexpected expression while parsing function parameter.");
			goto PARSE_FAIL;
		}

		// Ensure that this is not an array
		if (ParamVarNode->Val.Variable.IsArray)
		{
			Parser_Error(Parser, ParamVarNode->BufferLocation, "Cannot declare an array as a function parameter.");
			goto PARSE_FAIL;
		}

		// Add to function's parameters collection.
		Vector_Push(FunctionNode->Val.Function.Params, struct AST_Node*, ParamVarNode);

		// Parse next parameter.
		Vector_Destroy(&OutVars);
		OutVars = ParseVariableDeclarationNodes(Parser, SYMBOL_OP_COMMA);
		ASSERT(OutVars.Size < 2); // Because COMMA is set as the end symbol, we should never get more than 1 node at a time.
	}

	Vector_Destroy(&OutVars);

	// Expect a closing parenthesis.
	NextToken = Parser_NextToken(Parser);
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
		NextToken = Parser_NextToken(Parser);
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

ui8 ParseGlobal_Struct(struct ParserProcess* Parser)
{
	// UNIMPLEMENTED.
	return 0;
}
