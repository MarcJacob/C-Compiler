#include "parser.h"

// Core Implementation file of the Parser stage, including all other implementation files.

#include "parser_expressions.c"
#include "parser_statements.c"
#include "parser_objects.c"
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

struct AST_Node* AllocNewASTNode(enum AST_NODE_TYPE NodeType)
{
	struct AST_Node* NewNode = calloc(1, sizeof(struct AST_Node));
	ASSERT(NewNode != NULL);

	NewNode->Type = NodeType;
	return NewNode;
}

void FreeExpressionVector(struct Vector* Expressions);
void FreeExpression(struct Expression* Expression)
{
	if (Expression == NULL) return;
	switch (Expression->Type)
	{
	case EXP_OP:
		FreeExpression(Expression->Op.LeftOperand);
		FreeExpression(Expression->Op.RightOperand);
		break;
	case EXP_VAR_ACCESS:
		String_Free_ANSI(&Expression->Variable.Name);
		break;
	case EXP_FUNC_CALL:
		String_Free_ANSI(&Expression->FunctionCall.FunctionName);
		FreeExpressionVector(&Expression->FunctionCall.Params);
		break;
	case EXP_LITERAL_STRING:
		String_Free_ANSI(&Expression->Literal.String);
		break;
	case EXP_OP_CAST:
		FreeASTNode(Expression->Cast.Operand);
		FreeASTNode(Expression->Cast.TargetTypeASTObject);
		break;
	case EXP_OP_SIZEOF:
		FreeASTNode(Expression->Sizeof.Operand);
		break;
	}
}

void FreeExpressionVector(struct Vector* Expressions)
{
	if (Expressions == NULL) return;

	for (int i = 0; i < Expressions->Size; i++)
	{
		FreeExpression(Vector_GetValueAt(*Expressions, struct Expression*, i));
	}
}

// Frees a node and its children recursively.
void FreeASTNode(struct AST_Node* Node)
{
	if (Node == NULL) return;

	switch (Node->Type)
	{
	default:
		break;
	case AST_NODE_EXPRESSION:
		FreeExpression(&Node->Expression);
		break;
	case AST_NODE_OBJ_VAR:
		String_Free_ANSI(&Node->Obj.Name);
		if (Node->Obj.Var.InitIsInitializerList)
			FreeExpressionVector(&Node->Obj.Var.Initializer.List);
		else
			FreeExpression(Node->Obj.Var.Initializer.Expression);
		Vector_Destroy(&Node->Obj.Var.ArraySizes);
		break;
	case AST_NODE_OBJ_FUNC:
		String_Free_ANSI(&Node->Obj.Name);
		FreeNodeVector(&Node->Obj.Func.Params);
		FreeASTNode(Node->Obj.Func.StatementsBlock);	
		break;
	case AST_NODE_OBJ_STRUCT:
		String_Free_ANSI(&Node->Obj.ReturnType.TypeName);
		FreeNodeVector(&Node->Obj.Struct.Members);
		break;
	case AST_NODE_OBJ_ENUM:
		String_Free_ANSI(&Node->Obj.ReturnType.TypeName);
		FreeNodeVector(&Node->Obj.Enum.Members);
		break;
	case AST_NODE_STATEMENT_EXP:
		FreeASTNode(Node->Statement.Expression);
		break;
	case AST_NODE_STATEMENT_CONTROL:
		FreeASTNode(Node->Statement.Control.Expression);
		break;
	case AST_NODE_STATEMENT_IF:
		FreeASTNode(Node->Statement.If.EntryCondition);
		FreeASTNode(Node->Statement.If.ExecStatement);
		FreeASTNode(Node->Statement.If.ExecStatement_Else);
		break;
	case AST_NODE_STATEMENT_WHILE:
		FreeASTNode(Node->Statement.While.EntryCondition);
		FreeASTNode(Node->Statement.While.ExecStatement);
		FreeASTNode(Node->Statement.While.LoopCondition);
		break;
	case AST_NODE_STATEMENT_FOR:
		FreeASTNode(Node->Statement.For.InitExpression);
		FreeASTNode(Node->Statement.For.LoopCondition);
		FreeASTNode(Node->Statement.For.PostLoopExpression);
		FreeASTNode(Node->Statement.For.ExecStatement);
		break;
	case AST_NODE_STATEMENT_BLOCK:
		FreeNodeVector(&Node->Statement.Block.Statements);
		break;
	case AST_NODE_STATEMENT_OBJ_DEC:
		FreeNodeVector(&Node->Statement.ObjectDeclaration.Objects);
		break;
	}

	free(Node);
}

void FreeNodeVector(struct Vector* NodeVec)
{
	ASSERT(NodeVec != NULL);

	for (int i = 0; i < NodeVec->Size; i++)
	{
		FreeASTNode(Vector_GetValueAt(*NodeVec, struct AST_Node*, i));
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
		if (ParseNextRootObjects(Parser))
		{
			// Successfully parsed node tree(s).
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


static ui8 ParseDatatypeDef(struct ParserProcess* Parser, struct DatatypeDef* OutDatatypeDef)
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

	// Perform various flag checks. TODO: Make it so those keywords can be put in any order.

	// Check for static-ness.
	if (NextToken->Keyword == KEYWORD_STATIC)
	{
		Flags |= DATATYPE_IS_STATIC;
		NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume "static"
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

	// Check for extern.
	if (NextToken->Keyword == KEYWORD_EXTERN)
	{
		Flags |= DATATYPE_IS_EXTERN;
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

		OutDatatypeDef->Type = DATATYPE_USER_DEFINED;
		NextToken = (Parser_ConsumeToken(Parser), Parser_PeekToken(Parser)); // Consume "struct"
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
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
		else
		{
			// Generate a procedural name for the type.
			if (Flags & DATATYPE_IS_STRUCTURED)
			{
				if (Flags & DATATYPE_IS_ENUM_OR_UNION)
					OutDatatypeDef->TypeName = String_CreateFormat_ANSI("ANON_UNION_LOC_%d", NextToken->BufferLocation);
				else
					OutDatatypeDef->TypeName = String_CreateFormat_ANSI("ANON_STRUCT_LOC_%d", NextToken->BufferLocation);
			}
			else
			{
				OutDatatypeDef->TypeName = String_CreateFormat_ANSI("ANON_ENUM_LOC_%d", NextToken->BufferLocation);
			}
		}
	}
	else
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Missing type specifier.");
		goto PARSE_FAIL;
	}

	// Apply flags and return.
	OutDatatypeDef->Flags = Flags;

PARSE_SUCCESS:
	return 1;
}