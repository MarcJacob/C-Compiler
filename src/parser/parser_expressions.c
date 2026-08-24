// Implementation file for parsing expressions.

#include "parser.h"

// Attempts to parse the next available token as a literal expression.
static struct AST_Node* ParseExpressionable_Literal(struct ParserProcess* Parser)
{
	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) return NULL;

	struct AST_Node* LiteralNode = AllocNewNode(AST_NODE_EXPRESSION);
	LiteralNode->BufferLocation = NextToken->BufferLocation;

	switch (NextToken->Type)
	{
	case TOKEN_LITERAL_CHAR:
		LiteralNode->Expression.Type = EXP_LITERAL_CHAR;
		LiteralNode->Expression.Literal.Character = NextToken->LiteralCharacter;
		LiteralNode->Expression.ResultType = GetPrimitiveDatatypeDef_Char();
		break;
	case TOKEN_LITERAL_NUMBER_INT:
		LiteralNode->Expression.Type = EXP_LITERAL_INT;
		LiteralNode->Expression.Literal.Integer = NextToken->LiteralNumber.Integer;
		LiteralNode->Expression.ResultType = GetPrimitiveDatatypeDef_Int64();
		break;
	case TOKEN_LITERAL_NUMBER_FLOAT:
		LiteralNode->Expression.Type = EXP_LITERAL_FLOAT;
		LiteralNode->Expression.Literal.Float = NextToken->LiteralNumber.Float;
		LiteralNode->Expression.ResultType = GetPrimitiveDatatypeDef_Float();
		break;
	case TOKEN_LITERAL_NUMBER_DOUBLE:
		LiteralNode->Expression.Type = EXP_LITERAL_DOUBLE;
		LiteralNode->Expression.Literal.Double = NextToken->LiteralNumber.Double;
		LiteralNode->Expression.ResultType = GetPrimitiveDatatypeDef_Double();
		break;
	case TOKEN_LITERAL_STRING:
		LiteralNode->Expression.Type = EXP_LITERAL_STRING;
		LiteralNode->Expression.Literal.String = String_Copy_ANSI(NextToken->LiteralString);
		LiteralNode->Expression.ResultType = GetPrimitiveDatatypeDef_String();
		break;
	default:
		FreeNode(LiteralNode);
		return NULL;
	}

	// Consume token and return node.
	Parser_ConsumeToken(Parser);
	return LiteralNode;
}

// Attempts to parse the next available token as a variable read expression node.
static struct AST_Node* ParseExpressionable_Variable(struct ParserProcess* Parser)
{
	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) return NULL;

	if (NextToken->Type != TOKEN_IDENTIFIER)
	{
		return NULL;
	}

	struct AST_Node* VarNode = AllocNewNode(AST_NODE_EXPRESSION);
	VarNode->BufferLocation = NextToken->BufferLocation;

	VarNode->Expression.Type = EXP_VAR_ACCESS;
	VarNode->Expression.Variable.Name = NextToken->LiteralString;

	// Consume token and return.
	Parser_ConsumeToken(Parser);
	return VarNode;
}

// Attempts to parse the next available token as an operator expression node, WITHOUT attempting to parse
// the next tokens for an operand.
static struct AST_Node* ParseExpressionable_Operator(struct ParserProcess* Parser)
{
	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) return NULL;

	if (NextToken->Type != TOKEN_SYMBOL 
		|| (!Symbol_IsBinaryOp(NextToken->Symbol) 
			&& !Symbol_IsUnaryOp(NextToken->Symbol)))
	{
		return NULL;
	}

	// Parse next token as an operator node.
	struct AST_Node* OpExpression = AllocNewNode(AST_NODE_EXPRESSION);
	OpExpression->BufferLocation = NextToken->BufferLocation;
	OpExpression->Expression.Type = EXP_OP;
	OpExpression->Expression.Op.OperatorSymbol = NextToken->Symbol;

	// Consume token and return.
	Parser_ConsumeToken(Parser);
	return OpExpression;
}

// Attempts to parse the next tokens as a function call, starting sub-expression parsing processes
// for each parameter, delimited by commas (effectively overriding the standard nature of the comma operator).
static struct AST_Node* ParseExpressionable_Function(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;

	struct AST_Node* FunctionExpressionableNode = NULL;

	struct Token* NextToken = Parser_ConsumeToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing function expressionable.");
	PARSE_FAIL:
		if (FunctionExpressionableNode != NULL) FreeNode(FunctionExpressionableNode);
		Parser->TokenIndex = StartTokenIndex;
		return NULL;
	}
	
	// Look for an identifier, an opening parenthesis, comma-separated sub-expressions and a closing parenthesis.
	if (NextToken->Type != TOKEN_IDENTIFIER)
	{
		goto PARSE_FAIL;
	}

	struct Token* IdentifierToken = NextToken;
	NextToken = Parser_ConsumeToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{
		goto PARSE_FAIL;
	}

	FunctionExpressionableNode = AllocNewNode(AST_NODE_EXPRESSION);
	FunctionExpressionableNode->BufferLocation = IdentifierToken->BufferLocation;
	FunctionExpressionableNode->Expression.Type = EXP_FUNC_CALL;
	FunctionExpressionableNode->Expression.FunctionCall.FunctionName = IdentifierToken->Identifier;

	// Start parsing param expressions until a closing parenthesis is reached.
	FunctionExpressionableNode->Expression.FunctionCall.Params = Vector_Create(struct AST_Node*, 0);

	for (NextToken = Parser_PeekToken(Parser);
		!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE); NextToken = Parser_PeekToken(Parser))
	{
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		struct AST_Node* NewExpr = ParseExpressionNode(Parser, SYMBOL_OP_COMMA, 1);
		if (NewExpr == NULL)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse parameter expression.");
			goto PARSE_FAIL;
		}

		Vector_Push(FunctionExpressionableNode->Expression.FunctionCall.Params, struct AST_Node*, NewExpr);
	}

	NextToken = Parser_ConsumeToken(Parser);
	return FunctionExpressionableNode;
}

// Parses a single "expression component" from the next tokens and the previously parsed expression.
// An expressionable is an expression node that is either a leaf node, or incomplete in the case of operators.
static struct AST_Node* ParseExpressionableNode(struct ParserProcess* Parser)
{
	struct AST_Node* Expressionable = NULL;

	// Handle all valid Expressionable types.
	Expressionable = ParseExpressionable_Literal(Parser);
	if (Expressionable == NULL)
		Expressionable = ParseExpressionable_Function(Parser);
	if (Expressionable == NULL)
		Expressionable = ParseExpressionable_Operator(Parser);
	if (Expressionable == NULL)
		Expressionable = ParseExpressionable_Variable(Parser);

	if (Expressionable == NULL)
		return NULL;

	return Expressionable;
}

// Handles operator precedence between a "root" operator expression, and its left operand.
// If the left operand is itself an operator expression with an operator of LOWER precedence (and the same parenthesis level),
// then the root becomes the left operand's right operand, itself becoming the root's left operand, and the left operand becomes the new root (return value).
static struct AST_Node* HandleOperatorPrecedence(struct AST_Node* RootExpression)
{
	ASSERT(RootExpression != NULL);

	struct AST_Node* LeftOperand = RootExpression->Expression.Op.LeftOperand;
	ASSERT(LeftOperand != NULL);

	if (LeftOperand->Expression.Type != EXP_OP)
	{
		// Left operand is not an operator itself.
		return RootExpression;
	}

	if (LeftOperand->Expression.Op.RightOperand == NULL)
	{
		// Left operand has no right operand - no precedence can apply.
		return RootExpression;
	}

	if (RootExpression->Expression.ParenthesisLevel < LeftOperand->Expression.ParenthesisLevel)
	{
		// Left operand is inside an extra parenthesis scope.
		return RootExpression;
	}

	// Cache the "entry root" into the actual expression we'll be descending into the left expression tree.
	// RootExpression will be set to the first left operand that was promoted to be the root. 
	struct AST_Node* EntryNode = RootExpression;

	struct AST_Node* Left_Right_Operand = LeftOperand->Expression.Op.RightOperand;
	enum TOKEN_SYMBOL LeftOp = LeftOperand->Expression.Op.OperatorSymbol;
	enum TOKEN_SYMBOL EntryNodeOp = EntryNode->Expression.Op.OperatorSymbol;

	ui8 Left_ParenthesisLevel = LeftOperand->Expression.ParenthesisLevel;
	ui8 Left_Right_ParenthesisLevel = LeftOperand->Expression.Op.RightOperand->Expression.ParenthesisLevel;
	ui8 EntryNodeParenthesisLevel = EntryNode->Expression.ParenthesisLevel;

	struct AST_Node* ParentNode = NULL;

	// Repeatedly attempt to perform a swap.
	// It must avoid crossing a right-to-left / left-to-right boundary, parenthesis levels and
	// left operand must have LOWER precedence if the two have the same associativity, or it must be right-to-left and root left-to-right.
	while (
		// Check parenthesis level (Is entry at a deeper or similar level along with the left operand's right operand ?)
		(EntryNodeParenthesisLevel >= Left_Right_ParenthesisLevel)
		// Are we strictly deeper in parenthesis than the left operand OR do the rules of associativity and precedence mean we should capture the left_right operand ?
		&& (EntryNodeParenthesisLevel > Left_ParenthesisLevel
			|| (Symbol_CompareOpPrecedence(EntryNodeOp, LeftOp) > 0)
			|| (Symbol_CompareOpPrecedence(EntryNodeOp, LeftOp) == 0 && !Symbol_IsOpLeftToRightAssociative(LeftOp)))
		)
	{
		// Perform swap.
		LeftOperand->Expression.Op.RightOperand = EntryNode;
		EntryNode->Expression.Op.LeftOperand = Left_Right_Operand;

		// If this is the first swap, assign Left Operand as the new Root Expression before we lose track of it.
		if (RootExpression == EntryNode) RootExpression = LeftOperand;

		if (ParentNode != NULL)
		{
			ParentNode->Expression.Op.RightOperand = LeftOperand;
		}
		ParentNode = LeftOperand;

		LeftOperand = Left_Right_Operand;

		// Check break conditions.
		if (LeftOperand == NULL) break; // The Left Operand lacked its own right operand.
		if (LeftOperand->Expression.Type != EXP_OP) break; // The new left operand is not an operator.
		if (EntryNode->Expression.ParenthesisLevel < LeftOperand->Expression.ParenthesisLevel) break; // The new left operand is on a deeper parenthesis level.

		Left_Right_Operand = LeftOperand->Expression.Op.RightOperand;
		if (Left_Right_Operand == NULL) break; // The new left operand does not have a right operand.

		// Update check / cached values.
		LeftOp = LeftOperand->Expression.Op.OperatorSymbol;
		Left_ParenthesisLevel = LeftOperand->Expression.ParenthesisLevel;
		Left_Right_ParenthesisLevel = LeftOperand->Expression.Op.RightOperand->Expression.ParenthesisLevel;
	}

	return RootExpression;
}

// Handles parsing the right operand of a newly-parsed operator expressionable, if required.
// The expressionable must have its parenthesis level and left operand assigned.
// OutParenthesisLevel allows keeping track of current parenthesis level after the function returns.
// Returns the root of the resulting expression tree.
// End Symbol is used so in the case of parsing the next expressionable as a right operator, we can determine when
// to use an empty NOP node instead.
static struct AST_Node* ParseOperatorExpression(struct ParserProcess* Parser, struct AST_Node* Op, ui8* OutParenthesisLevel)
{
	ASSERT(Op != NULL);
	ASSERT(Op->Expression.Type == EXP_OP);

	enum TOKEN_SYMBOL OpSymbol = Op->Expression.Op.OperatorSymbol;

	// Check error case: Unary left operator given a left operand or unary right / binary operator NOT given a left operand.
	ui8 NeedsLeftOperand = Symbol_IsRightUnaryOp(OpSymbol) && !Symbol_IsBinaryOp(OpSymbol);
	ui8 SupportsLeftOperand = NeedsLeftOperand || Symbol_IsBinaryOp(OpSymbol);
	if (!SupportsLeftOperand && Op->Expression.Op.LeftOperand != NULL)
	{
		Parser_Error(Parser, Op->BufferLocation, "Unexpected left operand for operator.");
		return NULL;
	}
	// Check error case: Unary right operator not given a left operand.
	if (NeedsLeftOperand && Op->Expression.Op.LeftOperand == NULL)
	{
		Parser_Error(Parser, Op->BufferLocation, "Left operand required for operator.");
		return NULL;	}

	ui8 NeedsRightOperand = !Symbol_IsRightUnaryOp(OpSymbol);

	// We can now deduce the exact kind of operator we're dealing with. Deambiguate as needed.
	if (Op->Expression.Op.LeftOperand != NULL && NeedsRightOperand)
	{
		OpSymbol = Symbol_DeambiguateBinaryOp(OpSymbol);
	}
	else if (NeedsRightOperand)
	{
		OpSymbol = Symbol_DeambiguateLeftUnaryOp(OpSymbol);
	}
	else 
	{
		OpSymbol = Symbol_DeambiguateRightUnaryOp(OpSymbol);
	}

	Op->Expression.Op.OperatorSymbol = OpSymbol;

	// Initialize parenthesis level to whatever the entry Operator has been assigned to.
	ui8 ParenthesisLevel = Op->Expression.ParenthesisLevel;
	if (NeedsRightOperand)
	{
		// New operator requires a right operand.
		struct AST_Node* RightOperand = NULL;

		struct Token* NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL)
		{
		PARSE_FAIL_EOF:
			Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing expression.");
		PARSE_FAIL:
			if (RightOperand != NULL) FreeNode(RightOperand);
			return NULL;
		}

		// Look for opening parenthesis.
		while (Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
		{
			Parser_ConsumeToken(Parser);
			NextToken = Parser_PeekToken(Parser);
			if (NextToken == NULL) goto PARSE_FAIL_EOF;

			ParenthesisLevel++;
		}

		// Parse next available expressionable.
		RightOperand = ParseExpressionableNode(Parser);
		if (RightOperand == NULL)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Unexpected token while parsing right operand for operator.");
			goto PARSE_FAIL;
		}

		RightOperand->Expression.ParenthesisLevel = ParenthesisLevel;

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		// If right operand expressionable is itself an operator, recursively call this function on it.
		if (RightOperand->Expression.Type == EXP_OP)
		{
			// The right operand is parsed without a left operand for itself meaning it will only work with left unary operators.
			Op->Expression.Op.RightOperand = ParseOperatorExpression(Parser, RightOperand, &ParenthesisLevel);
			if (Op->Expression.Op.RightOperand == NULL)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse right operand operator expression.");
				goto PARSE_FAIL;
			}
		}
		else
		{
			Op->Expression.Op.RightOperand = RightOperand;
		}

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		// Check for closing parenthesis up until reaching back out of our own parenthesis level.
		while (Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
		{
			if (ParenthesisLevel == 0) break; // Can happen when closing parenthesis is to be used as end symbol.
			if (ParenthesisLevel < Op->Expression.ParenthesisLevel) break;

			ParenthesisLevel--;

			Parser_ConsumeToken(Parser);
			NextToken = Parser_PeekToken(Parser);
			if (NextToken == NULL) goto PARSE_FAIL_EOF;

		}
	}

	// If a left operand is present, handle precedence between it and this operator expression.
	if (Op->Expression.Op.LeftOperand != NULL)
	{
		Op = HandleOperatorPrecedence(Op);
	}

	*OutParenthesisLevel = ParenthesisLevel;
	return Op;
}

// Entry point of expression parsing. Parses a "root expression" until reaching an end symbol (';' or ',' if specified) or a closing parenthesis
// that does not match an opening parenthesis inside the expression itself.
// Returns an NOP expression if no expression could be parsed at all, or NULL if there was a parser error.
struct AST_Node* ParseExpressionNode(struct ParserProcess* Parser, ui8 StopAtComma, ui8 ConsumeStopChar)
{
	int TokenStartIndex = Parser->TokenIndex;

	struct AST_Node* ExpressionRootNode = NULL;
		struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing expression.");
	PARSE_FAIL:
		if (ExpressionRootNode != NULL) FreeNode(ExpressionRootNode);
		return NULL;
	}

	ui8 ParenthesisLevel = 0;

	// Check for opening parenthesis, before the first expressionable is parsed.
	// After that, it can only increase while parsing the right operand of an operator.
	while (Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{
		Parser_ConsumeToken(Parser);
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		ParenthesisLevel++;
	}

	// Constantly try to update the root node to cover the next expressionables / sub-expression that are found until a stop point is reached.
	for (;;)
	{
		NextToken = Parser_PeekToken(Parser);

		// Check for End Symbol if outside a parenthesis scope.
		// Temp: Also stop on a closing parenthesis regardless, but do not consume it if it's not set as the end symbol.
		// TODO: Rework this function so that it can accept multiple end symbols cleanly.
		if (ParenthesisLevel == 0 && (Token_IsSymbol(NextToken, SYMBOL_SEMICOLON) || (StopAtComma && Token_IsSymbol(NextToken, SYMBOL_OP_COMMA))))
		{
			if (ConsumeStopChar)
			{
				Parser_ConsumeToken(Parser);
			}
			break;
		}
		else if (ParenthesisLevel == 0 && Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
		{
			break;
		}

		// Get the next node composing the expression.
		struct AST_Node* NextNode = ParseExpressionableNode(Parser);
		if (NextNode == NULL)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Unexpected token in expression.");
			goto PARSE_FAIL;
		}
		NextNode->BufferLocation = NextToken->BufferLocation;
		NextNode->Expression.ParenthesisLevel = ParenthesisLevel;

		if (NextNode->Expression.Type == EXP_OP)
		{
			// If we're here then it means parsing has just started or that the previous node can be operated on (as opposed
			// to an incomplete operator node which can't yet), so it can be put into the new node's left operand slot right away.
			NextNode->Expression.Op.LeftOperand = ExpressionRootNode;

			// Continue parsing to obtain the right operand of this operator.
			ExpressionRootNode = ParseOperatorExpression(Parser, NextNode, &ParenthesisLevel);
			if (ExpressionRootNode == NULL)
			{
				if (Parser->HasError) goto PARSE_FAIL;
				else break;
			}
		}
		else if (ExpressionRootNode == NULL)
		{
			ExpressionRootNode = NextNode;
		}
		else // Error case if two non-operator nodes are put in succession.
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Unexpected operand in expression.");
			goto PARSE_FAIL;
		}

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		// Check for closing parenthesis.
		while (Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
		{
			if (ParenthesisLevel == 0) break;

			Parser_ConsumeToken(Parser);
			NextToken = Parser_PeekToken(Parser);
			if (NextToken == NULL) goto PARSE_FAIL_EOF;

			ParenthesisLevel--;
		}
	}

	// If parsing was valid but failed to turn up any expression, return a NOP expression.
	if (ExpressionRootNode == NULL)
	{
		ExpressionRootNode = AllocNewNode(AST_NODE_EXPRESSION);
		ExpressionRootNode->BufferLocation = NextToken->BufferLocation;
		ExpressionRootNode->Expression.Type = EXP_NOP;
	}

	return ExpressionRootNode;
}
