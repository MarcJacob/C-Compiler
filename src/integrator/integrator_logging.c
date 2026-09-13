// Implementation file for logging Integrated Program Trees.

#include "integrator.h"

void PrintStructSymbol(struct ProgramSymbol* StructSymbol, ui32 Depth)
{
	PrintIndent(Depth);
	StructSymbol->Struct.IsUnion ? printf("UNION ") : printf("STRUCT ");
	printf("'%s', Size = %lld bytes, Align = %d bytes\n", StructSymbol->Name.Str, StructSymbol->Struct.Size, StructSymbol->Struct.Alignment);

	if (StructSymbol->Struct.Size > 0)
	for (int MemberSymbolIndex = 0; MemberSymbolIndex < StructSymbol->Struct.Scope->Symbols.Size; MemberSymbolIndex++)
	{
		struct ProgramSymbol* MemberSymbol = Vector_GetValueAt(StructSymbol->Struct.Scope->Symbols, struct ProgramSymbol*, MemberSymbolIndex);
		ASSERT(MemberSymbol != NULL);
		if (MemberSymbol->Type != SYMBOL_TYPE_VARIABLE) continue;

		PrintIndent(Depth + 1);
		printf("VAR '%s' : ", MemberSymbol->Name.Str);
		PrintTypeSignature(MemberSymbol->Variable.DeclarationType);
		if (MemberSymbol->Variable.BitSize % 8 == 0)
		{
			printf(", Size = %lld bytes, Offset = %lld\n", MemberSymbol->Variable.BitSize / 8, MemberSymbol->Variable.Offset);
		}
		else
		{
			printf(", Size = %lld bits, Offset = %lld (+ %d bits)\n", MemberSymbol->Variable.BitSize, MemberSymbol->Variable.Offset, MemberSymbol->Variable.BitOffset);
		}
	}
}


// Recursively prints a function scope's symbols (parameters, locals, and any locally-declared struct / union / enum / typedef),
// then recurses into its child scopes (nested statement blocks), one indentation level deeper each time.
// ParamCount marks how many of THIS scope's own symbols (from the start) are function parameters rather than locals -
// only meaningful for the function's top scope, pass 0 for any nested (child) scope.
void PrintFunctionScope(struct SymbolScope* Scope, ui32 Depth, ui32 ParamCount)
{
	ASSERT(Scope != NULL);

	for (int SymbolIndex = 0; SymbolIndex < Scope->Symbols.Size; SymbolIndex++)
	{
		struct ProgramSymbol* Symbol = Vector_GetValueAt(Scope->Symbols, struct ProgramSymbol*, SymbolIndex);
		ASSERT(Symbol != NULL);

		if (Symbol->Type != SYMBOL_TYPE_VARIABLE)
		{
			PrintSymbol(Symbol, Depth);
			continue;
		}

		PrintIndent(Depth);
		printf(SymbolIndex < ParamCount ? "PARAM '%s' : " : "LOCAL VAR '%s' : ", Symbol->Name.Str);
		PrintTypeSignature(Symbol->Variable.DeclarationType);
		printf("\n");
	}

	if (Scope->ChildScopes.Size > 0)
	{
		printf("\n");
		PrintIndent(Depth);
		printf("---------\n\n");
	}
	for (int ChildIndex = 0; ChildIndex < Scope->ChildScopes.Size; ChildIndex++)
	{
		struct SymbolScope* ChildScope = Vector_GetValueAt(Scope->ChildScopes, struct SymbolScope*, ChildIndex);
		ASSERT(ChildScope != NULL);

		PrintIndent(Depth);
		printf("SUB SCOPE {\n");

		PrintFunctionScope(ChildScope, Depth + 1, 0);

		PrintIndent(Depth);
		printf("}\n");
	}
}

// Prints an integrated Expression node's specific data and, for operator / function call expressions, recurses into its sub-expressions.
// Mirrors PrintParsedExpression (parser_logging.c), except VAR_ACCESS / FUNCTION_CALL expressions print their resolved symbol's name
// instead of the parsed name, since both fields share the same union slot and the parsed name is no longer valid once Integration has run.
void PrintIntegratedExpression(struct Expression* Expression, ui32 Depth)
{
	if (Expression == NULL) return;

	PrintIndent(Depth);

	switch (Expression->Type)
	{
	case EXP_LITERAL_INT:
		printf("<LITERAL_INT: %lld : ", Expression->Literal.Integer);
		PrintTypeSignature(Expression->ResultType);
		printf(">\n");
		break;
	case EXP_LITERAL_FLOAT:
		printf("<LITERAL_FLOAT: %f : ", Expression->Literal.Float);
		PrintTypeSignature(Expression->ResultType);
		printf(">\n");
		break;
	case EXP_LITERAL_DOUBLE:
		printf("<LITERAL_DOUBLE: %lf : ", Expression->Literal.Double);
		PrintTypeSignature(Expression->ResultType);
		printf(">\n");
		break;
	case EXP_LITERAL_STRING:
		printf("<LITERAL_STRING: \"");
		PrintEscapedString(Expression->Literal.String.Str);
		printf("\" : ");
		PrintTypeSignature(Expression->ResultType);
		printf(">\n");
		break;
	case EXP_LITERAL_CHAR:
		printf("<LITERAL_CHAR: '");
		PrintEscapedChar(Expression->Literal.Character);
		printf("' : ");
		PrintTypeSignature(Expression->ResultType);
		printf(">\n");
		break;
	case EXP_VAR_ACCESS:
		printf("<VAR_ACCESS: '%s' : ", Expression->Variable.Symbol->Name.Str);
		PrintTypeSignature(Expression->ResultType);
		printf(">\n");
		break;
	case EXP_OP:
		printf("<OP: '%s' : ", Symbol_ToString(Expression->Op.OperatorSymbol));
		PrintTypeSignature(Expression->ResultType);
		printf(">\n");
		PrintIntegratedExpression(Expression->Op.LeftOperand, Depth + 1);
		PrintIntegratedExpression(Expression->Op.RightOperand, Depth + 1);
		break;
	case EXP_FUNC_CALL:
		printf("<FUNCTION_CALL: '%s' : ", Expression->FunctionCall.Symbol->Name.Str);
		PrintTypeSignature(Expression->ResultType);
		printf(">\n");
		for (int i = 0; i < Expression->FunctionCall.Params.Size; i++)
			PrintIntegratedExpression(Vector_GetValueAt(Expression->FunctionCall.Params, struct Expression*, i), Depth + 1);
		break;
	case EXP_OP_SIZEOF:
		printf("<SIZE_OF : ");
		PrintTypeSignature(Expression->ResultType);
		printf(">\n");
		PrintIntegratedExpression(Expression->Sizeof.Operand, Depth + 1);
		break;
	case EXP_OP_CAST:
		printf("<CAST: ");
		PrintTypeSignature(Expression->ResultType);
		printf(">\n");
		PrintIntegratedExpression(Expression->Cast.Operand, Depth + 1);
		break;
	case EXP_NOP:
		PrintTypeSignature(Expression->ResultType);
		printf("\n");
	}
}

// Returns the index of a target instruction within a function's Instructions vector, or -1 if the target is NULL (fallthrough / not applicable).
int FindInstructionIndex(struct ProgramSymbol* FuncSymbol, struct ProgramInstruction* TargetInstruction)
{
	if (TargetInstruction == NULL) return -1;

	for (int InstructionIndex = 0; InstructionIndex < FuncSymbol->Function.Instructions.Size; InstructionIndex++)
	{
		if (Vector_GetPtrAt(FuncSymbol->Function.Instructions, struct ProgramInstruction, InstructionIndex) == TargetInstruction) return InstructionIndex;
	}
	return -1;
}

// Prints all instructions integrated for a function, in source order, indexed so jump instructions can reference their targets by number.
void PrintFunctionInstructions(struct ProgramSymbol* FuncSymbol, ui32 Depth)
{
	ASSERT(FuncSymbol != NULL);

	for (int InstructionIndex = 0; InstructionIndex < FuncSymbol->Function.Instructions.Size; InstructionIndex++)
	{
		struct ProgramInstruction* Instruction = Vector_GetPtrAt(FuncSymbol->Function.Instructions, struct ProgramInstruction, InstructionIndex);
		ASSERT(Instruction != NULL);

		PrintIndent(Depth);
		printf("[%d] ", InstructionIndex);

		switch (Instruction->Type)
		{
		case INSTRUCTION_TYPE_EXPRESSION:
			printf("EXPR:\n");
			PrintIntegratedExpression(Instruction->Exp, Depth + 1);
			break;
		case INSTRUCTION_TYPE_RETURN:
			if (Instruction->Exp == NULL || Instruction->Exp->Type == EXP_NOP)
			{
				printf("RETURN\n");
			}
			else
			{
				printf("RETURN:\n");
				PrintIntegratedExpression(Instruction->Exp, Depth + 1);
			}
			break;
		case INSTRUCTION_TYPE_JUMP:
		{
			// Assumes the owning function's instruction indices have already been resolved into pointers (see union InstructionRef).
			int TargetIndex = FindInstructionIndex(FuncSymbol, Instruction->Jump.JumpTarget.Ptr);
			if (TargetIndex >= 0) printf("JUMP -> [%d]\n", TargetIndex);
			else printf("JUMP -> <end>\n");
			break;
		}
		case INSTRUCTION_TYPE_COND_JUMP:
		{
			// Assumes the owning function's instruction indices have already been resolved into pointers (see union InstructionRef).
			int NonZeroIndex = FindInstructionIndex(FuncSymbol, Instruction->ConditionalJump.IfNonZero.Ptr);
			int ZeroIndex = FindInstructionIndex(FuncSymbol, Instruction->ConditionalJump.IfZero.Ptr);

			printf("IF NON-ZERO -> ");
			if (NonZeroIndex >= 0) printf("[%d]", NonZeroIndex); else printf("<end>");
			printf(" ELSE -> ");
			if (ZeroIndex >= 0) printf("[%d]", ZeroIndex); else printf("<end>");
			printf(":\n");

			PrintIntegratedExpression(Instruction->Exp, Depth + 1);
			break;
		}
		default:
			printf("?\n");
			break;
		}
	}
}

void PrintFunctionSymbol(struct ProgramSymbol* FuncSymbol, ui32 Depth)
{
	PrintIndent(Depth);

	if (FuncSymbol->Function.Scope == NULL)
	{
		// Declaration: no scope to print, so parameters are printed as bare type signatures without names.
		printf("FUNC DEC '%s' : ", FuncSymbol->Name.Str);
		PrintTypeSignature(FuncSymbol->Function.ReturnType);

		printf("(");
		for (int ParamIndex = 0; ParamIndex < FuncSymbol->Function.ParamTypeSignatures.Size; ParamIndex++)
		{
			if (ParamIndex > 0) printf(", ");

			struct TypeSignature* ParamTypeSig = Vector_GetValueAt(FuncSymbol->Function.ParamTypeSignatures, struct TypeSignature*, ParamIndex);
			PrintTypeSignature(ParamTypeSig);
		}
		printf(")\n");
	}
	else
	{
		// Definition: same header, then the named parameters and local variables from the function's scope.
		printf("FUNC DEF '%s' : ", FuncSymbol->Name.Str);
		PrintTypeSignature(FuncSymbol->Function.ReturnType);

		printf("(");
		for (int ParamIndex = 0; ParamIndex < FuncSymbol->Function.ParamTypeSignatures.Size; ParamIndex++)
		{
			if (ParamIndex > 0) printf(", ");

			struct TypeSignature* ParamTypeSig = Vector_GetValueAt(FuncSymbol->Function.ParamTypeSignatures, struct TypeSignature*, ParamIndex);
			PrintTypeSignature(ParamTypeSig);
		}
		printf(")\n");

		PrintFunctionScope(FuncSymbol->Function.Scope, Depth + 1, FuncSymbol->Function.ParamTypeSignatures.Size);

		printf("\n");
		PrintIndent(Depth + 1);
		printf("---------\n\n");

		PrintIndent(Depth + 1);
		printf("INSTRUCTIONS:\n");
		PrintFunctionInstructions(FuncSymbol, Depth + 2);
		printf("\n");
	}
}

void PrintSymbol(struct ProgramSymbol* Symbol, ui32 Depth)
{
	ASSERT(Symbol != NULL);

	switch (Symbol->Type)
	{
	case SYMBOL_TYPE_VARIABLE:
		PrintIndent(Depth);
		printf("VAR '%s' : ", Symbol->Name.Str);
		PrintTypeSignature(Symbol->Variable.DeclarationType);
		printf(", Size = %lld bytes\n", Symbol->Variable.BitSize / 8, Symbol->Variable.Offset);
		break;
	case SYMBOL_TYPE_STRUCT:
	case SYMBOL_TYPE_UNION:
		PrintStructSymbol(Symbol, Depth);
		break;
	case SYMBOL_TYPE_FUNCTION:
		PrintFunctionSymbol(Symbol, Depth);
		break;
	case SYMBOL_TYPE_ENUM:
		PrintIndent(Depth);
		printf("ENUM '%s', Type Size = %lld\n", Symbol->Name.Str, Symbol->Enum.UnderlyingTypeSize);
		break;
	case SYMBOL_TYPE_ENUM_VAL:
		// It's a little hacky but ENUM VAL symbols should always immediately follow their parent ENUM, so the extra indent level will make that look better.
		PrintIndent(Depth + 1);
		printf("ENUM VAL '%s' = %lld\n", Symbol->Name.Str, Symbol->Enum_Member.NumericValue);
		break;
	case SYMBOL_TYPE_TYPEDEF:
		PrintIndent(Depth);
		printf("TYPEDEF '%s' : ", Symbol->Name.Str);
		PrintTypeSignature(Symbol->Typedef.Type);
		printf("\n");
		break;
	default:
		break;
	}
}

void Integrator_PrintTree(struct IntegratorProcess* Integrator)
{
	ASSERT(Integrator != NULL);
	ASSERT(Integrator->ProgramTree != NULL);

	printf("\n===== INTEGRATOR OUTPUT =====\n\n");

	printf("Total Top-Level Symbols: %lld\n", Integrator->ProgramTree->RootScope->Symbols.Size);

	printf("\n== GLOBAL SCOPE SYMBOLS ==\n");

	// Print root scope symbols.
	for (int GlobalSymbolIndex = 0; GlobalSymbolIndex < Integrator->ProgramTree->RootScope->Symbols.Size; GlobalSymbolIndex++)
	{
		struct ProgramSymbol* GlobalSymbol = Vector_GetValueAt(Integrator->ProgramTree->RootScope->Symbols, struct ProgramSymbol*, GlobalSymbolIndex);
		PrintSymbol(GlobalSymbol, 0);
	}
}
