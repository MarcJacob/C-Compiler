// Implementation file for structure / union / enum parsing.

#include "parser.h"

static struct AST_Node* ParseStructOrEnumNode(struct ParserProcess* Parser, struct Vector* OutInlineVarObjs);

// Parses an enumeration node into the provided pre-allocated node pointer and returns inline-declared variables along with it.
static ui8 ParseEnumNode(struct ParserProcess* Parser, struct AST_Node* EnumNode, struct Vector* OutInlineVarObjs)
{
	ASSERT(EnumNode != NULL);
	ASSERT(EnumNode->Type == AST_NODE_ENUM);
	ASSERT(OutInlineVarObjs != NULL);

	EnumNode->Enum.Members = Vector_Create(struct AST_Node*, 0);

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing struct.");
	PARSE_FAIL:
		FreeNode(EnumNode);
		return 0;
	}

	// Parse comma-separated expressions until closing brace is reached.
	// Expect each expression to be of two possible formats:
	// - VAR_ACCESS (Enum member with auto assignment)
	// - OP ASSIGN (LEFT: VAR_ACCESS, RIGHT: EXPR) (Enum member with explicit assignment).

	Parser_ConsumeToken(Parser); // Consume '{'.

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	for(;;)
	{
		struct AST_Node* MemberExpNode = ParseExpressionNode(Parser, 1, 0);
		if (MemberExpNode == NULL)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Expected expression.");
			goto PARSE_FAIL;
		}

		// If expression is NOP (typically happens between the last member's comma and closing bracket), don't do anything.
		if (MemberExpNode->Expression.Type != EXP_NOP)
		{
			// Check expression format and add to members vector.
			ui8 ExpressionIsValid = MemberExpNode->Expression.Type == EXP_VAR_ACCESS
				|| (MemberExpNode->Expression.Type == EXP_OP && MemberExpNode->Expression.Op.LeftOperand != NULL
					&& MemberExpNode->Expression.Op.LeftOperand->Expression.Type == EXP_VAR_ACCESS);

			if (!ExpressionIsValid)
			{
				Parser_Error(Parser, MemberExpNode->BufferLocation, "Invalid enum member declaration.");
				goto PARSE_FAIL;
			}

			Vector_Push(EnumNode->Enum.Members, struct AST_Node*, MemberExpNode);
		}

		NextToken = Parser_ConsumeToken(Parser); // Consume whatever ended the expression.
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		if (Token_IsSymbol(NextToken, SYMBOL_OP_COMMA)) continue;
		if (Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE)) break;
		
		// Else...
		Parser_Error(Parser, NextToken->BufferLocation, "Expected '}' token.");
		goto PARSE_FAIL;
	}

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	if (Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		Parser_ConsumeToken(Parser);
		return 1;
	}

	// Inline variable declaration(s).
	struct DatatypeDef InlineVarType = EnumNode->Enum.Type;
	ParseDeclarators(Parser, &InlineVarType, OutInlineVarObjs);
	if (Parser->HasError)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Error parsing enum inline variable declarations.");
		goto PARSE_FAIL;
	}

	return 1;
}

// Parses a structure node (and child structures / enums recursively) into the provided pre-allocated node pointer,
// and returns inline-declared variables along with it.
static ui8 ParseStructNode(struct ParserProcess* Parser, struct AST_Node* StructNode, struct Vector* OutInlineVarObjs)
{	
	ASSERT(StructNode != NULL);
	ASSERT(StructNode->Type == AST_NODE_STRUCT);
	ASSERT(OutInlineVarObjs != NULL);

	StructNode->Struct.Members = Vector_Create(struct AST_Node*, 0);

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing struct.");
	PARSE_FAIL:
		FreeNode(StructNode);
		return 0;
	}

	Parser_ConsumeToken(Parser); // Consume '{'.

	// Parse members until the closing brace is reached. Each member slot may itself be a nested sub-structure.
	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	while (!Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE))
	{
		// First try parsing a substructure, then a variable.
		struct Vector SubInlineVarDecNodes = Vector_Create(struct AST_Node*, 1);
		struct AST_Node* SubStructNode = ParseStructOrEnumNode(Parser, &SubInlineVarDecNodes);
		if (Parser->HasError)
		{
			goto PARSE_FAIL;
		}

		if (SubStructNode != NULL) // Member is a substructure or subenum.
		{
			Vector_PushPtr(&StructNode->Struct.Members, &SubStructNode);
			Vector_Append(&StructNode->Struct.Members, &SubInlineVarDecNodes);
		}
		else // Member is a var declaration statement.
		{
			struct AST_Node* MemberVarDecNode = ParseVariableDeclarationStatementNode(Parser, SYMBOL_SEMICOLON);
			Vector_Push(StructNode->Struct.Members, struct AST_Node*, MemberVarDecNode);
		}

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

	Parser_ConsumeToken(Parser); // Consume '}'

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	if (Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		Parser_ConsumeToken(Parser);
		return 1;
	}

	// Inline variable declaration(s).
	struct DatatypeDef InlineVarType = StructNode->Struct.Type;
	ParseDeclarators(Parser, &InlineVarType, OutInlineVarObjs);
	if (Parser->HasError)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Error parsing struct inline variable declarations.");
		goto PARSE_FAIL;
	}

	return 1;
}

// Attempts to parse a struct / union / enum type declaration/definition (named or anonymous), optionally followed by inline
// variable declaration(s) of that type. 
// Fills OutInlineVarObjs with any inline variable objs / declarators produced.
// Returns NULL, if this isn't a struct declaration or definition at all.
// Check Parser->HasError after a NULL return to tell a real parse error apart from a simple non-match.
static struct AST_Node* ParseStructOrEnumNode(struct ParserProcess* Parser, struct Vector* OutInlineVarObjs)
{
	ASSERT(OutInlineVarObjs != NULL);

	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* NewNode = NULL;

	struct Token* StartToken = Parser_PeekToken(Parser);

	// Let the generic datatype parser handle the "struct Name" / "struct" (anonymous) prefix
	struct DatatypeDef Type;
	if (!ParseDatatypeDef(Parser, &Type, 0))
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
		if (NewNode != NULL) FreeNode(NewNode);
		FreeNodeVector(OutInlineVarObjs);
		return NULL;
	}

	if (!Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN) && !Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		// Neither a body nor a bare declaration follows, so this is probably a variable declaration.
		goto PARSE_FAIL;
	}

	if (!(Type.Flags & DATATYPE_IS_STRUCTURED) 
		&& !(Type.Flags & DATATYPE_IS_ENUM_OR_UNION))
	{
		// Not a struct or type.
		goto PARSE_FAIL;
	}

	// From here on this is unambiguously a struct or union or enum declaration/definition, so any weirdness is a hard error.
	if (Type.Flags & DATATYPE_IS_VOLATILE)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "A struct / union / enum declaration cannot be marked volatile.");
		goto PARSE_FAIL;
	}
	if (Type.PointerLevel != 0)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Unexpected pointer level in struct / union / enum declaration.");
		goto PARSE_FAIL;
	}

	// Initialize node.
	if (Type.Flags & DATATYPE_IS_STRUCTURED)
	{
		// Node is a struct or union.
		NewNode = AllocNewNode(AST_NODE_STRUCT);
		NewNode->Struct.Type = Type;
		NewNode->Struct.IsUnion = (Type.Flags & DATATYPE_IS_STRUCTURED) && (Type.Flags & DATATYPE_IS_ENUM_OR_UNION);
	}
	else
	{
		// Node is an enum.
		NewNode = AllocNewNode(AST_NODE_ENUM);
		NewNode->BufferLocation = StartToken->BufferLocation;
		NewNode->Enum.Type = Type; // Move type string into the structure.
	}

	if (Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		if (Type.TypeName.Length == 0)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Anonymous struct / union / enum requires a body.");
			goto PARSE_FAIL;
		}

		Parser_ConsumeToken(Parser); // Consume ';'.
		return NewNode;
	}

	// Branch down two possible paths: Parse a Struct Node or an Enum node.
	if (NewNode->Type == AST_NODE_ENUM)
	{
		if (!ParseEnumNode(Parser, NewNode, OutInlineVarObjs))
		{
			Parser_Error(Parser, Parser_PeekToken(Parser)->BufferLocation, "Error parsing enum.");
			goto PARSE_FAIL;
		}
	}
	else
	{
		// Type is a structure or union.
		if (!ParseStructNode(Parser, NewNode, OutInlineVarObjs))
		{
			Parser_Error(Parser, Parser_PeekToken(Parser)->BufferLocation, "Error parsing structure.");
			goto PARSE_FAIL;
		}
	}

	return NewNode;
}

ui8 ParseGlobal_Struct_Union_Enum(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;

	struct Vector InlineVarDecNodes = Vector_Create(struct AST_Node*, 0);
	struct AST_Node* NewNode = ParseStructOrEnumNode(Parser, &InlineVarDecNodes);
	if (NewNode == NULL)
	{
		return 0;
	}

	if (NewNode->Struct.Type.TypeName.Length == 0 && InlineVarDecNodes.Size == 0)
	{
		Parser_Error(Parser, NewNode->BufferLocation, "Invalid anonymous struct declaration. Add a type name, or inline variable declarations using it.");
		Parser->TokenIndex = StartTokenIndex;
		return 0;
	}

	Vector_PushPtr(Parser->RootNodes, &NewNode);
	Vector_Append(Parser->RootNodes, &InlineVarDecNodes);
	return 1;
}


