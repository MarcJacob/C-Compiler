#include "parser.h"

// Core Implementation file of the Parser stage, including all other implementation files.

#include "parser_expressions.c"
#include "parser_statements.c"
#include "parser_structs.c"
#include "parser_logging.c"

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
		switch (Node->Expression.Type)
		{
		case EXP_OP:
			FreeNode(Node->Expression.Op.LeftOperand);
			FreeNode(Node->Expression.Op.RightOperand);
			break;
		case EXP_VAR_ACCESS:
			String_Free_ANSI(&Node->Expression.Variable.Name);
			break;
		case EXP_FUNC_CALL:
			String_Free_ANSI(&Node->Expression.FunctionCall.FunctionName);
			FreeNodeVector(&Node->Expression.FunctionCall.Params);
			break;
		case EXP_LITERAL_STRING:
			String_Free_ANSI(&Node->Expression.Literal.String);
			break;
		case EXP_OP_CAST:
			FreeNode(Node->Expression.Cast.Operand);
			FreeNode(Node->Expression.Cast.TargetTypeDeclarator);
			break;
		case EXP_OP_SIZEOF:
			FreeNode(Node->Expression.Sizeof.Operand);
			break;
		}
		break;
	case AST_NODE_OBJ_VAR:
	case AST_NODE_OBJ_FUNC:
		String_Free_ANSI(&Node->Obj.Name);
		FreeNodeVector(&Node->Obj.Func_Params);
		if (Node->Type == AST_NODE_OBJ_FUNC)
		{
			FreeNode(Node->Obj.Func_Block);	
		}
		else
		{
			FreeNode(Node->Obj.Var_InitExpression);
			Vector_Destroy(&Node->Obj.Var_ArraySizes);
		}
		break;
	case AST_NODE_STRUCT:
		String_Free_ANSI(&Node->Struct.Type.TypeName);
		FreeNodeVector(&Node->Struct.Members);
		break;
	case AST_NODE_STATEMENT_EXP:
		FreeNode(Node->Statement.Expression);
		break;
	case AST_NODE_STATEMENT_CONTROL:
		FreeNode(Node->Statement.Control.Expression);
		break;
	case AST_NODE_STATEMENT_IF:
		FreeNode(Node->Statement.If.EntryCondition);
		FreeNode(Node->Statement.If.ExecStatement);
		FreeNode(Node->Statement.If.ExecStatement_Else);
		break;
	case AST_NODE_STATEMENT_WHILE:
		FreeNode(Node->Statement.While.EntryCondition);
		FreeNode(Node->Statement.While.ExecStatement);
		FreeNode(Node->Statement.While.LoopCondition);
		break;
	case AST_NODE_STATEMENT_FOR:
		FreeNode(Node->Statement.For.InitExpression);
		FreeNode(Node->Statement.For.LoopCondition);
		FreeNode(Node->Statement.For.PostLoopExpression);
		FreeNode(Node->Statement.For.ExecStatement);
		break;
	case AST_NODE_STATEMENT_BLOCK:
		FreeNodeVector(&Node->Statement.Block.Statements);
		break;
	case AST_NODE_STATEMENT_VAR_DEC:
		FreeNodeVector(&Node->Statement.VarDeclaration.Declarators);
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

// Main Parser Process function. Turns the SourceTokens vector within the Process into a set of Abstract Syntax Trees (ASTs) whose roots will be put
// in the RootNodes vector in the Process.
void Parser_Run(struct ParserProcess* Parser)
{
	ASSERT(Parser->SourceTokens != NULL);
	ASSERT(Parser->RootNodes != NULL);

	while (Parser_PeekToken(Parser) != NULL)
	{
		if (ParseGlobal_Typedef(Parser) 
			|| ParseGlobal_Object(Parser) 
			|| ParseGlobal_Structs(Parser))
		{
			// Successfully parsed node tree.
		}
		else
		{
			Parser_Error(Parser, Parser_PeekToken(Parser)->BufferLocation, "Expected variable, function or type declaration.");
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

	// Check that the first token can parse either into a standard type specifier or is a typedef alias identifier.
	if ((NextToken->Type != TOKEN_KEYWORD
		|| (!Keyword_IsPrimitiveType(NextToken->Keyword)
			&& !Keyword_IsTypeSpecifier(NextToken->Keyword)
			&& NextToken->Keyword != KEYWORD_STRUCT
			&& NextToken->Keyword != KEYWORD_UNION
			&& NextToken->Keyword != KEYWORD_ENUM))
		&& NextToken->Type != TOKEN_IDENTIFIER)
	{
		// Not a type. Can be interpreted as default type (int32) instead from the call site.
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
	if (NextToken->Keyword == KEYWORD_STATIC)
	{
		Flags |= DATATYPE_IS_STATIC;
		NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume "static"
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

	// Check for const-ness.
	if (NextToken->Keyword == KEYWORD_CONST)
	{
		Flags |= DATATYPE_IS_CONST;
		NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume "const"
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

	// Determine volatility.
	if (NextToken->Keyword == KEYWORD_VOLATILE)
	{
		Flags |= DATATYPE_IS_VOLATILE;
		NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume "volatile"
		if (NextToken == NULL)
		{
			goto PARSE_FAIL_EOF;
		}
	}

	// Handle type aliases (must bind to a typedef at Validation time).
	if (NextToken->Type == TOKEN_IDENTIFIER)
	{
		Flags |= DATATYPE_IS_TYPEDEF;
		OutDatatypeDef->Type = DATATYPE_USER_DEFINED;
		OutDatatypeDef->Flags = Flags;
		OutDatatypeDef->TypeName = String_Copy_ANSI(NextToken->Identifier);

		Parser_ConsumeToken(Parser); // Consume identifier.
		return 1;
	}

	// Handle structured / enumerated types...
	if (NextToken->Keyword == KEYWORD_STRUCT)
	{
		Flags |= DATATYPE_IS_STRUCTURED;
		Flags &= ~DATATYPE_IS_ENUM_OR_UNION;

		OutDatatypeDef->Type = DATATYPE_USER_DEFINED;
		NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume "struct"
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}
	else if (NextToken->Keyword == KEYWORD_UNION)
	{
		Flags |= DATATYPE_IS_STRUCTURED | DATATYPE_IS_ENUM_OR_UNION;

		OutDatatypeDef->Type = DATATYPE_USER_DEFINED;
		NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume "struct"
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}
	else if (NextToken->Keyword == KEYWORD_ENUM)
	{
		Flags &= ~DATATYPE_IS_STRUCTURED;
		Flags |= DATATYPE_IS_ENUM_OR_UNION;

		Parser_Error(Parser, NextToken->BufferLocation, "Enum parsing is unimplemented.");
		goto PARSE_FAIL;
	}

	// Determine signage.
	Flags |= DATATYPE_IS_UNSIGNED * (NextToken->Keyword == KEYWORD_UNSIGNED);
	ui8 SignKeywordPresent = 0;
	if (NextToken->Keyword == KEYWORD_SIGNED
		|| NextToken->Keyword == KEYWORD_UNSIGNED)
	{
		if (Flags & (DATATYPE_IS_STRUCTURED | DATATYPE_IS_ENUM_OR_UNION))
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
		if (NextToken->Type != TOKEN_KEYWORD || !Keyword_IsPrimitiveType(NextToken->Keyword))
		{
			*OutDatatypeDef = GetPrimitiveDatatypeDef_Int32();
		}
	}

	// Next determine size and broad type.
	if (NextToken->Type == TOKEN_KEYWORD)
	{
		if (Flags & (DATATYPE_IS_STRUCTURED | DATATYPE_IS_ENUM_OR_UNION))
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier combination.");
			goto PARSE_FAIL;
		}

		switch (NextToken->Keyword)
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
			if (NextToken->Type != TOKEN_KEYWORD || NextToken->Keyword != KEYWORD_LONG)
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
	else if (Flags & (DATATYPE_IS_STRUCTURED | DATATYPE_IS_ENUM_OR_UNION))
	{
		if (NextToken->Type == TOKEN_IDENTIFIER)
		{
			OutDatatypeDef->TypeName = String_Copy_ANSI(NextToken->Identifier);
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

// Performs the parsing process for a single declarator / obj node.
struct AST_Node* ParseDeclarator(struct ParserProcess* Parser, struct DatatypeDef* ReturnType)
{
	ASSERT(ReturnType != NULL);

	int TokenStartIndex = Parser->TokenIndex;
	struct Token* NextToken = Parser_PeekToken(Parser);
	struct AST_Node* ObjNode = NULL;
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF.");
	PARSE_FAIL:
		Parser->TokenIndex = TokenStartIndex;
		if (ObjNode != NULL) FreeNode(ObjNode);
		return 0;
	}

	if (Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN)) goto PARSE_FAIL;


	// For each declarator, parse the following:
	// - A set of star symbols, one for each declarator-specific pointer level.
	// - An identifier, either directly or between parenthesis, or none.
	//	- In the latter case first get more star symbols, at least one being required, and each adding a function pointer level.
	// - Either a semicolon (declaration), an opening parenthesis (function or function pointer) or an assignment operator (initializer expression).
	// - In the second case, start parsing comma separated datatype & sub-declarators as the function parameters until a closing parenthesis is reached.
	// - In the other two cases, assign the right operand of the assignment operator as the initializer expression (if any).

	ObjNode = AllocNewNode(AST_NODE_OBJ_VAR);
	ObjNode->BufferLocation = NextToken->BufferLocation;
	ObjNode->Obj.ReturnType = *ReturnType;

	// Check entry conditions: All non-empty declarators have to start with pointer levels,
	// an identifier or an opening parenthesis.
	if (!Token_IsSymbol(NextToken, SYMBOL_OP_AMB_STAR)
		&& !Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN)
		&& NextToken->Type != TOKEN_IDENTIFIER)
	{
		// Return empty declarator.
		return ObjNode;
	}

	while (Token_IsSymbol(NextToken, SYMBOL_OP_AMB_STAR))
	{
		ObjNode->Obj.ReturnType.PointerLevel++;

		Parser_ConsumeToken(Parser);
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

	// Parse opening parenthesis and function pointer levels.
	ui8 IdentifierInParenthesis = 0;
	if (Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{	
		IdentifierInParenthesis = 1;

		Parser_ConsumeToken(Parser);
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		while (Token_IsSymbol(NextToken, SYMBOL_OP_AMB_STAR) || Token_IsSymbol(NextToken, SYMBOL_OP_DEREF))
		{
			ObjNode->Obj.FuncPointerLevel++;
			Parser_ConsumeToken(Parser);
			NextToken = Parser_PeekToken(Parser);
			if (NextToken == NULL) goto PARSE_FAIL_EOF;
		}	
		
		// Handle special case - pre-check for identifier if no stars were found. If no identifier is found either, then skip straight to parsing parameters.
		if (ObjNode->Obj.FuncPointerLevel == 0
			&& NextToken->Type != TOKEN_IDENTIFIER)
		{
			// No star characters and no identifier = skip to parameters.
			goto FUNC_PARAMS_PARSING;
		}
	}

	// Parse identifier.
	if (NextToken->Type == TOKEN_IDENTIFIER)
	{
		ObjNode->Obj.Name = String_Copy_ANSI(NextToken->Identifier);	
		
		Parser_ConsumeToken(Parser);
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

	// Ensure we have a closing parenthesis if there was an opening and consume it.
	if (IdentifierInParenthesis)
	{
		if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Expected ')' token.");
			goto PARSE_FAIL;
		}

		Parser_ConsumeToken(Parser);
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

	if (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN))
	{
		// ObjNode is a variable.

		// Check that we weren't expecting a function pointer, and that the declarator doesn't end up being linked to a void type.
		if (ObjNode->Obj.FuncPointerLevel > 0)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Expected '(' token.");
			goto PARSE_FAIL;
		}
		if (ObjNode->Obj.ReturnType.Type == DATATYPE_VOID && ObjNode->Obj.ReturnType.PointerLevel == 0)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier.");
			goto PARSE_FAIL;
		}

		ObjNode->Type = AST_NODE_OBJ_VAR;
	}
	else
	{
		Parser_ConsumeToken(Parser);

		// ObjNode is a function or function pointer variable.
		// Start recursively parsing datatype + declarator pairs as parameters.

FUNC_PARAMS_PARSING:
		ObjNode->Obj.Func_Params = Vector_Create(struct AST_Node*, 0);
		ObjNode->Type = ObjNode->Obj.FuncPointerLevel > 0 ? AST_NODE_OBJ_VAR : AST_NODE_OBJ_FUNC;

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
		while (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
		{
			struct DatatypeDef ParamDatatype = { 0 };
			if (!ParseDatatypeDef(Parser, &ParamDatatype, 0))
			{
				// This isn't a function / function ptr declarator. It may be a function call so do not error out.
				goto PARSE_FAIL;
			}

			struct AST_Node* ParamObj = ParseDeclarator(Parser, &ParamDatatype);
			if (ParamObj == NULL)
			{
				if (Parser->HasError) goto PARSE_FAIL;
				break;
			}

			// Any function is automatically given a pointer level and turned into a variable,
			// meaning int (*foo)(int) becomes equivalent to int foo(int) in this specific context.
			if (ParamObj->Type == AST_NODE_OBJ_FUNC)
			{
				ParamObj->Obj.FuncPointerLevel = 1;
				ParamObj->Type = AST_NODE_OBJ_VAR;
			}

			Vector_Push(ObjNode->Obj.Func_Params, struct AST_Node*, ParamObj);

			// Consume comma character and continue.
			NextToken = Parser_PeekToken(Parser);
			if (Token_IsSymbol(NextToken, SYMBOL_OP_COMMA))
			{
				Parser_ConsumeToken(Parser);
				NextToken = Parser_PeekToken(Parser);
				if (NextToken == NULL) goto PARSE_FAIL_EOF;
			}
		}

		// Consume closing parenthesis.
		Parser_ConsumeToken(Parser);
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

	// If the declarator is a variable, then accept any number of array size declarations.
	if (Token_IsSymbol(NextToken, SYMBOL_BRACKET_OPEN))
	{
		if (ObjNode->Type == AST_NODE_OBJ_FUNC)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Cannot declare array of functions.");
			goto PARSE_FAIL;
		}

		ObjNode->Obj.Var_ArraySizes = Vector_Create(struct AST_Node*, 0);
		while (Token_IsSymbol(NextToken, SYMBOL_BRACKET_OPEN))
		{
			struct AST_Node* ArrayExpressionNode = ParseExpressionable_ArrayAccess(Parser);
			if (ArrayExpressionNode == NULL || Parser->HasError)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse array size declaration expression.");
				goto PARSE_FAIL;
			}

			// The node's right operand is the size we're looking for. Take it away from the array expression node and discard the latter.
			Vector_Push(ObjNode->Obj.Var_ArraySizes, struct AST_Node*, ArrayExpressionNode->Expression.Op.RightOperand);
			ArrayExpressionNode->Expression.Op.RightOperand = NULL;
			FreeNode(ArrayExpressionNode);

			NextToken = Parser_PeekToken(Parser);
			if (NextToken == NULL) goto PARSE_FAIL_EOF;
		}

	}
	
	// If the next token is an assignment operator, then we have an initialization expression. Parse it up until the next-encountered comma.
	if (Token_IsSymbol(NextToken, SYMBOL_OP_ASSIGN))
	{
		if (ObjNode->Type == AST_NODE_OBJ_FUNC)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Unexpected '=' token.");
			goto PARSE_FAIL;
		}

		Parser_ConsumeToken(Parser);
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		ObjNode->Obj.Var_InitExpression = ParseExpressionNode(Parser, 1, 0);
		if (ObjNode->Obj.Var_InitExpression == NULL || ObjNode->Obj.Var_InitExpression->Expression.Type == EXP_NOP)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Expected initializer expression.");
			goto PARSE_FAIL;
		}
	}

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	return ObjNode;
}

// Parses a set of obj / declarator nodes into the OutObjNodes vector.
// Returns 1 if at least one declarator was parsed successfully.
ui8 ParseDeclarators(struct ParserProcess* Parser, struct DatatypeDef* ReturnType, struct Vector* OutObjNodes)
{
	ASSERT(OutObjNodes != NULL);

	int TokenStartIndex = Parser->TokenIndex;
	struct Token* NextToken = Parser_PeekToken(Parser);
	struct AST_Node* ObjNode = NULL;
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF.");
	PARSE_FAIL:
		Parser->TokenIndex = TokenStartIndex;
		if (ObjNode != NULL) FreeNode(ObjNode);
		return 0;
	}

	ObjNode = ParseDeclarator(Parser, ReturnType);
	if (ObjNode == NULL) goto PARSE_FAIL;

	while(ObjNode != NULL)
	{
		// Push new declarator to vector.
		Vector_PushPtr(OutObjNodes, &ObjNode);

		NextToken = Parser_PeekToken(Parser);
		if (Token_IsSymbol(NextToken, SYMBOL_SEMICOLON) || Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN)) break;

		if (Token_IsSymbol(NextToken, SYMBOL_OP_COMMA)) Parser_ConsumeToken(Parser);

		ObjNode = ParseDeclarator(Parser, ReturnType);
	}

	NextToken = Parser_PeekToken(Parser);
	if ( Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		// Consume final semicolon.
		Parser_ConsumeToken(Parser);
	}

	return 1;
}

ui8 ParseGlobal_Object(struct ParserProcess* Parser)
{
	int TokenStartIndex = Parser->TokenIndex;
	struct Token* NextToken = Parser_PeekToken(Parser);
	struct AST_Node* ObjNode = NULL;
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF.");
	PARSE_FAIL:
		Parser->TokenIndex = TokenStartIndex;
		if (ObjNode != NULL) FreeNode(ObjNode);
		return 0;
	}

	struct DatatypeDef ReturnType = { 0 };
	if (!ParseDatatypeDef(Parser, &ReturnType, 1))
	{
		goto PARSE_FAIL;
	}

	struct Vector ObjNodes = Vector_Create(struct AST_Node*, 1);
	if (!ParseDeclarators(Parser, &ReturnType, &ObjNodes))
	{
		goto PARSE_FAIL;
	}

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	// TODO: Check validity of top-level objects.

	// If the token reached after parsing the declarators is an opening bracket, it must constitute the function definition for the last declarator that was parsed.
	if (Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN))
	{
		if (ObjNodes.Size == 0)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Expected ; token.");
			goto PARSE_FAIL;
		}
		struct AST_Node* LastObj = *(struct AST_Node**)Vector_GetLastPtr(&ObjNodes);
		if (LastObj->Type != AST_NODE_OBJ_FUNC)
		{
			// Not a function.
			Parser_Error(Parser, NextToken->BufferLocation, "Expected ; token.");
			goto PARSE_FAIL;
		}

		// Parse the function's block statement.
		LastObj->Obj.Func_Block = ParseBlockStatementNode(Parser);
		if (LastObj->Obj.Func_Block == NULL)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Error parsing function block.");
			goto PARSE_FAIL;
		}
	}

	Vector_Append(Parser->RootNodes, &ObjNodes);
	Vector_Destroy(&ObjNodes);
	return 1;
}

ui8 ParseGlobal_Typedef(struct ParserProcess* Parser)
{
	int TokenStartIndex = Parser->TokenIndex;
	struct Token* NextToken = Parser_PeekToken(Parser);
	struct AST_Node* TypedefNode = NULL;
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF.");
	PARSE_FAIL:
		Parser->TokenIndex = TokenStartIndex;
		if (TypedefNode != NULL) FreeNode(TypedefNode);
		return 0;
	}

	if (!Token_IsKeyword(NextToken, KEYWORD_TYPEDEF))
	{
		goto PARSE_FAIL;
	}

	Parser_ConsumeToken(Parser); // Consume typedef keyword.
	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	// Parse a datatype def - declarator pair stopped by a semicolon.
	struct DatatypeDef Type;
	if (!ParseDatatypeDef(Parser, &Type, 1))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected type declaration.");
		goto PARSE_FAIL;
	}

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	struct AST_Node* DeclaratorNode = ParseDeclarator(Parser, &Type);
	if (DeclaratorNode == NULL)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected declaration.");
		goto PARSE_FAIL;
	}

	// Ensure the end character of the declarator is a semicolon. TODO: Allow compounded typedefs separated by commas.
	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;
	if (!Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected ';' token.");
		goto PARSE_FAIL;
	}
	Parser_ConsumeToken(Parser); // Consume ';' token.

	// "Extract" the declarator from the node then free the node.
	TypedefNode = AllocNewNode(AST_NODE_TYPEDEF);
	TypedefNode->BufferLocation = DeclaratorNode->BufferLocation;

	// Declarator Node's Obj data can be moved into the Typedef Node's, then discarded without freeing.
	TypedefNode->Typedef.Declarator = DeclaratorNode->Obj;

	Vector_PushPtr(Parser->RootNodes, &TypedefNode);
	return 1;
}
