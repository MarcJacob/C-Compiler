// Implementation file for logging Integrated Program Trees.

#include "integrator.h"

void PrintStructSymbol(struct ProgramSymbol* StructSymbol, ui32 Depth)
{
	for (ui32 IndentIndex = 0; IndentIndex < Depth; IndentIndex++) printf("\t");
	StructSymbol->Struct.IsUnion ? printf("UNION ") : printf("STRUCT ");
	printf("'%s', Size = %lld bytes, Align = %d bytes\n", StructSymbol->Name.Str, StructSymbol->Struct.Size, StructSymbol->Struct.Alignment);

	if (StructSymbol->Struct.Size > 0)
	for (int MemberSymbolIndex = 0; MemberSymbolIndex < StructSymbol->Struct.Scope->Symbols.Size; MemberSymbolIndex++)
	{
		struct ProgramSymbol* MemberSymbol = Vector_GetValueAt(StructSymbol->Struct.Scope->Symbols, struct ProgramSymbol*, MemberSymbolIndex);
		ASSERT(MemberSymbol != NULL);
		if (MemberSymbol->Type != SYMBOL_TYPE_VARIABLE) continue;

		for (ui32 IndentIndex = 0; IndentIndex < Depth + 1; IndentIndex++) printf("\t");
		printf("VAR '%s' : ", MemberSymbol->Name.Str);
		PrintTypeSignature(MemberSymbol->Variable.DeclarationType);
		for (int i = 0; i < MemberSymbol->Variable.ArraySizes.Size; i++)
		{
			printf("[%lld]", Vector_GetValueAt(MemberSymbol->Variable.ArraySizes, i64, i));
		}
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

		for (ui32 IndentIndex = 0; IndentIndex < Depth; IndentIndex++) printf("\t");
		printf(SymbolIndex < ParamCount ? "PARAM '%s' : " : "LOCAL VAR '%s' : ", Symbol->Name.Str);
		PrintTypeSignature(Symbol->Variable.DeclarationType);
		for (int i = 0; i < Symbol->Variable.ArraySizes.Size; i++)
		{
			printf("[%lld]", Vector_GetValueAt(Symbol->Variable.ArraySizes, i64, i));
		}
		printf("\n");
	}

	if (Scope->ChildScopes.Size > 0)
	{
		printf("\n");
		for (ui32 IndentIndex = 0; IndentIndex < Depth; IndentIndex++) printf("\t");
		printf("---------\n\n");
	}
	for (int ChildIndex = 0; ChildIndex < Scope->ChildScopes.Size; ChildIndex++)
	{
		struct SymbolScope* ChildScope = Vector_GetValueAt(Scope->ChildScopes, struct SymbolScope*, ChildIndex);
		ASSERT(ChildScope != NULL);

		for (ui32 IndentIndex = 0; IndentIndex < Depth; IndentIndex++) printf("\t");
		printf("SUB SCOPE {\n");

		PrintFunctionScope(ChildScope, Depth + 1, 0);

		for (ui32 IndentIndex = 0; IndentIndex < Depth; IndentIndex++) printf("\t");
		printf("}\n");
	}
}

void PrintFunctionSymbol(struct ProgramSymbol* FuncSymbol, ui32 Depth)
{
	for (ui32 IndentIndex = 0; IndentIndex < Depth; IndentIndex++) printf("\t");

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
	}
}

void PrintSymbol(struct ProgramSymbol* Symbol, ui32 Depth)
{
	ASSERT(Symbol != NULL);

	switch (Symbol->Type)
	{
	case SYMBOL_TYPE_VARIABLE:
		for (ui32 IndentIndex = 0; IndentIndex < Depth; IndentIndex++) printf("\t");
		printf("VAR '%s' : ", Symbol->Name.Str);
		PrintTypeSignature(Symbol->Variable.DeclarationType);
		for (int i = 0; i < Symbol->Variable.ArraySizes.Size; i++)
		{
			printf("[%lld]", Vector_GetValueAt(Symbol->Variable.ArraySizes, i64, i));
		}
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
		for (ui32 IndentIndex = 0; IndentIndex < Depth; IndentIndex++) printf("\t");
		printf("ENUM '%s', Type Size = %lld\n", Symbol->Name.Str, Symbol->Enum.UnderlyingTypeSize);
		break;
	case SYMBOL_TYPE_ENUM_VAL:
		// It's a little hacky but ENUM VAL symbols should always immediately follow their parent ENUM, so the extra indent level will make that look better.
		for (ui32 IndentIndex = 0; IndentIndex < Depth + 1; IndentIndex++) printf("\t");
		printf("ENUM VAL '%s' = %lld\n", Symbol->Name.Str, Symbol->Enum_Member.NumericValue);
		break;
	case SYMBOL_TYPE_TYPEDEF:
		for (ui32 IndentIndex = 0; IndentIndex < Depth; IndentIndex++) printf("\t");
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
