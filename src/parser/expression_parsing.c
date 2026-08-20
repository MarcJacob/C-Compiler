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

// Parses a full operator expression from an optional preceding node tree (which can become either the new operator's left operand, or use the new operator as its right operand).
struct AST_Node* ParseOperatorExpression(struct ParserProcess* Parser, struct AST_Node* PrevNode, struct AST_Node* Op)
{
	Parser_Error(Parser, Parser_PeekToken(Parser)->BufferLocation, "Operator parsing unimplemented.");
	return NULL;
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

	ExpressionRootNode = ParseExpressionableNode(Parser);
	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	// Constantly try to update the root node to cover the next expressionables / sub-expression that are found until a stop point is reached.
	for (;;)
	{
		// Check for End Symbol.
		if (Token_IsSymbol(NextToken, EndSymbol))
		{
			Parser_NextToken(Parser);
			break;
		}

		// Get the next node composing the expression.
		struct AST_Node* NextNode = NULL;
		if (Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
		{
			// When encountering an opening parenthesis scope, 
			NextToken = Parser_NextToken(Parser);
			if (NextToken == NULL) goto PARSE_FAIL_EOF;

			// Recursive call to parse the entire sub-expression (IE until a matching closing parenthesis is encountered).
			NextNode = ParseExpressionNode(Parser, SYMBOL_PARENTHESIS_CLOSE);
			if (Parser->HasError) goto PARSE_FAIL;
		}
		else
		{
			NextNode = ParseExpressionableNode(Parser);
		}

		// If no node was successfully parsed, then we encountered an invalid character.
		if (NextNode == NULL)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Unexpected token in expression.");
			goto PARSE_FAIL;
		}

		if (NextNode->Val.Expression.Type == EXP_OP)
		{
			ExpressionRootNode = ParseOperatorExpression(Parser, ExpressionRootNode, NextNode);
			if (ExpressionRootNode == NULL)
			{
				if (Parser->HasError) goto PARSE_FAIL;
				else break;
			}
		}
		else if (ExpressionRootNode == NULL)
		{
			ExpressionRootNode = NextNode;
			continue;
		}
		else // Error case if two non-operator nodes are put in succession.
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Unexpected operand in expression.");
			goto PARSE_FAIL;
		}
	}

	return ExpressionRootNode;
}
