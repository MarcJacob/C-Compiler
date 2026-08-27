// Implementation file for structure / union / enum parsing.

#include "parser.h"

// Attempts to parse a struct type declaration/definition (named or anonymous), optionally followed by inline
// variable declaration(s) of that type. 
// Fills OutInlineVarObjs with any inline variable objs / declarators produced.
// Fills OutPromotedStructures with any non-anonymous sub-struct.
// Returns NULL, if this isn't a struct declaration or definition at all.
// Check Parser->HasError after a NULL return to tell a real parse error apart from a simple non-match.
static struct AST_Node* ParseStructNode(struct ParserProcess* Parser, struct Vector* OutInlineVarObjs)
{
	ASSERT(OutInlineVarObjs != NULL);

	int StartTokenIndex = Parser->TokenIndex;
	struct AST_Node* StructNode = NULL;

	struct Token* StartToken = Parser_PeekToken(Parser);

	// Let the generic datatype parser handle the "struct Name" / "struct" (anonymous) prefix
	struct DatatypeDef StructType;
	if (!ParseDatatypeDef(Parser, &StructType, 0))
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
		if (StructNode != NULL) FreeNode(StructNode);
		FreeNodeVector(OutInlineVarObjs);
		return NULL;
	}

	if (!(StructType.Flags & DATATYPE_IS_STRUCTURED))
	{
		// Not a struct type.
		goto PARSE_FAIL;
	}

	if (!Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN) && !Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		// Neither a body nor a bare declaration follows, so this is probably a variable declaration.
		goto PARSE_FAIL;
	}

	// From here on this is unambiguously a struct declaration/definition, so any weirdness is a hard error.
	if (StructType.Flags & DATATYPE_IS_VOLATILE)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "A struct declaration cannot be marked volatile.");
		goto PARSE_FAIL;
	}
	if (StructType.PointerLevel != 0)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Unexpected pointer level in struct declaration.");
		goto PARSE_FAIL;
	}

	StructNode = AllocNewNode(AST_NODE_STRUCT);
	StructNode->BufferLocation = StartToken->BufferLocation;
	StructNode->Struct.Type = StructType;
	StructNode->Struct.IsUnion = (StructType.Flags & DATATYPE_IS_STRUCTURED) && (StructType.Flags & DATATYPE_IS_ENUM_OR_UNION);
	StructNode->Struct.Members = Vector_Create(struct AST_Node*, 0);

	if (Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		if (StructType.TypeName.Length == 0)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Anonymous struct requires a body.");
			goto PARSE_FAIL;
		}

		Parser_ConsumeToken(Parser); // Consume ';'.
		return StructNode;
	}

	Parser_ConsumeToken(Parser); // Consume '{'.

	// Parse members until the closing brace is reached. Each member slot may itself be a nested sub-structure.
	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	while (!Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE))
	{
		// First try parsing a substructure, then a variable.
		struct Vector SubInlineVarDecNodes = Vector_Create(struct AST_Node*, 1);
		struct AST_Node* SubStructNode = ParseStructNode(Parser, &SubInlineVarDecNodes);
		if (Parser->HasError)
		{
			goto PARSE_FAIL;
		}

		if (SubStructNode != NULL) // Member is a substructure.
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

	Parser_ConsumeToken(Parser); // Consume '}'.

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	if (Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		Parser_ConsumeToken(Parser);
		return StructNode;
	}

	// Inline variable declaration(s).
	struct DatatypeDef InlineVarType = StructNode->Struct.Type;
	if (!ParseDeclarators(Parser, &StructType, OutInlineVarObjs))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Error parsing struct inline variable declarations.");
		goto PARSE_FAIL;
	}

	return StructNode;
}

ui8 ParseGlobal_Structs(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;

	struct Vector InlineVarDecNodes = Vector_Create(struct AST_Node*, 0);
	struct AST_Node* StructNode = ParseStructNode(Parser, &InlineVarDecNodes);
	if (StructNode == NULL)
	{
		return 0;
	}

	if (StructNode->Struct.Type.TypeName.Length == 0 && InlineVarDecNodes.Size == 0)
	{
		Parser_Error(Parser, StructNode->BufferLocation, "Invalid anonymous struct declaration. Add a type name, or inline variable declarations using it.");
		Parser->TokenIndex = StartTokenIndex;
		return 0;
	}

	Vector_PushPtr(Parser->RootNodes, &StructNode);
	Vector_Append(Parser->RootNodes, &InlineVarDecNodes);
	return 1;
}


