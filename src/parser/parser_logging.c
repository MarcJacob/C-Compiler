// Implementation file for Parser / AST logging capabilities.
// Mostly authored by AI.

#include "parser.h"

// AST Printing

static void PrintNode(struct AST_Node* Node, ui32 Depth);

static void PrintIndent(ui32 Depth)
{
	for (ui32 i = 0; i < Depth; i++) printf("  ");
}

// Prints a datatype's name followed by one '*' character per pointer level (eg. "int**" for a pointer to pointer to int).
static void PrintDatatypeName(const struct DatatypeDef* Datatype)
{
	printf("%s", Datatype_GetName(Datatype));
	for (ui8 i = 0; i < Datatype->PointerLevel; i++) printf("*");
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
static void PrintObj_AsParam(struct ObjDeclarator* Param)
{
	PrintDatatypeName(&Param->ReturnType);
	if (Param->FuncPointerLevel > 0)
	{
		printf(" (");
		for (i8 j = 0; j < Param->FuncPointerLevel; j++) printf("*");
		if (Param->Name.Length > 0) printf("%s", Param->Name.Str);
		printf(")");
		PrintParamList(&Param->Func_Params);
	}
	else if (Param->Name.Length > 0)
	{
		printf(" %s", Param->Name.Str);
	}
}

// Prints a function / function pointer's parameter list inline, comma-separated and wrapped in
// parentheses (eg. "(int a, void (*callback)(int x))"). Always emits parens, even when Params is empty,
// so the declarator still reads as callable rather than looking like a simple pointer.
static void PrintParamList(struct Vector* Params)
{
	printf("(");
	for (int i = 0; i < Params->Size; i++)
	{
		if (i > 0) printf(", ");

		struct ObjDeclarator* Param = &Vector_GetValueAt(*Params, struct AST_Node*, i)->Obj;
		PrintObj_AsParam(Param);
	}
	printf(")");
}

// Prints an ObjDeclarator's name and type (datatype name, pointer stars, function pointer
// dereference parens if FuncPointerLevel > 0, and an inline parameter list if IsFuncLike).
// Shared by Obj nodes (which wrap this with a VARIABLE/FUNCTION/FUNC_POINTER label and a body /
// initializer) and Typedef nodes (which wrap it with a plain TYPEDEF label and nothing else).
static void PrintObjDeclarator(struct ObjDeclarator* Declarator, ui8 IsFuncLike)
{
	printf("'%s' : ", Declarator->Name.Str);
	PrintDatatypeName(&Declarator->ReturnType);
	if (Declarator->FuncPointerLevel > 0)
	{
		printf(" (");
		for (i8 i = 0; i < Declarator->FuncPointerLevel; i++) printf("*");
		printf(")");
	}

	if (IsFuncLike) PrintParamList(&Declarator->Func_Params);

	for (i8 i = 0; i < Declarator->Var_ArraySizes.Size; i++)
	{
		printf("[]");
	}

	printf(">\n");
}

// Prints an Obj declarator node (AST_NODE_OBJ_VAR / AST_NODE_OBJ_FUNC), covering plain variables,
// function pointers (FuncPointerLevel > 0) and functions, then recurses into its
// body (function) or initializer expression (variable / function pointer).
static void PrintObjNode(struct AST_Node* Node, ui32 Depth)
{
	ui8 IsFunc = Node->Type == AST_NODE_OBJ_FUNC;
	ui8 IsFuncPointer = Node->Obj.FuncPointerLevel > 0;

	printf("<%s: ", IsFunc ? "FUNCTION" : (IsFuncPointer ? "FUNC_POINTER" : "VARIABLE"));
	PrintObjDeclarator(&Node->Obj, IsFunc || IsFuncPointer);

	if (IsFunc)
		PrintLabeledNode("BODY", Node->Obj.Func_Block, Depth + 1);
	else
		PrintLabeledNode("INIT", Node->Obj.Var_InitExpression, Depth + 1);
}

// Prints an Expression node's specific data and, for operator / function call expressions, recurses into its sub-expressions.
static void PrintExpressionNode(struct AST_Node* Node, ui32 Depth)
{
	switch (Node->Expression.Type)
	{
	case EXP_LITERAL_INT:
		printf("<LITERAL_INT: %lld>\n", Node->Expression.Literal.Integer);
		break;
	case EXP_LITERAL_FLOAT:
		printf("<LITERAL_FLOAT: %f>\n", Node->Expression.Literal.Float);
		break;
	case EXP_LITERAL_DOUBLE:
		printf("<LITERAL_DOUBLE: %lf>\n", Node->Expression.Literal.Double);
		break;
	case EXP_LITERAL_STRING:
		printf("<LITERAL_STRING: \"%s\">\n", Node->Expression.Literal.String.Str);
		break;
	case EXP_LITERAL_CHAR:
		printf("<LITERAL_CHAR: '%c'>\n", Node->Expression.Literal.Character);
		break;
	case EXP_VAR_ACCESS:
		printf("<VAR_ACCESS: '%s' : ", Node->Expression.Variable.Name.Str);
		PrintDatatypeName(&Node->Expression.ResultType);
		printf(">\n");
		break;
	case EXP_OP:
		printf("<OP: '%s'>\n", Symbol_ToString(Node->Expression.Op.OperatorSymbol));
		PrintNode(Node->Expression.Op.LeftOperand, Depth + 1);
		PrintNode(Node->Expression.Op.RightOperand, Depth + 1);
		break;
	case EXP_FUNC_CALL:
		printf("<FUNCTION_CALL: '%s' : ", Node->Expression.FunctionCall.FunctionName.Str);
		PrintDatatypeName(&Node->Expression.ResultType);
		printf(">\n");
		for (int i = 0; i < Node->Expression.FunctionCall.Params.Size; i++)
			PrintNode(Vector_GetValueAt(Node->Expression.FunctionCall.Params, struct AST_Node*, i), Depth + 1);
		break;
	case EXP_OP_SIZEOF:
		if (Node->Expression.Sizeof.IsDeclarator)
		{
			printf("<SIZE_OF: ");
			PrintObj_AsParam(&Node->Expression.Sizeof.Operand->Obj);
			printf(">\n");
		}
		else
		{
			printf("<SIZE_OF>");
			PrintNode(Node->Expression.Sizeof.Operand, Depth + 1);
		}
		break;
	case EXP_OP_CAST:
		printf("<CAST: ");
		PrintObj_AsParam(&Node->Expression.Cast.TargetTypeDeclarator->Obj);
		printf(">\n");
		PrintNode(Node->Expression.Cast.Operand, Depth + 1);
	}
}

// Prints a single AST node and recurses into its children, indenting each depth level by two spaces.
static void PrintNode(struct AST_Node* Node, ui32 Depth)
{
	if (Node == NULL) return;

	PrintIndent(Depth);

	switch (Node->Type)
	{
	case AST_NODE_STRUCT:
		printf("<STRUCT: '%s'>\n", Node->Struct.Type.TypeName.Str);
		for (int i = 0; i < Node->Struct.Members.Size; i++)
			PrintNode(Vector_GetValueAt(Node->Struct.Members, struct AST_Node*, i), Depth + 1);
		break;
	case AST_NODE_TYPEDEF:
		printf("<TYPEDEF: ");
		PrintObjDeclarator(&Node->Typedef.Declarator, Node->Typedef.Declarator.FuncPointerLevel > 0);
		break;
	case AST_NODE_ENUM:
		printf("<ENUM>\n");
		break;
	case AST_NODE_OBJ_VAR:
	case AST_NODE_OBJ_FUNC:
		PrintObjNode(Node, Depth);
		break;
	case AST_NODE_EXPRESSION:
		PrintExpressionNode(Node, Depth);
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
	case AST_NODE_STATEMENT_VAR_DEC:
		printf("<VAR_DECLARATION>\n");
		for (int i = 0; i < Node->Statement.VarDeclaration.Declarators.Size; i++)
			PrintNode(Vector_GetValueAt(Node->Statement.VarDeclaration.Declarators, struct AST_Node*, i), Depth + 1);
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

