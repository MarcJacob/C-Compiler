// Implementation file for Statement Parsing.

#include "parser.h"


void GetAllStatements_Block(struct AST_Node* BlockStatement, struct Vector* Out)
{
	ASSERT(BlockStatement != NULL);
	ASSERT(BlockStatement->Type == AST_NODE_STATEMENT_BLOCK);
	ASSERT(Out != NULL);

	Vector_Push(*Out, struct AST_Node*, BlockStatement);

	for (int i = 0; i < BlockStatement->Statement.Block.Statements.Size; i++)
	{
		GetAllStatements(Vector_GetValueAt(BlockStatement->Statement.Block.Statements, struct AST_Node*, i), Out);
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
		GetAllStatements(ConditionalStatement->Statement.If.ExecStatement, Out);

		if (ConditionalStatement->Statement.If.ExecStatement_Else != NULL)
		{
			GetAllStatements(ConditionalStatement->Statement.If.ExecStatement_Else, Out);
		}
	}
	else
	{
		GetAllStatements(ConditionalStatement->Statement.While.ExecStatement, Out);
	}
}

static void GetAllStatements_For(struct AST_Node* ForStatement, struct Vector* Out)
{
	ASSERT(ForStatement != NULL);
	ASSERT(ForStatement->Type == AST_NODE_STATEMENT_FOR);
	ASSERT(Out != NULL);

	Vector_Push(*Out, struct AST_Node*, ForStatement);

	GetAllStatements(ForStatement->Statement.For.ExecStatement, Out);
 }

// Recursively collects all sub-statements from a given statement into Out.
void GetAllStatements(struct AST_Node* RootStatement, struct Vector* Out)
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

// Attempts to parse a Var Declaration statement node, up until reaching the provided end symbol.
struct AST_Node* ParseVariableDeclarationStatementNode(struct ParserProcess* Parser, enum TOKEN_SYMBOL EndSymbol)
{
	int StartTokenIndex = Parser->TokenIndex;

	struct AST_Node* VarDecNode = NULL;

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing variable.");
	PARSE_FAIL:
		if (VarDecNode != NULL) FreeNode(VarDecNode);
		Parser->TokenIndex = StartTokenIndex;
		return NULL;
	}

	// Get the data type to assign to the returned variable declaration node.
	// If it fails, then we know we're not dealing with a variable declaration, but it could be something else
	// hence no error is output.
	struct DatatypeDef VarType;
	if (!ParseDatatypeDef(Parser, &VarType, 0))
	{
		goto PARSE_FAIL;
	}

	// Construct Variable Declaration StatementNode node and return it.
	VarDecNode = AllocNewNode(AST_NODE_STATEMENT_VAR_DEC);
	VarDecNode->BufferLocation = NextToken->BufferLocation;

	VarDecNode->Statement.VarDeclaration.Declarators = Vector_Create(struct AST_Node*, 1);
	if (!ParseDeclarators(Parser, &VarType, &VarDecNode->Statement.VarDeclaration.Declarators))
	{
		goto PARSE_FAIL;
	}

	return VarDecNode;
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
	BlockNode->Statement.Block.Statements = Vector_Create(struct AST_Node*, 0);

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
		Vector_Push(BlockNode->Statement.Block.Statements, struct AST_Node*, Statement);

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
	struct AST_Node* ConditionNode = ParseExpressionNode(Parser, 0, 0);
	if (ConditionNode == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse conditional block expression.");
		goto PARSE_FAIL;
	}

	// Check end character is correct and consume it.
	NextToken = Parser_PeekToken(Parser);
	if (Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
	{
		Parser_ConsumeToken(Parser);
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}
	else
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected ')' token.");
		goto PARSE_FAIL;
	}

	if (IsWhile)
	{
		StatementNode->Statement.While.LoopCondition = ConditionNode;
	}
	else
	{
		StatementNode->Statement.If.EntryCondition = ConditionNode;
	}

	struct AST_Node* ExecNode = ParseDependentStatementNode(Parser);

	if (ExecNode == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse conditional instruction.");
		goto PARSE_FAIL;
	}

	if (IsWhile)
	{
		StatementNode->Statement.While.ExecStatement = ExecNode;

		// Go over all sub-statements of this while loop and link up any break and continue statement that isn't already linked up to something.
		struct Vector Substatements = Vector_Create(struct AST_Node*, 8);
		GetAllStatements(StatementNode->Statement.While.ExecStatement, &Substatements);

		for (int i = 0; i < Substatements.Size; i++)
		{
			struct AST_Node* Substatement = Vector_GetValueAt(Substatements, struct AST_Node*, i);
			ASSERT(Substatement != NULL);

			if (Substatement->Type != AST_NODE_STATEMENT_CONTROL) continue;
			enum TOKEN_KEYWORD Kwd = Substatement->Statement.Control.Keyword;

			if (Kwd != KEYWORD_CONTINUE && Kwd != KEYWORD_BREAK) continue;
			if (Substatement->Statement.Control.TargetStatement != NULL) continue; // Already linked to a sub-loop.

			Substatement->Statement.Control.TargetStatement = StatementNode;
		}

		Vector_Destroy(&Substatements);
	}
	else
	{
		StatementNode->Statement.If.ExecStatement = ExecNode;

		// Check for an else instruction.
		NextToken = Parser_PeekToken(Parser);
		if (Token_IsKeyword(NextToken, KEYWORD_ELSE))
		{
			Parser_ConsumeToken(Parser);
			StatementNode->Statement.If.ExecStatement_Else = ParseDependentStatementNode(Parser);

			if (StatementNode->Statement.If.ExecStatement_Else == NULL)
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
	StatementNode->Statement.For.InitExpression = ParseExpressionNode(Parser, 0, 1);
	if (Parser->HasError) goto PARSE_FAIL;
	StatementNode->Statement.For.LoopCondition = ParseExpressionNode(Parser, 0, 1);	
	if (Parser->HasError) goto PARSE_FAIL;
	StatementNode->Statement.For.PostLoopExpression = ParseExpressionNode(Parser, 0, 0);
	if (Parser->HasError) goto PARSE_FAIL;

	// Check end character is correct and consume it.
	NextToken = Parser_PeekToken(Parser);
	if (Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
	{
		Parser_ConsumeToken(Parser);
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}
	else
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected ')' token.");
		goto PARSE_FAIL;
	}

	StatementNode->Statement.For.ExecStatement = ParseDependentStatementNode(Parser);
	if (StatementNode->Statement.For.ExecStatement == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected statement following for loop declaration.");
		goto PARSE_FAIL;
	}

	// Go over all sub-statements of this while loop and link up any break and continue statement that isn't already linked up to something.
	struct Vector Substatements = Vector_Create(struct AST_Node*, 8);
	GetAllStatements(StatementNode->Statement.For.ExecStatement, &Substatements);

	for (int i = 0; i < Substatements.Size; i++)
	{
		struct AST_Node* Substatement = Vector_GetValueAt(Substatements, struct AST_Node*, i);
		ASSERT(Substatement != NULL);

		if (Substatement->Type != AST_NODE_STATEMENT_CONTROL) continue;
		enum TOKEN_KEYWORD Kwd = Substatement->Statement.Control.Keyword;

		if (Kwd != KEYWORD_CONTINUE && Kwd != KEYWORD_BREAK) continue;
		if (Substatement->Statement.Control.TargetStatement != NULL) continue; // Already linked to a sub-loop.

		Substatement->Statement.Control.TargetStatement = StatementNode;
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

	switch (NextToken->Keyword)
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

	ControlStatementNode->Statement.Control.Keyword = NextToken->Keyword;

	// Parse following expression. No expression is expected for BREAK and CONTINUE. For RETURN, whatever is parsed gets assigned and will be checked by Symbolizer.
	struct AST_Node* ExpressionNode = ParseExpressionNode(Parser, SYMBOL_SEMICOLON, 1);
	if (Parser->HasError)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Error while parsing control keyword expression.");
		goto PARSE_FAIL;
	}

	if (NextToken->Keyword == KEYWORD_RETURN)
	{
		ControlStatementNode->Statement.Control.Expression = ExpressionNode;
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
			StatementNode = ParseExpressionNode(Parser, SYMBOL_SEMICOLON, 1);
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

