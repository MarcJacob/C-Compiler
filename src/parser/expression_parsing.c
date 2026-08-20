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
		LiteralNode->Val.Expression.Type = EXP_LITERAL_CHAR;
		LiteralNode->Val.Expression.Literal.Character = NextToken->Val.LiteralCharacter;
		LiteralNode->Val.Expression.ResultType = GetPrimitiveDatatypeDef_Char();
		break;
	case TOKEN_LITERAL_NUMBER_INT:
		LiteralNode->Val.Expression.Type = EXP_LITERAL_INT;
		LiteralNode->Val.Expression.Literal.Integer = NextToken->Val.LiteralNumber.Integer;
		LiteralNode->Val.Expression.ResultType = GetPrimitiveDatatypeDef_Int64();
		break;
	case TOKEN_LITERAL_NUMBER_FLOAT:
		LiteralNode->Val.Expression.Type = EXP_LITERAL_FLOAT;
		LiteralNode->Val.Expression.Literal.Float = NextToken->Val.LiteralNumber.Float;
		LiteralNode->Val.Expression.ResultType = GetPrimitiveDatatypeDef_Float();
		break;
	case TOKEN_LITERAL_NUMBER_DOUBLE:
		LiteralNode->Val.Expression.Type = EXP_LITERAL_DOUBLE;
		LiteralNode->Val.Expression.Literal.Double = NextToken->Val.LiteralNumber.Double;
		LiteralNode->Val.Expression.ResultType = GetPrimitiveDatatypeDef_Double();
		break;
	case TOKEN_LITERAL_STRING:
		LiteralNode->Val.Expression.Type = EXP_LITERAL_STRING;
		LiteralNode->Val.Expression.Literal.String = NextToken->Val.LiteralString;
		LiteralNode->Val.Expression.ResultType = GetPrimitiveDatatypeDef_String();
		break;
	default:
		FreeNode(LiteralNode);
		return NULL;
	}

	// Consume token and return node.
	Parser_NextToken(Parser);
	return LiteralNode;
}

// Attempts to parse the next available token as a variable read expression node.
static struct AST_Node* ParseExpressionable_Variable(struct ParserProcess* Parser)
{
	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) return NULL;

	// For now we accept any encountered identifier as a variable read.
	// When implementing function call parsing, make sure to check that there isn't
	// an opening parenthesis right after the identifier.
	if (NextToken->Type != TOKEN_IDENTIFIER)
	{
		return NULL;
	}

	struct AST_Node* VarNode = AllocNewNode(AST_NODE_EXPRESSION);
	VarNode->BufferLocation = NextToken->BufferLocation;

	VarNode->Val.Expression.Type = EXP_VARIABLE;
	VarNode->Val.Expression.Variable.Name = NextToken->Val.LiteralString;

	// Consume token and return.
	Parser_NextToken(Parser);
	return VarNode;
}

// Attempts to parse the next available token as an operator expression node, WITHOUT attempting to parse
// the next tokens for an operand.
static struct AST_Node* ParseExpressionable_Operator(struct ParserProcess* Parser)
{
	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) return NULL;

	if (NextToken->Type != TOKEN_SYMBOL 
		|| (!Symbol_IsBinaryOperator(NextToken->Val.Symbol) 
			&& !Symbol_IsUnaryOperator(NextToken->Val.Symbol)))
	{
		return NULL;
	}

	// Parse next token as an operator node.
	struct AST_Node* OpExpression = AllocNewNode(AST_NODE_EXPRESSION);
	OpExpression->BufferLocation = NextToken->BufferLocation;
	OpExpression->Val.Expression.Type = EXP_OP;
	OpExpression->Val.Expression.Op.OperatorSymbol = NextToken->Val.Symbol;

	// Consume token and return.
	Parser_NextToken(Parser);
	return OpExpression;
}

// Attempts to parse the next tokens as a function call, starting sub-expression parsing processes
// for each parameter, delimited by commas (effectively overriding the standard nature of the comma operator).
static struct AST_Node* ParseExpressionable_Function(struct ParserProcess* Parser)
{
	// TODO...
	// Don't forget to stop the function name being interpreted as a variable name
	// in ParseExpressionable_Variable.
	return NULL;
}

// Parses a single "expression component" from the next tokens and the previously parsed expression.
// An expressionable is an expression node that is either a leaf node, or incomplete in the case of operators.
static struct AST_Node* ParseExpressionableNode(struct ParserProcess* Parser)
{
	struct AST_Node* Expressionable = NULL;

	// Handle all valid Expressionable types.
	Expressionable = ParseExpressionable_Literal(Parser);
	if (Expressionable == NULL)
		Expressionable = ParseExpressionable_Variable(Parser);
	if (Expressionable == NULL)
		Expressionable = ParseExpressionable_Operator(Parser);
	if (Expressionable == NULL)
		Expressionable = ParseExpressionable_Function(Parser);

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

	struct AST_Node* LeftOperand = RootExpression->Val.Expression.Op.LeftOperand;
	ASSERT(LeftOperand != NULL);

	if (LeftOperand->Val.Expression.Type != EXP_OP)
	{
		// Left operand is not an operator itself.
		return RootExpression;
	}

	if (RootExpression->Val.Expression.ParenthesisLevel < LeftOperand->Val.Expression.ParenthesisLevel)
	{
		// Left operand is inside an extra parenthesis scope.
		return RootExpression;
	}

	// If left operand's operator has a LOWER precedence than root expression's, then it should act first.
	// To achieve this, left operand's right operand becomes root's left operand, root becomes left operand's right operand,
	// and left operand becomes the new root.
	while (LeftOperand != NULL
		&& LeftOperand->Val.Expression.Type == EXP_OP
		&& Symbol_CompareOpPrecedence(RootExpression->Val.Expression.Op.OperatorSymbol, 
			LeftOperand->Val.Expression.Op.OperatorSymbol) == 1)
	{
		struct AST_Node* Left_Right = LeftOperand->Val.Expression.Op.RightOperand;
		LeftOperand->Val.Expression.Op.RightOperand = RootExpression;
		RootExpression->Val.Expression.Op.LeftOperand = Left_Right;

		RootExpression = LeftOperand;
		LeftOperand = RootExpression->Val.Expression.Op.LeftOperand;
	}

	return RootExpression;
}

// Handles parsing the right operand of a newly-parsed operator expressionable, if required.
// The expressionable must have its parenthesis level and left operand assigned.
// OutParenthesisLevel allows keeping track of current parenthesis level after the function returns.
// Returns the root of the resulting expression tree.
static struct AST_Node* ParseOperatorExpression(struct ParserProcess* Parser, struct AST_Node* Op, ui8* OutParenthesisLevel)
{
	ASSERT(Op != NULL);
	ASSERT(Op->Val.Expression.Type == EXP_OP);

	enum TOKEN_SYMBOL OpSymbol = Op->Val.Expression.Op.OperatorSymbol;

	// Check error case: Unary left operator given a left operand or unary right / binary operator NOT given a left operand.
	ui8 NeedsLeftOperand = Symbol_IsRightUnaryOperator(OpSymbol) || Symbol_IsBinaryOperator(OpSymbol);
	if (NeedsLeftOperand != (Op->Val.Expression.Op.LeftOperand != NULL))
	{
		Parser_Error(Parser, Op->BufferLocation, "Invalid operands for operator.");
		return NULL;
	}

	ui8 NeedsRightOperand = Symbol_IsLeftUnaryOperator(OpSymbol) || Symbol_IsBinaryOperator(OpSymbol);

	// Initialize parenthesis level to whatever the entry Operator has been assigned to.
	ui8 ParenthesisLevel = Op->Val.Expression.ParenthesisLevel;
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
			Parser_NextToken(Parser);
			NextToken = Parser_PeekToken(Parser);
			if (NextToken == NULL) goto PARSE_FAIL_EOF;

			ParenthesisLevel++;
		}

		RightOperand = ParseExpressionableNode(Parser);
		if (RightOperand == NULL)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Unexpected token while parsing right operand for operator.");
			goto PARSE_FAIL;
		}

		RightOperand->Val.Expression.ParenthesisLevel = ParenthesisLevel;

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		// If right operand expressionable is itself an operator, recursively call this function on it.
		if (RightOperand->Val.Expression.Type == EXP_OP)
		{
			// The right operand is parsed without a left operand for itself meaning it will only work with left unary operators.
			Op->Val.Expression.Op.RightOperand = ParseOperatorExpression(Parser, RightOperand, &ParenthesisLevel);
			if (Op->Val.Expression.Op.RightOperand == NULL)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse right operand operator expression.");
				goto PARSE_FAIL;
			}
		}
		else
		{
			Op->Val.Expression.Op.RightOperand = RightOperand;
		}

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		// Check for closing parenthesis up until reaching back out of our own parenthesis level.
		while (Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
		{
			if (ParenthesisLevel < Op->Val.Expression.ParenthesisLevel) break;
			ParenthesisLevel--;

			Parser_NextToken(Parser);
			NextToken = Parser_PeekToken(Parser);
			if (NextToken == NULL) goto PARSE_FAIL_EOF;
		}
	}

	// If a left operand is present, handle precedence between it and this operator expression.
	if (NeedsLeftOperand)
	{
		Op = HandleOperatorPrecedence(Op);
	}

	*OutParenthesisLevel = ParenthesisLevel;
	return Op;
}

// Entry point of expression parsing. Parses a "root expression" until reaching an end character (closing parenthesis or semicolon).
struct AST_Node* ParseExpressionNode(struct ParserProcess* Parser, enum TOKEN_SYMBOL EndSymbol)
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

	// Constantly try to update the root node to cover the next expressionables / sub-expression that are found until a stop point is reached.
	for (;;)
	{
		NextToken = Parser_PeekToken(Parser);

		// Check for End Symbol if outside a parenthesis scope.
		if (ParenthesisLevel == 0 && Token_IsSymbol(NextToken, EndSymbol))
		{
			Parser_NextToken(Parser);
			break;
		}

		// Check for opening parenthesis.
		while (Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
		{
			Parser_NextToken(Parser);
			NextToken = Parser_PeekToken(Parser);
			if (NextToken == NULL) goto PARSE_FAIL_EOF;

			ParenthesisLevel++;
		}

		// Get the next node composing the expression.
		struct AST_Node* NextNode = ParseExpressionableNode(Parser);
		if (NextNode == NULL)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Unexpected token in expression.");
			goto PARSE_FAIL;
		}
		NextNode->BufferLocation = NextToken->BufferLocation;
		NextNode->Val.Expression.ParenthesisLevel = ParenthesisLevel;

		if (NextNode->Val.Expression.Type == EXP_OP)
		{
			// If we're here then it means parsing has just started or that the previous node can be operated on (as opposed
			// to an incomplete operator node which can't yet), so it can be put into the new node's left operand slot right away.
			NextNode->Val.Expression.Op.LeftOperand = ExpressionRootNode;

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
			Parser_NextToken(Parser);
			NextToken = Parser_PeekToken(Parser);
			if (NextToken == NULL) goto PARSE_FAIL_EOF;

			ParenthesisLevel--;

			if (ParenthesisLevel == 0) break;
		}
	}

	return ExpressionRootNode;
}
