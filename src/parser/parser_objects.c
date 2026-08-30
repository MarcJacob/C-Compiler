// Implementation file for parsing Objects, which include:
// - Variables
// - Functions
// - Structures / Unions / Enums
// - Typedefs

#include "parser.h"

struct AST_Node* ParseObject_Enum_Def(struct ParserProcess* Parser);
struct AST_Node* ParseObject_Struct_Def(struct ParserProcess* Parser);

struct AST_Node* ParseObject_Enum_Def(struct ParserProcess* Parser)
{
	// Expect an opening brace, then start parsing expressions.

	struct AST_Node* EnumNode = AllocASTNode(AST_NODE_OBJ_ENUM);
	EnumNode->Obj.Enum.Members = Vector_Create(struct AST_Node*, 0);

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing struct.");
	PARSE_FAIL:
		FreeASTNode(EnumNode);
		return 0;
	}

	if (!Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN))
	{
		goto PARSE_FAIL;
	}
	Parser_ConsumeToken(Parser); // Consume '{'.

	// Parse comma-separated expressions until closing brace is reached.
	// Expect each expression to be of two possible formats:
	// - VAR_ACCESS (Enum member with auto assignment)
	// - OP ASSIGN (LEFT: VAR_ACCESS, RIGHT: EXPR) (Enum member with explicit assignment).

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	for(;;)
	{
		struct Expression* MemberExpNode = ParseRootExpression(Parser, 1, 0);

		// If expression is NOP (typically happens between the last member's comma and closing bracket), don't do anything.
		if (MemberExpNode->Type != EXP_NOP)
		{
			// Check expression format and add to members vector.
			ui8 ExpressionIsValid = MemberExpNode->Type == EXP_VAR_ACCESS
				|| (MemberExpNode->Type == EXP_OP && MemberExpNode->Op.LeftOperand != NULL
					&& MemberExpNode->Op.LeftOperand->Type == EXP_VAR_ACCESS);

			if (!ExpressionIsValid)
			{
				Parser_Error(Parser, MemberExpNode->BufferLocation, "Invalid enum member declaration.");
				goto PARSE_FAIL;
			}

			Vector_Push(EnumNode->Obj.Enum.Members, struct Expression*, MemberExpNode);
		}

		NextToken = Parser_ConsumeToken(Parser); // Consume whatever ended the expression.
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		if (Token_IsSymbol(NextToken, SYMBOL_OP_COMMA)) continue;
		if (Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE)) break;
		
		// Else...
		Parser_Error(Parser, NextToken->BufferLocation, "Expected '}' token.");
		goto PARSE_FAIL;
	}

	return EnumNode;
}

// Attempts the parsing process for a single structure object.
struct AST_Node* ParseObject_Struct_Def(struct ParserProcess* Parser)
{
	// Expect an opening brace, then start parsing variable, struct / union and enum objects.

	struct AST_Node* StructNode = AllocASTNode(AST_NODE_OBJ_STRUCT);
	StructNode->Obj.Struct.Members = Vector_Create(struct AST_Node*, 2);

	struct Token* NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF while parsing struct.");
	PARSE_FAIL:
		FreeASTNode(StructNode);
		return NULL;
	}

	if (!Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN))
	{
		goto PARSE_FAIL;
	}
	Parser_ConsumeToken(Parser); // Consume '{'.

	// Parse members until the closing brace is reached. Each member slot may itself be a nested sub-structure.
	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	while (!Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE))
	{
		struct TypeSignature* MemberType = AllocTypeSignature();
		if (!ParseTypeSignature(Parser, MemberType))
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Expected type declaration");
			goto PARSE_FAIL;
		}

		struct AST_Node* SubStructNode = NULL; // Cache any sub struct / enum created so it can be selectively left or not left inside the structure.

		// Recursively parse a sub-structure / enum definition if the member type is structured or an enum.
		if (MemberType->Flags & TYPE_IS_STRUCTURED)
		{
			SubStructNode = ParseObject_Struct_Def(Parser);
			if (SubStructNode != NULL)
			{
				SubStructNode->Obj.TypeSignature = MemberType;
				SubStructNode->Obj.Name = String_Copy_ANSI(MemberType->TypeName);
				SubStructNode->Obj.Struct.IsUnion = (MemberType->Flags & TYPE_IS_ENUM_OR_UNION) != 0;
			}
		}
		else if (MemberType->Flags & TYPE_IS_ENUM_OR_UNION)
		{
			struct AST_Node* EnumNode = ParseObject_Enum_Def(Parser);
			if (EnumNode != NULL)
			{
				EnumNode->Obj.TypeSignature = MemberType;

				// Push directly to parser as a root node (enums always exist at the top level).
				Vector_PushPtr(Parser->RootNodes, &EnumNode);
			}
		}
	
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		ui8 ParsedVarObject = 0;

		// Parse objects using the member type.
		while (!Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
		{
			struct AST_Node* NextMember = ParseObject_VarFunc(Parser, MemberType, 0, 0, 1);
			if (NextMember == NULL)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Expected ';' token.");
				goto PARSE_FAIL;
			}

			if (NextMember->Type == AST_NODE_OBJ_FUNC)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Functions are not allowed inside structs / unions.");
				goto PARSE_FAIL;
			}

			ParsedVarObject = 1;
			Vector_Push(StructNode->Obj.Struct.Members, struct AST_Node*, NextMember);

			NextToken = Parser_PeekToken(Parser);
			if (NextToken == NULL) goto PARSE_FAIL_EOF;

			if (Token_IsSymbol(NextToken, SYMBOL_OP_COMMA))
			{
				Parser_ConsumeToken(Parser); // Consume ','.
				NextToken = Parser_PeekToken(Parser);
			}
		}

		Parser_ConsumeToken(Parser); // Consume ';'.

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		// In the case where a sub-structure was declared without following sub-objects,
		// then the whole sub-structure is effectively appended to this one, sharing the sub's members.
		if (!ParsedVarObject && SubStructNode != NULL)
		{
			Vector_Push(StructNode->Obj.Struct.Members, struct AST_Node*, SubStructNode);
		}
		// ... Otherwise add the substructure directly to the root nodes of the Parser process output.
		else if (SubStructNode != NULL)
		{
			Vector_PushPtr(Parser->RootNodes, &SubStructNode);
		}
	}

	Parser_ConsumeToken(Parser); // Consume '}'
	return StructNode;
}

// Attempts the parsing process for a single object, either a variable or a function.
// The passed in Type Signature allocates deep copies of itself for each produced object.
// If AllowEmpty is set, the function will be able to return an empty declarator that's just a wrapper for the given type.
struct AST_Node* ParseObject_VarFunc(struct ParserProcess* Parser, struct TypeSignature* TypeSignature, ui8 AllowEmpty, ui8 AllowInitializer, ui8 AllowBitCount)
{
	ASSERT(TypeSignature != NULL);

	int TokenStartIndex = Parser->TokenIndex;
	struct Token* NextToken = Parser_PeekToken(Parser);
	struct AST_Node* ObjNode = NULL;
	if (NextToken == NULL)
	{
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF.");
	PARSE_FAIL:
		Parser->TokenIndex = TokenStartIndex;
		if (ObjNode != NULL) FreeASTNode(ObjNode);
		return 0;
	}

	// Initialize new object with a default type of Var.
	ObjNode = AllocASTNode(AST_NODE_OBJ_VAR);
	ObjNode->BufferLocation = NextToken->BufferLocation;
	ObjNode->Obj.TypeSignature = AllocTypeSignatureCopy(TypeSignature);

	// Check entry conditions: All non-empty declarators have to start with pointer levels,
	// an identifier or an opening parenthesis (function or function pointer).
	if (!Token_IsSymbol(NextToken, SYMBOL_OP_AMB_STAR)
		&& !Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_OPEN)
		&& NextToken->Type != TOKEN_IDENTIFIER)
	{
		// Return empty declarator if allowed, otherwise consider this a failure (with no error).
		if (AllowEmpty)
			return ObjNode;

		goto PARSE_FAIL;
	}

	while (Token_IsSymbol(NextToken, SYMBOL_OP_AMB_STAR))
	{
		ObjNode->Obj.TypeSignature->PointerLevel++;

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

		ObjNode->Obj.TypeSignature->IsFunctionPointer = 1;

		while (Token_IsSymbol(NextToken, SYMBOL_OP_AMB_STAR) || Token_IsSymbol(NextToken, SYMBOL_OP_DEREF))
		{
			ObjNode->Obj.TypeSignature->FuncPtr.PointerLevel++;
			Parser_ConsumeToken(Parser);
			NextToken = Parser_PeekToken(Parser);
			if (NextToken == NULL) goto PARSE_FAIL_EOF;
		}	
		
		// Handle special case - pre-check for identifier if no stars were found. If no identifier is found either, then skip straight to parsing parameters.
		if (ObjNode->Obj.TypeSignature->FuncPtr.PointerLevel == 0
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
		// ObjNode is a non-function-pointer variable.

		// Check that we weren't expecting a function pointer, and that the declarator doesn't end up being linked to a void type.
		if (ObjNode->Obj.TypeSignature->IsFunctionPointer)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Expected '(' token.");
			goto PARSE_FAIL;
		}
		if (ObjNode->Obj.TypeSignature->Type == DATATYPE_VOID && ObjNode->Obj.TypeSignature->PointerLevel == 0)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier.");
			goto PARSE_FAIL;
		}

		ObjNode->Type = AST_NODE_OBJ_VAR;
	}
	else
	{
		Parser_ConsumeToken(Parser); // Consume '('.

		// ObjNode is a function or function pointer variable.
		// Start recursively parsing datatype + declarator pairs as parameters.

	FUNC_PARAMS_PARSING:
		ObjNode->Type = ObjNode->Obj.TypeSignature->FuncPtr.PointerLevel > 0 ? AST_NODE_OBJ_VAR : AST_NODE_OBJ_FUNC;

		struct Vector Params = Vector_Create(struct AST_Node*, 0);

		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
		while (!Token_IsSymbol(NextToken, SYMBOL_PARENTHESIS_CLOSE))
		{
			struct TypeSignature* ParamDatatype = AllocTypeSignature();
			if (!ParseTypeSignature(Parser, ParamDatatype))
			{
				// This isn't a function / function ptr declarator. It may be a function call so do not error out.
				FreeTypeSignature(ParamDatatype);
				goto PARSE_FAIL;
			}

			struct AST_Node* ParamObj = ParseObject_VarFunc(Parser, ParamDatatype, 1, 0, 0);
			if (ParamObj == NULL)
			{
				if (Parser->HasError) goto PARSE_FAIL;
				break;
			}

			// Any function is automatically given a pointer level and turned into a variable,
			// meaning int foo(int) becomes equivalent to int (*foo)(int) in this specific context.
			if (ParamObj->Type == AST_NODE_OBJ_FUNC)
			{
				ParamObj->Type = AST_NODE_OBJ_VAR;
				
				ParamObj->Obj.TypeSignature->IsFunctionPointer = 1;
				ParamObj->Obj.TypeSignature->FuncPtr.PointerLevel = 1;

				// Extract parameter objects from the function and turn them into function pointer param types.
				for (int ParamIndex = 0; ParamObj->Obj.Func.Params.Size; ParamIndex++)
				{
					// For each param, move the type signature out and into the ParamObj's type signature's function pointer params vector.
					struct AST_Node* SubParamObj = Vector_GetValueAt(ParamObj->Obj.Func.Params, struct AST_Node*, ParamIndex);
					Vector_Push(SubParamObj->Obj.TypeSignature->FuncPtr.ParamTypes, struct TypeSignature*, SubParamObj->Obj.TypeSignature);
					SubParamObj->Obj.TypeSignature = NULL;
				}

				// Discard param objects.
				FreeASTNodeVector(&ParamObj->Obj.Func.Params);
			}

			Vector_Push(Params, struct AST_Node*, ParamObj);

			// Consume comma character and continue.
			NextToken = Parser_PeekToken(Parser);
			if (Token_IsSymbol(NextToken, SYMBOL_OP_COMMA))
			{
				Parser_ConsumeToken(Parser);
				NextToken = Parser_PeekToken(Parser);
				if (NextToken == NULL) goto PARSE_FAIL_EOF;
			}
		}

		if (ObjNode->Type == AST_NODE_OBJ_FUNC)
		{
			ObjNode->Obj.Func.Params = Params;
		}
		else
		{
			// Extract parameter type signatures to being parameters of the function pointer,
			// and discard the objects themselves.

			ObjNode->Obj.TypeSignature->FuncPtr.ParamTypes = Vector_Create(struct TypeSignature, 0);

			for (int ParamIndex = 0; ParamIndex < Params.Size; ParamIndex++)
			{
				struct AST_Node* ParamObj = Vector_GetValueAt(Params, struct AST_Node*, ParamIndex);
				Vector_Push(ObjNode->Obj.TypeSignature->FuncPtr.ParamTypes, struct TypeSignature*, ParamObj->Obj.TypeSignature);
				ParamObj->Obj.TypeSignature = NULL;
			}

			FreeASTNodeVector(&Params);
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

		// If any array sizes are provided, disallow bit count assignment.
		AllowBitCount = 0;

		ObjNode->Obj.Var.ArraySizes = Vector_Create(struct AST_Node*, 0);
		while (Token_IsSymbol(NextToken, SYMBOL_BRACKET_OPEN))
		{
			struct Expression* ArrayExpressionNode = ParseExpressionable_ArrayAccess(Parser);
			if (ArrayExpressionNode == NULL || Parser->HasError)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse array size declaration expression.");
				goto PARSE_FAIL;
			}

			// The node's right operand is the size we're looking for. Take it away from the array expression node and discard the latter.
			Vector_Push(ObjNode->Obj.Var.ArraySizes, struct Expression*, ArrayExpressionNode->Op.RightOperand);
			ArrayExpressionNode->Op.RightOperand = NULL;
			FreeExpression(ArrayExpressionNode);

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
		if (!AllowInitializer)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Initializer not allowed here.");
			goto PARSE_FAIL;
		}

		Parser_ConsumeToken(Parser); // Consume '='.
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		// Check whether we should parse an expression or an initializer list.
		if (Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN))
		{
			Parser_ConsumeToken(Parser); // Consume '{'.

			ObjNode->Obj.Var.InitIsInitializerList = 1;
			ObjNode->Obj.Var.Initializer.List = Vector_Create(struct Expression*, 1);

			for(;;)
			{
				struct Expression* NextExpression = ParseRootExpression(Parser, 1, 0);
				if (NextExpression == NULL || NextExpression->Type == EXP_NOP ||
					Parser->HasError)
				{
					Parser_Error(Parser, NextToken->BufferLocation, "Expected expression.");
					goto PARSE_FAIL;
				}

				NextToken = Parser_ConsumeToken(Parser); // Consume whatever ended the expression.
				if (NextToken == NULL) goto PARSE_FAIL_EOF;

				if (Token_IsSymbol(NextToken, SYMBOL_OP_COMMA))
				{
					Vector_Push(ObjNode->Obj.Var.Initializer.List, struct Expression*, NextExpression);
					continue;
				}

				if (Token_IsSymbol(NextToken, SYMBOL_BRACE_CLOSE))
				{
					break;
				}

				Parser_Error(Parser, NextToken->BufferLocation, "Expected '}' token.");
				goto PARSE_FAIL;
			}
		}
		else
		{
			// Parse an expression initalizer.
			ObjNode->Obj.Var.InitIsInitializerList = 0;
			ObjNode->Obj.Var.Initializer.Expression = ParseRootExpression(Parser, 1, 0);
			if (ObjNode->Obj.Var.Initializer.Expression == NULL || ObjNode->Obj.Var.Initializer.Expression->Type == EXP_NOP)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Expected initializer expression.");
				goto PARSE_FAIL;
			}
		}
	}
	// If the next token is an opening brace, parse a function definition.
	else if (Token_IsSymbol(NextToken, SYMBOL_BRACE_OPEN))
	{
		if (ObjNode->Type != AST_NODE_OBJ_FUNC || !AllowInitializer)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Unexpected function definition.");
			goto PARSE_FAIL;
		}

		ObjNode->Obj.Func.StatementsBlock = ParseBlockStatementNode(Parser);
		if (Parser->HasError || ObjNode->Obj.Func.StatementsBlock == NULL)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Error parsing function definition.");
			goto PARSE_FAIL;
		}
	}
	// If the next token is a colon, parse a bit count.
	else if (Token_IsSymbol(NextToken, SYMBOL_AMB_COLON))
	{
		if (ObjNode->Type != AST_NODE_OBJ_VAR || !AllowBitCount)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Unexpected bit count assignment.");
			goto PARSE_FAIL;
		}

		Parser_ConsumeToken(Parser); // Consume ':'.

		// Parse an expression initializer.
		ObjNode->Obj.Var.InitIsInitializerList = 0;
		ObjNode->Obj.Var.Initializer.Expression = ParseRootExpression(Parser, 0, 0);
		if (Parser->HasError || ObjNode->Obj.Var.Initializer.Expression == NULL)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Error parsing bit count assignment expression.");
			goto PARSE_FAIL;
		}
	}

	return ObjNode;
}

ui8 ParseNextRootObjects(struct ParserProcess* Parser)
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
		if (ObjNode != NULL) FreeASTNode(ObjNode);
		return 0;
	}

	// Check if this "line" of objects is starting with a typedef keyword, in which case all produced objects will be typedefs.
	// This forbids any initializer but does allow structure / enum definitions.

	ui8 IsTypedef = 0;
	if (Token_IsKeyword(NextToken, KEYWORD_TYPEDEF))
	{
		IsTypedef = 1;
		Parser_ConsumeToken(Parser); // Consume 'typedef'.
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;
	}

	struct TypeSignature* ObjectsReturnType = AllocTypeSignature();
	if (!ParseTypeSignature(Parser, ObjectsReturnType))
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Expected type specifier.");
		FreeTypeSignature(ObjectsReturnType);
		goto PARSE_FAIL;
	}

	// If the parsed datatype is structured or an enum, first attempt to parse a definition for it.
	if (ObjectsReturnType->Flags & TYPE_IS_STRUCTURED)
	{
		struct AST_Node* StructNode = ParseObject_Struct_Def(Parser);
		if (StructNode != NULL)
		{
			StructNode->Obj.TypeSignature = ObjectsReturnType;
			StructNode->Obj.Struct.IsUnion = (ObjectsReturnType->Flags & TYPE_IS_ENUM_OR_UNION) > 0;
			StructNode->Obj.Name = String_Copy_ANSI(ObjectsReturnType->TypeName);
			Vector_PushPtr(Parser->RootNodes, &StructNode);
		}
	}
	else if (ObjectsReturnType->Flags & TYPE_IS_ENUM_OR_UNION)
	{
		struct AST_Node* EnumNode = ParseObject_Enum_Def(Parser);
		if (EnumNode != NULL)
		{
			EnumNode->Obj.TypeSignature = ObjectsReturnType;
			Vector_PushPtr(Parser->RootNodes, &EnumNode);
		}
	}

	if (Parser->HasError) goto PARSE_FAIL; // There was an error parsing a struct, union or enum definition.

	NextToken = Parser_PeekToken(Parser);
	if (NextToken == NULL) goto PARSE_FAIL_EOF;

	// Check for early leave if there's nothing beyond the struct / union / enum declaration.
	if (Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		if (IsTypedef)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Expected declaration.");
			goto PARSE_FAIL;
		}

		Parser_ConsumeToken(Parser); // Consume ';'.
		return 1;
	}

	// Loop on the creation and pushing of new variable or function objects until a semicolon or a function definition NOT followed by a comma is encountered.
	while (!Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
	{
		NextToken = Parser_PeekToken(Parser);
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		ObjNode = ParseObject_VarFunc(Parser, ObjectsReturnType, 0, !IsTypedef, 0);
		if (ObjNode == NULL || Parser->HasError)
		{
			Parser_Error(Parser, NextToken->BufferLocation, "Failed to parse object.");
			goto PARSE_FAIL;
		}

		ObjNode->Obj.IsTypedef = IsTypedef;

		Vector_PushPtr(Parser->RootNodes, &ObjNode);

		NextToken = Parser_PeekToken(Parser);

		if (NextToken != NULL && Token_IsSymbol(NextToken, SYMBOL_OP_COMMA))
		{
			Parser_ConsumeToken(Parser); // Consume ','.
			continue;
		}

		if (NextToken != NULL && Token_IsSymbol(NextToken, SYMBOL_SEMICOLON))
		{
			Parser_ConsumeToken(Parser); // Consume ';'.
			break;
		}

		// If we just parsed a function with a definition, then we can break as we didn't encounter a comma making us continue.
		if (ObjNode->Type == AST_NODE_OBJ_FUNC 
			&& ObjNode->Obj.Func.StatementsBlock != NULL)
			break;

		// Otherwise it's an error.
		if (NextToken == NULL) goto PARSE_FAIL_EOF;

		Parser_Error(Parser, NextToken->BufferLocation, "Expected ';' token.");
		goto PARSE_FAIL;
	}
	

	return 1;
}

