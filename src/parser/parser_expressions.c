// Implementation file for parsing expressions.

#include "parser.h"

static struct AST_Node* ParseExpressionableNode(struct ParserProcess* Parser, ui8* ParenthesisLevel);

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
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing Function expressionable.");
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

		struct AST_Node* NewExpr = ParseExpressionNode(Parser, 1, 0);
		if (NewExpr == NULL)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse parameter expression.");
			goto PARSE_FAIL;
		}

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		Vector_Push(FunctionExpressionableNode->Expression.FunctionCall.Params, struct AST_Node*, NewExpr);

		Parser_ConsumeToken(Parser); // Consume whatever character caused the param expression to end.
		if (Token_IsSymbol(NextToken, SYMBOL_OP_COMMA)) continue;
		if (Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE)) break;
		// Else...
		Parser_Error(Parser, NextToken->BufferLocation, "Expected ')' token.");
		goto PARSE_FAIL;
	}

	return FunctionExpressionableNode;
}

// Attempts to parse a Ternary operator node with a pre-built Ternary Delimitor operator and a parsed left operand for it.
// The node must then be given its left operator (condition) and its right branch operand (if condition == 0).
static struct AST_Node* ParseExpressionable_Ternary(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;

	struct AST_Node* TernaryNode = NULL;
	struct AST_Node* DelimNode = NULL;
	struct AST_Node* TrueExpNode = NULL;
	struct AST_Node* FalseExpNode = NULL;

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing Ternary expressionable.");
	PARSE_FAIL:
		FreeNode(TernaryNode);
		FreeNode(DelimNode);
		FreeNode(TrueExpNode);
		Parser->TokenIndex = StartTokenIndex;
		return NULL;
	}

	// Expect to begin with a ? operator token.
	if (!Token_IsSymbol(NextToken, SYMBOL_OP_TERNARY_BRANCH))
	{
		goto PARSE_FAIL; // Not a ternary.
	}

	TernaryNode = AllocNewNode(AST_NODE_EXPRESSION);
	TernaryNode->BufferLocation = NextToken->BufferLocation;
	TernaryNode->Expression.Type = EXP_OP;
	TernaryNode->Expression.Op.OperatorSymbol = SYMBOL_OP_TERNARY_BRANCH;

	Parser_ConsumeToken(Parser); // Consume '?'
	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	// Parse true expression, delimitor, then false expression.

	TrueExpNode = ParseExpressionNode(Parser, 0, 0);
	if (TrueExpNode == NULL || TrueExpNode->Expression.Type == EXP_NOP)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected expression following ternary operator.");
		goto PARSE_FAIL;
	}

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	if (!Token_IsSymbol(NextToken, SYMBOL_AMB_COLON))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected ':' token.");
		goto PARSE_FAIL;
	}

	Parser_ConsumeToken(Parser); // Consume ':'.
	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	FalseExpNode = ParseExpressionNode(Parser, 0, 0);
	if (FalseExpNode == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected expression following ternary delimitor.");
		goto PARSE_FAIL;
	}

	DelimNode = AllocNewNode(AST_NODE_EXPRESSION);
	DelimNode->Expression.Type = EXP_OP;
	DelimNode->Expression.Op.OperatorSymbol = SYMBOL_OP_TERNARY_DELIM;
	DelimNode->Expression.Op.LeftOperand = TrueExpNode;
	DelimNode->Expression.Op.RightOperand = FalseExpNode;

	TernaryNode->Expression.Op.RightOperand = DelimNode;
	
	return TernaryNode;
}

static struct AST_Node* ParseExpressionable_Sizeof(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;

	struct AST_Node* SizeofNode = NULL;
	struct AST_Node* OperandNode = NULL;

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing Sizeof expressionable.");
	PARSE_FAIL:
		FreeNode(SizeofNode);
		FreeNode(OperandNode);
		Parser->TokenIndex = StartTokenIndex;
		return NULL;
	}

	// Check that we're starting with a sizeof keyword.
	if (!Token_IsKeyword(NextToken, KEYWORD_SIZEOF))
	{
		goto PARSE_FAIL;
	}
	Parser_ConsumeToken(Parser); // Consume 'sizeof'.
	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	SizeofNode = AllocNewNode(AST_NODE_EXPRESSION);
	SizeofNode->Expression.Type = EXP_OP_SIZEOF;
	SizeofNode->Expression.ResultType = GetPrimitiveDatatypeDef_Int64();

	// If an opening parenthesis is found, attempt to parse a datatype / declarator pair first.
	// If that fails, or no parenthesis are found, parse an expression.
	
	if (Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{
		Parser_ConsumeToken(Parser); // Consume '('.

		struct DatatypeDef Type;
		if (ParseDatatypeDef(Parser, &Type)
			&& (OperandNode = ParseObject_VarFunc(Parser, &Type, 1, 0, 0)) != NULL)
		{
			if (OperandNode->Obj.Name.Length > 0)
			{
				// Deny any named object declarations.
				Parser_Error(Parser, OperandNode->BufferLocation, "Sizeof operand may not be a declaration.");
				goto PARSE_FAIL;
			}
			if (OperandNode->Type == AST_NODE_OBJ_FUNC)
			{
				// Deny function declarations.
				Parser_Error(Parser, OperandNode->BufferLocation, "Sizeof operand may not be a function.");
				goto PARSE_FAIL;
			}

			SizeofNode->Expression.Sizeof.IsDeclarator = 1;
		}
		else
		{
			OperandNode = ParseExpressionNode(Parser, 0, 0);
		}	

		// Check that we have a closing parenthesis after declarator or expression.
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
		if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Expected ')' token.");
			goto PARSE_FAIL;
		}

		Parser_ConsumeToken(Parser); // Consume ')'.
		NextToken = Parser_PeekToken(Parser);
	}
	else
	{
		OperandNode = ParseExpressionNode(Parser, 0, 1);
		if (OperandNode == NULL)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Expected expression.");
			goto PARSE_FAIL;
		}
	}

	SizeofNode->Expression.Sizeof.Operand = OperandNode;
	return SizeofNode;
}

static struct AST_Node* ParseExpressionable_Cast(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;

	struct AST_Node* CastNode = NULL;
	struct AST_Node* ObjNode = NULL;
	struct AST_Node* OperandNode = NULL;

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing Cast expressionable.");
	PARSE_FAIL:
		FreeNode(CastNode);
		FreeNode(ObjNode);
		FreeNode(OperandNode);
		Parser->TokenIndex = StartTokenIndex;
		return NULL;
	}

	// Expect to begin with an anonymous object declarator between parenthesis as the target type.
	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{
		goto PARSE_FAIL;
	}

	Parser_ConsumeToken(Parser); // Consume '('.

	struct DatatypeDef TargetBaseType;
	if (!ParseDatatypeDef(Parser, &TargetBaseType))
	{
		goto PARSE_FAIL; // Don't error out as this could still be a valid expression.
	}

	ObjNode = ParseObject_VarFunc(Parser, &TargetBaseType, 1, 0, 0);
	if (ObjNode == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected type declaration.");
		goto PARSE_FAIL;
	}

	if (ObjNode->Obj.Name.Length > 0)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Target type of a cast cannot be a declaration.");
		goto PARSE_FAIL;
	}

	if (ObjNode->Type == AST_NODE_OBJ_FUNC)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Target type of a cast cannot be a function.");
		goto PARSE_FAIL;
	}

	if (ObjNode->Obj.Var.ArraySizes.Size > 0)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Target type of a cast cannot be an array.");
		goto PARSE_FAIL;
	}

	// Check that declarator is followed by a closing parenthesis.
	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
	{
		// Do not output an error, as this may just be an expression and not a cast.
		goto PARSE_FAIL;
	}

	Parser_ConsumeToken(Parser); // Consume ')'.
	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	// If the cast is followed by an opening parenthesis, get the entire expression within. Otherwise just get the next expressionable.
	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{
		ui8 ParenthesisLevel = 0;
		OperandNode = ParseExpressionableNode(Parser, &ParenthesisLevel); // Parse only the next expressionable as operand. It will effectively be "shielded" from precedence concerns.
	}
	else
	{
		Parser_ConsumeToken(Parser); // Consume '(' so the parsed expression below will stop at the corresponding closing parenthesis.
		OperandNode = ParseExpressionNode(Parser, 0, 0);

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Expected ')' token.");
			goto PARSE_FAIL;
		}
	}

	CastNode = AllocNewNode(AST_NODE_EXPRESSION);
	CastNode->BufferLocation = ObjNode->BufferLocation;
	CastNode->Expression.Type = EXP_OP_CAST;
	CastNode->Expression.Cast.TargetTypeDeclarator = ObjNode;
	CastNode->Expression.Cast.Operand = OperandNode;

	return CastNode;
}

// Parses a single "expression component" from the next tokens and the previously parsed expression.
// An expressionable is an expression node that is either a leaf node, or incomplete in the case of operators.
// They can either be built from a single token or trigger a more complex parsing system that ends up forming
// its own expression tree. The only commonality between expressionables is that they are "atomic" in the way they're parsed.
static struct AST_Node* ParseExpressionableNode(struct ParserProcess* Parser, ui8* ParenthesisLevel)
{
	struct AST_Node* Expressionable = NULL;

PARSE_EXPRESSIONABLE:
	// Handle all valid Expressionable types.
	// For this determination to go a little faster, put the simplest or most specific types at the top
	// since they are faster to discard as a possibility.
	Expressionable = ParseExpressionable_Literal(Parser);
	if (Expressionable == NULL)
		Expressionable = ParseExpressionable_Ternary(Parser);
	if (Expressionable == NULL)
		Expressionable = ParseExpressionable_Sizeof(Parser);
	if (Expressionable == NULL)
		Expressionable = ParseExpressionable_ArrayAccess(Parser);
	if (Expressionable == NULL)
		Expressionable = ParseExpressionable_Cast(Parser);
	if (Expressionable == NULL)
		Expressionable = ParseExpressionable_Function(Parser);
	if (Expressionable == NULL)
		Expressionable = ParseExpressionable_Operator(Parser);
	if (Expressionable == NULL)
		Expressionable = ParseExpressionable_Variable(Parser);

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (Expressionable == NULL && Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{
		Parser_ConsumeToken(Parser); // Consume '('.
		*ParenthesisLevel = *ParenthesisLevel + 1;
		goto PARSE_EXPRESSIONABLE;
	}

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
		// First possibility: Are entry and left_right both in a lower parenthesis level than left ?
		(EntryNodeParenthesisLevel > Left_ParenthesisLevel && Left_Right_ParenthesisLevel > Left_ParenthesisLevel)
		// Second possibility: Do the rules of associativity and precedence mean we should capture the left_right operand ?
		|| (	(Symbol_CompareOpPrecedence(EntryNodeOp, LeftOp) > 0)
			||	(Symbol_CompareOpPrecedence(EntryNodeOp, LeftOp) == 0 
					&& !Symbol_IsOpLeftToRightAssociative(LeftOp)))
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
	if (NeedsRightOperand && Op->Expression.Op.RightOperand == NULL)
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

		// Parse next available expressionable.
		RightOperand = ParseExpressionableNode(Parser, &ParenthesisLevel);
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

	// Constantly try to update the root node to cover the next expressionables / sub-expression that are found until a stop point is reached.
	for (;;)
	{
		NextToken = Parser_PeekToken(Parser);

		// Check for End Symbol if outside a parenthesis scope.
		if (ParenthesisLevel == 0 && 
			(Token_IsSymbol(NextToken, SYMBOL_SEMICOLON) 
				|| (StopAtComma && Token_IsSymbol(NextToken, SYMBOL_OP_COMMA))
				|| Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE)
				|| Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE)
				|| Token_IsSymbol(NextToken, SYMBOL_BRACKET_CLOSE)
				|| Token_IsSymbol(NextToken, SYMBOL_AMB_COLON)))
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
		struct AST_Node* NextNode = ParseExpressionableNode(Parser, &ParenthesisLevel);
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

struct AST_Node* ParseExpressionable_ArrayAccess(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* ArrayAccessNode = NULL;

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing array access expression.");
	PARSE_FAIL:
		if (ArrayAccessNode != NULL) FreeNode(ArrayAccessNode);
		Parser->TokenIndex = StartTokenIndex;
		return NULL;
	}

	// Expect first token to be an opening bracket.
	NextToken = Parser_ConsumeToken(Parser);
	if (!Token_IsSymbol(NextToken, SYMBOL_BRACKET_OPEN))
	{
		goto PARSE_FAIL; // Not an array access.
	}

	// Parse whatever expression follows and expect it to end with a closing bracket.
	struct AST_Node* IndexExpressionNode = ParseExpressionNode(Parser, 0, 0);
	if (IndexExpressionNode == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected expression.");
		goto PARSE_FAIL;
	}

	int ArrayAccessBufferLocation = NextToken->BufferLocation;
	NextToken = Parser_ConsumeToken(Parser);
	if (!Token_IsSymbol(NextToken, SYMBOL_BRACKET_CLOSE))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected ']' token.");
		goto PARSE_FAIL;
	}

	// Create a new Array Access operator node and assign the index expression as its right operand, and leave
	// the left operand empty.

	ArrayAccessNode = AllocNewNode(AST_NODE_EXPRESSION);
	ArrayAccessNode->BufferLocation = NextToken->BufferLocation;
	ArrayAccessNode->Expression.Type = EXP_OP;
	ArrayAccessNode->Expression.Op.OperatorSymbol = SYMBOL_OP_ARRAY_ACCESS;
	ArrayAccessNode->Expression.Op.RightOperand = IndexExpressionNode;
	ArrayAccessNode->Expression.Op.LeftOperand = NULL;

	return ArrayAccessNode;
}
