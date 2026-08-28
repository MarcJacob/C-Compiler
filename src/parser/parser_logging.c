// Implementation file for Parser / AST logging capabilities.
// Mostly authored by AI.

#include "parser.h"

// AST Printing

static void PrintIndent(ui32 Depth)
{
	for (ui32 i = 0; i < Depth; i++) printf("  ");
}

// Prints a named header for a sub-node of a complex statement (e.g. an IF's CONDITION / THEN / ELSE) before printing the node itself one level deeper.
// Skipped entirely if Node is NULL, so optional sub-nodes don't leave a dangling, empty-looking header.
static void PrintLabeledNode(const char* Label, struct AST_Node* Node, ui32 Depth)
{
	if (Node == NULL) return;

	PrintIndent(Depth);
	printf("[%s]\n", Label);
	PrintNode(Node, Depth + 1);
}

static void PrintParamList(struct Vector* Params);

// Prints a type signature inline.
static void PrintTypeSignature(struct TypeSignature* TypeSig)
{
	printf("%s", TypeSignature_GetName(TypeSig));

	if (TypeSig == NULL) return;

	for (ui8 i = 0; i < TypeSig->PointerLevel; i++) printf("*");

	if (!TypeSig->IsFunctionPointer) return;

	printf("(");
	for (ui8 i = 0; i < TypeSig->FuncPtr.PointerLevel; i++) printf("*");
	printf(")");

	printf("(");
	for (int i = 0; i < TypeSig->FuncPtr.ParamTypes.Size; i++)
	{
		if (i > 0) printf(", ");

		struct TypeSignature* ParamType = Vector_GetValueAt(TypeSig->FuncPtr.ParamTypes, struct TypeSignature*, i);
		PrintTypeSignature(ParamType);
	}
	printf(")");
}

// Prints a function's parameter list inline, comma-separated and wrapped in
// parentheses (eg. "(int a, void (*callback)(int x))"). Always emits parens, even when Params is empty,
// so the declarator still reads as callable rather than looking like a simple pointer.
static void PrintParamList(struct Vector* Params)
{
	printf("(");
	for (int i = 0; i < Params->Size; i++)
	{
		if (i > 0) printf(", ");

		struct AST_Node* Param = Vector_GetValueAt(*Params, struct AST_Node*, i);
		PrintTypeSignature(Param->Obj.TypeSignature);
	}
	printf(")");
}

static void PrintObjNode(struct AST_Node* Node, ui32 Depth)
{
	ui8 IsFunc = Node->Type == AST_NODE_OBJ_FUNC;
	ui8 IsFuncPointer = !IsFunc && Node->Obj.TypeSignature->IsFunctionPointer;

	printf("<%s: ", IsFunc ? "FUNCTION" : (IsFuncPointer ? "FUNC_POINTER" : "VARIABLE"));

	printf("'%s' : ", Node->Obj.Name.Str);
	PrintTypeSignature(Node->Obj.TypeSignature);

	if (IsFunc) PrintParamList(&Node->Obj.Func.Params);
	else
		for (i8 i = 0; i < Node->Obj.Var.ArraySizes.Size; i++)
		{
			printf("[]");
		}

	printf(">\n");

	// Print array size expressions.
	if (Node->Type == AST_NODE_OBJ_VAR && Node->Obj.Var.ArraySizes.Size > 0)
	{
		PrintIndent(Depth + 1);
		printf("[ARRAY SIZE]\n");
		// Parse array index expression(s).
		for (int ArrIndex = 0; ArrIndex < Node->Obj.Var.ArraySizes.Size; ArrIndex++)
		{
			PrintExpression(Vector_GetValueAt(Node->Obj.Var.ArraySizes, struct Expression*, ArrIndex), Depth + 2);
		}
	}

	if (IsFunc)
		PrintLabeledNode("BODY", Node->Obj.Func.StatementsBlock, Depth + 1);
	else if (Node->Obj.Var.InitIsInitializerList)
	{
		PrintIndent(Depth + 1);
		printf("[INIT LIST]\n");
		for (int i = 0; i < Node->Obj.Var.Initializer.List.Size; i++)
		{
			struct Expression* ListExpression = Vector_GetValueAt(Node->Obj.Var.Initializer.List, struct Expression*, i);
			ASSERT(ListExpression != NULL);

			PrintExpression(ListExpression, Depth + 2);
		}
	}
	else if (Node->Obj.Var.Initializer.Expression != NULL)
	{
		PrintIndent(Depth + 1);
		printf("[INIT EXP]\n");
		PrintExpression(Node->Obj.Var.Initializer.Expression, Depth + 2);
	}
}

// Prints an Expression node's specific data and, for operator / function call expressions, recurses into its sub-expressions.
static void PrintExpression(struct Expression* Expression, ui32 Depth)
{
	if (Expression == NULL) return;

	PrintIndent(Depth);

	switch (Expression->Type)
	{
	case EXP_LITERAL_INT:
		printf("<LITERAL_INT: %lld>\n", Expression->Literal.Integer);
		break;
	case EXP_LITERAL_FLOAT:
		printf("<LITERAL_FLOAT: %f>\n", Expression->Literal.Float);
		break;
	case EXP_LITERAL_DOUBLE:
		printf("<LITERAL_DOUBLE: %lf>\n", Expression->Literal.Double);
		break;
	case EXP_LITERAL_STRING:
		printf("<LITERAL_STRING: \"%s\">\n", Expression->Literal.String.Str);
		break;
	case EXP_LITERAL_CHAR:
		printf("<LITERAL_CHAR: '%c'>\n", Expression->Literal.Character);
		break;
	case EXP_VAR_ACCESS:
		printf("<VAR_ACCESS: '%s' : ", Expression->Variable.Name.Str);
		PrintTypeSignature(Expression->ResultType);
		printf(">\n");
		break;
	case EXP_OP:
		printf("<OP: '%s'>\n", Symbol_ToString(Expression->Op.OperatorSymbol));
		PrintExpression(Expression->Op.LeftOperand, Depth + 1);
		PrintExpression(Expression->Op.RightOperand, Depth + 1);
		break;
	case EXP_FUNC_CALL:
		printf("<FUNCTION_CALL: '%s' : ", Expression->FunctionCall.FunctionName.Str);
		PrintTypeSignature(Expression->ResultType);
		printf(">\n");
		for (int i = 0; i < Expression->FunctionCall.Params.Size; i++)
			PrintExpression(Vector_GetValueAt(Expression->FunctionCall.Params, struct Expression*, i), Depth + 1);
		break;
	case EXP_OP_SIZEOF:
		printf("<SIZE_OF>\n");
		PrintExpression(Expression->Sizeof.Operand, Depth + 1);
		break;
	case EXP_OP_CAST:
		printf("<CAST: ");
		PrintTypeSignature(Expression->Cast.TypeSignature);
		printf(">\n");
		PrintExpression(Expression->Cast.Operand, Depth + 1);
		break;
	case EXP_NOP:
		PrintTypeSignature(Expression->ResultType);
		printf("\n");
	}
}

// Prints a single AST node and recurses into its children, indenting each depth level by two spaces.
static void PrintNode(struct AST_Node* Node, ui32 Depth)
{
	if (Node == NULL) return;

	if (Node->Type == AST_NODE_EXPRESSION)
	{
		PrintExpression(Node->Expression, Depth);
		return;
	}

	PrintIndent(Depth);

	if (Node->Type == AST_NODE_OBJ_VAR
		|| Node->Type == AST_NODE_OBJ_FUNC
		|| Node->Type == AST_NODE_OBJ_STRUCT
		|| Node->Type == AST_NODE_OBJ_ENUM)
	{
		if (Node->Obj.IsTypedef)
		{
			printf("[TYPEDEF]");
		}
	}

	switch (Node->Type)
	{
	case AST_NODE_OBJ_STRUCT:
		if (!Node->Obj.Struct.IsUnion)
			printf("<STRUCT: '%s'>\n", Node->Obj.TypeSignature->TypeName.Str);
		else
			printf("<UNION: '%s'>\n", Node->Obj.TypeSignature->TypeName.Str);
		for (int i = 0; i < Node->Obj.Struct.Members.Size; i++)
			PrintNode(Vector_GetValueAt(Node->Obj.Struct.Members, struct AST_Node*, i), Depth + 1);
		break;
	case AST_NODE_OBJ_ENUM:
		printf("<ENUM : '%s'>\n", Node->Obj.TypeSignature->TypeName.Str);
		for (int i = 0; i < Node->Obj.Enum.Members.Size; i++)
			PrintExpression(Vector_GetValueAt(Node->Obj.Enum.Members, struct Expression*, i), Depth + 1);
		break;
	case AST_NODE_OBJ_VAR:
	case AST_NODE_OBJ_FUNC:
		PrintObjNode(Node, Depth);
		break;
	case AST_NODE_STATEMENT_EXP:
		printf("<STATEMENT_EXP>\n");
		PrintNode(Node->Statement.Expression, Depth + 1);
		break;
	case AST_NODE_STATEMENT_CONTROL:
		printf("<%s>\n", Keyword_ToString(Node->Statement.Control.Keyword));
		PrintNode(Node->Statement.Control.Expression, Depth + 1);
		break;
	case AST_NODE_STATEMENT_BLOCK:
		if (Node->Statement.Block.Statements.Size > 0)
		{
			printf("<BLOCK>\n");
			for (int i = 0; i < Node->Statement.Block.Statements.Size; i++)
				PrintNode(Vector_GetValueAt(Node->Statement.Block.Statements, struct AST_Node*, i), Depth + 1);
			PrintIndent(Depth);
			printf("</BLOCK>\n");
		}
		else
		{
			printf("<EMPTY BLOCK>\n");
		}
		break;
	case AST_NODE_STATEMENT_IF:
		printf("<IF>\n");
		PrintLabeledNode("CONDITION", Node->Statement.If.EntryCondition, Depth + 1);
		PrintLabeledNode("THEN", Node->Statement.If.ExecStatement, Depth + 1);
		PrintLabeledNode("ELSE", Node->Statement.If.ExecStatement_Else, Depth + 1);
		break;
	case AST_NODE_STATEMENT_WHILE:
		printf("<WHILE>\n");
		PrintLabeledNode("ENTRY_CONDITION", Node->Statement.While.EntryCondition, Depth + 1);
		PrintLabeledNode("LOOP_CONDITION", Node->Statement.While.LoopCondition, Depth + 1);
		PrintLabeledNode("BODY", Node->Statement.While.ExecStatement, Depth + 1);
		break;
	case AST_NODE_STATEMENT_FOR:
		printf("<FOR>\n");
		PrintLabeledNode("INIT", Node->Statement.For.InitExpression, Depth + 1);
		PrintLabeledNode("CONDITION", Node->Statement.For.LoopCondition, Depth + 1);
		PrintLabeledNode("POST", Node->Statement.For.PostLoopExpression, Depth + 1);
		PrintLabeledNode("BODY", Node->Statement.For.ExecStatement, Depth + 1);
		break;
	case AST_NODE_STATEMENT_OBJ_DEC:
		printf("<OBJ_DEC>\n");
		for (int i = 0; i < Node->Statement.ObjectDeclaration.Objects.Size; i++)
			PrintNode(Vector_GetValueAt(Node->Statement.ObjectDeclaration.Objects, struct AST_Node*, i), Depth + 1);
		break;
	default:
		printf("<UNKNOWN NODE TYPE>\n");
		break;
	}
}

void Parser_PrintTree(struct ParserProcess* Parser)
{
	ASSERT(Parser != NULL);
	ASSERT(Parser->RootNodes != NULL);

	printf("\n===== PARSER OUTPUT =====\n\n");

	for (int i = 0; i < Parser->RootNodes->Size; i++)
	{
		PrintNode(Vector_GetValueAt(*Parser->RootNodes, struct AST_Node*, i), 0);
	}
}

