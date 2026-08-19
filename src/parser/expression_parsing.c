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
}

struct AST_Node* ParseExpressionNode(struct ParserProcess* Parser, enum TOKEN_SYMBOL EndSymbol)
{
	// ...
	Parser_Error(Parser, Parser_PeekToken(Parser)->BufferLocation, "Expression parsing STILL not implemented.");
	return NULL;
}
