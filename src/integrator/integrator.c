#include "integrator.h"
#include <stdarg.h>

// Main implementation file for the Integrator stage.

void Integrator_Error(struct IntegratorProcess* Integrator, ui32 BufferLoc, const char* Format, ...)
{
	if (Integrator->HasError) return; // Most specific error only.

	Integrator->HasError = 1;
	Integrator->Error.Location = BufferLoc;

	va_list args;
	va_start(args, Format);
	Integrator->Error.Message = String_CreateFormatV_ANSI(Format, args);
	va_end(args);
}

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

void PrintSymbol(struct ProgramSymbol* Symbol, ui32 Depth);

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

struct SymbolScope* AllocScope(struct SymbolScope* Parent);
void FreeScope(struct SymbolScope* Scope);

struct ProgramSymbol* AllocSymbol(enum SYMBOL_TYPE Type)
{
	struct ProgramSymbol* NewSymbol = calloc(1, sizeof(struct ProgramSymbol));
	ASSERT(NewSymbol != NULL);
	NewSymbol->Type = Type;

	switch (Type)
	{
	case SYMBOL_TYPE_VARIABLE:
		NewSymbol->Variable.ArraySizes = Vector_Create(ui64, 0);
		break;
	case SYMBOL_TYPE_ENUM:
		NewSymbol->Enum.Values = Vector_Create(struct ProgramSymbol*, 2);
		break;
	default:
		break;
	}

	return NewSymbol;
}

void FreeSymbol(struct ProgramSymbol* Symbol)
{
	if (Symbol == NULL) return;

	switch (Symbol->Type)
	{
	case SYMBOL_TYPE_VARIABLE:
		Vector_Destroy(&Symbol->Variable.ArraySizes);
		break;
	case SYMBOL_TYPE_FUNCTION:
		FreeScope(Symbol->Function.Scope);
		break;
	case SYMBOL_TYPE_STRUCT:
		FreeScope(Symbol->Struct.Scope);
		break;
	case SYMBOL_TYPE_ENUM:
		Vector_Destroy(&Symbol->Enum.Values);
		break;
	default:
		break;
	}
}

struct SymbolScope* AllocScope(struct SymbolScope* Parent)
{
	struct SymbolScope* NewScope = calloc(1, sizeof(struct SymbolScope));
	ASSERT(NewScope != NULL);
	NewScope->Symbols = Vector_Create(struct ProgramSymbol*, 1);
	NewScope->ChildScopes = Vector_Create(struct SymbolScope*, 0);
	NewScope->Parent = Parent;

	if (Parent != NULL)
	{
		Vector_Push(Parent->ChildScopes, struct SymbolScope*, NewScope);
	}

	return NewScope;
}

void FreeScope(struct SymbolScope* Scope)
{
	if (Scope == NULL) return;

	for (int ChildIndex = 0; ChildIndex < Scope->ChildScopes.Size; ChildIndex++)
	{
		FreeScope(Vector_GetValueAt(Scope->ChildScopes, struct SymbolScope*, ChildIndex));
	}
	Vector_Destroy(&Scope->ChildScopes);

	for (int SymbolIndex = 0; SymbolIndex < Scope->Symbols.Size; SymbolIndex++)
	{
		FreeSymbol(Vector_GetValueAt(Scope->Symbols, struct ProgramSymbol*, SymbolIndex));
	}
	Vector_Destroy(&Scope->Symbols);
}

void Scope_AddSymbol(struct SymbolScope* Scope, struct ProgramSymbol* Symbol)
{
	ASSERT(Scope != NULL);
	ASSERT(Symbol != NULL);

	Vector_Push(Scope->Symbols, struct ProgramSymbol*, Symbol);
}

// Returns the "earliest found" symbol starting from the provided scope.
// Returns NULL if no symbols were found.
struct ProgramSymbol* Scope_FindSymbol(const struct SymbolScope* Scope, const struct String_ANSI* Name, ui8 SearchParent)
{
	ASSERT(Scope != NULL);
	ASSERT(Name != NULL);

	for (int SymbolIndex = 0; SymbolIndex < Scope->Symbols.Size; SymbolIndex++)
	{
		struct ProgramSymbol* Symbol = Vector_GetValueAt(Scope->Symbols, struct ProgramSymbol*, SymbolIndex);
		ASSERT(Symbol != NULL);
		ASSERT(Symbol->Name.Str != NULL);
		ASSERT(Name->Str != NULL);

		if (strcmp(Symbol->Name.Str, Name->Str) == 0) return Symbol;
	}

	return (Scope->Parent != NULL && SearchParent) ? Scope_FindSymbol(Scope->Parent, Name, 1) : NULL;
}

ui8 EvalConstantExpression(struct IntegratorProcess* Integrator, struct SymbolScope* Scope, struct Expression* Expression, i64* OutResult, enum DATATYPE* OutResultType);

// Resolves an operation between two expressions, recursively.
ui8 EvalConstantOpExpression(struct IntegratorProcess* Integrator, struct SymbolScope* Scope, enum TOKEN_SYMBOL Op, struct Expression* LeftOperand, struct Expression* RightOperand,
	ui64* OutResult, enum DATATYPE* OutResultType)
{
	ASSERT(OutResult != NULL);
	ASSERT(
		   Symbol_IsLeftUnaryOp(Op) && RightOperand != NULL
		|| Symbol_IsRightUnaryOp(Op) && LeftOperand != NULL
		|| Symbol_IsBinaryOp(Op) && LeftOperand != NULL && RightOperand != NULL
	);

	ui32 BufferLoc = LeftOperand != NULL ? LeftOperand->BufferLocation : RightOperand->BufferLocation;

	i64 LeftOpRes = 0, RightOpRes = 0;
	enum DATATYPE LeftOpResType, RightOpResType;
	if (LeftOperand != NULL
		&& !EvalConstantExpression(Integrator, Scope, LeftOperand, &LeftOpRes, &LeftOpResType)) return 0;
	if (RightOperand != NULL
		&& !EvalConstantExpression(Integrator, Scope, RightOperand, &RightOpRes, &RightOpResType)) return 0;

	// TODO: Check compatibility between result types & perform appropriate casts.
	// For now we just error out if we ever get anything other than an INT64.
	if (LeftOpRes != DATATYPE_INT64 || RightOpRes != DATATYPE_INT64)
	{
		Integrator_Error(Integrator, BufferLoc, "Unimplemented constant expression resolution for other types than uint64.");
		return 0;
	}

	*OutResultType = DATATYPE_INT64; // TODO: Later on this is where we would perform compatibility test and determine which type is returned.

	switch (Op)
	{
	case SYMBOL_OP_ADD:
		*OutResult = LeftOpRes + RightOpRes;
		return 1;
	case SYMBOL_OP_SUB:
		*OutResult = LeftOpRes - RightOpRes;
		return 1;
	case SYMBOL_OP_MULT:
		*OutResult = LeftOpRes * RightOpRes;
		return 1;
	case SYMBOL_OP_DIV:
		if (RightOpRes == 0)
		{
			Integrator_Error(Integrator, RightOperand->BufferLocation, "Division by 0.");
			return 0;
		}
		*OutResult = LeftOpRes / RightOpRes;
		return 1;
	case SYMBOL_OP_MOD:
		if (RightOpRes == 0) 
		{
			Integrator_Error(Integrator, RightOperand->BufferLocation, "Division by 0.");
			return 0;
		}
		*OutResult = LeftOpRes % RightOpRes;
		return 1;
	case SYMBOL_OP_EQUAL:
		*OutResult = LeftOpRes == RightOpRes;
		return 1;
	case SYMBOL_OP_UNEQUAL:
		*OutResult = LeftOpRes != RightOpRes;
		return 1;
	case SYMBOL_OP_LOWER:
		*OutResult = LeftOpRes < RightOpRes;
		return 1;
	case SYMBOL_OP_LOWER_EQUAL:
		*OutResult = LeftOpRes <= RightOpRes;
		return 1;
	case SYMBOL_OP_GREATER:
		*OutResult = LeftOpRes > RightOpRes;
		return 1;
	case SYMBOL_OP_GREATER_EQUAL:
		*OutResult = LeftOpRes >= RightOpRes;
		return 1;
	case SYMBOL_OP_LEFT_SHIFT:
		*OutResult = LeftOpRes << RightOpRes;
		return 1;
	case SYMBOL_OP_RIGHT_SHIFT:
		*OutResult = LeftOpRes >> RightOpRes;
		return 1;
	case SYMBOL_OP_BITWISE_AND:
		*OutResult = LeftOpRes & RightOpRes;
		return 1;
	case SYMBOL_OP_BITWISE_OR:
		*OutResult = LeftOpRes | RightOpRes;
		return 1;
	case SYMBOL_OP_BITWISE_XOR:
		*OutResult = LeftOpRes ^ RightOpRes;
		return 1;
	case SYMBOL_OP_BITWISE_REVERSE:
		*OutResult = ~RightOpRes;
		return 1;
	default:
		Integrator_Error(Integrator, BufferLoc, "Operator not supported in constant expression.");
		return 0;
	}
}

// Resolves a constant expression and returns its result within the 8 bytes pointer provided.
// Returns 1 if successful, returns 0 if there was an error.
ui8 EvalConstantExpression(struct IntegratorProcess* Integrator, struct SymbolScope* Scope, struct Expression* Expression, i64* OutResult, enum DATATYPE* OutResultType)
{
	ASSERT(Scope != NULL);
	ASSERT(Expression != NULL);
	ASSERT(OutResult != NULL);

	switch (Expression->Type)
	{
		// Valid base cases
	case EXP_LITERAL_CHAR:
		*OutResult = Expression->Literal.Character;
		*OutResultType = DATATYPE_CHAR;
		return 1;
	case EXP_LITERAL_INT:
		*OutResult = Expression->Literal.Integer;
		*OutResultType = DATATYPE_INT64;
		return 1;
	case EXP_VAR_ACCESS:
	{
		struct ProgramSymbol* Symbol = Scope_FindSymbol(Scope, &Expression->Variable.Name, 1);
		if (Symbol == NULL)
		{
			Integrator_Error(Integrator, Expression->BufferLocation, "Undeclared identifier '%s' in constant expression.", Expression->Variable.Name.Str);
			return 0;
		}
		if (Symbol->Type != SYMBOL_TYPE_ENUM_VAL)
		{
			Integrator_Error(Integrator, Expression->BufferLocation, "'%s' is not usable in a constant expression.", Expression->Variable.Name.Str);
			return 0;
		}

		*OutResult = Symbol->Enum_Member.NumericValue;
		*OutResultType = DATATYPE_INT64;
		return 1;
	}
		// Invalid base cases
	case EXP_LITERAL_STRING:
	case EXP_NOP:
	case EXP_FUNC_CALL:
	default:
		Integrator_Error(Integrator, Expression->BufferLocation, "Expression must be constant integral.");
		return 0;
		// Complex cases
	case EXP_OP:
		return EvalConstantOpExpression(Integrator, Scope, Expression->Op.OperatorSymbol, Expression->Op.LeftOperand, Expression->Op.RightOperand, OutResult, OutResultType);
	}
}

// Returns the resolved size of a passed in type signature.
// Returns 0 if trying to use an incomplete type, non-pointer signature.
// 
// If the type is primitive or a pointer then this is trivial and just returns the type's size.
// If not, then the whole program tree as it currently exists will be searched to find a matching symbol.
// In that case if a symbol is successfully found, it is returned through the OutTypeSymbol parameter.
// If not, a new symbol declaration is created and added to the Program Tree's root scope.
// Note: This only happens for pointer type signatures. Non-pointer type signatures that use an undeclared type will trigger an error.
ui64 IntegrateTypeSignature(struct IntegratorProcess* Integrator, struct SymbolScope* Scope, struct TypeSignature* TypeSig, struct ProgramSymbol** OutTypeSymbol)
{
	ASSERT(TypeSig != NULL);
	ASSERT(OutTypeSymbol != NULL);

	ui64 TypeSize = 0;
	if (TypeSig->IsFunctionPointer || TypeSig->PointerLevel)
	{
		TypeSize = POINTER_SIZE;
		if (TypeSig->Type != DATATYPE_USER_DEFINED)
		{
			// Type is pointer to primitive.
			return TypeSize;
		}
	}
	else if (TypeSig->Type != DATATYPE_USER_DEFINED)
	{
		// Type is non-pointer primitive.
		TypeSize = TypeSig->Size;
		return TypeSize;
	}

	// From here we're dealing with a non-primitive type.
	// If it's a pointer, we try to find a matching symbol but may create one from scratch, effectively
	// making this the declaration site for it.
	// If not, we MUST find a matching DECLARED / RESOLVED type symbol (Struct / Union, Typedef or Enum).

	struct ProgramSymbol* TypeSymbol = Scope_FindSymbol(Scope, &TypeSig->TypeName, 1);
	*OutTypeSymbol = TypeSymbol;

	// If Type Symbol is found, check compatibility.
	if (TypeSymbol != NULL)
	{
		if ((TypeSig->Flags & TYPE_IS_STRUCTURED && TypeSig->Flags & TYPE_IS_ENUM_OR_UNION == 0 && TypeSymbol->Type != SYMBOL_TYPE_STRUCT)
			|| (TypeSig->Flags & TYPE_IS_STRUCTURED && TypeSig->Flags & TYPE_IS_ENUM_OR_UNION && !TypeSymbol->Struct.IsUnion)
			|| (TypeSig->Flags & TYPE_IS_STRUCTURED == 0 && TypeSig->Flags & TYPE_IS_ENUM_OR_UNION && TypeSymbol->Type != SYMBOL_TYPE_ENUM))
		{
			return 0; // Incompatible types. The OutTypeSymbol pointer remains populated as an indication of the exact problem.
		}
	}

	// If type sig is a pointer, return pointer size after first creating a declaration symbol for the type if required.
	if (TypeSig->PointerLevel > 0)
	{
		if (TypeSymbol == NULL)
		{
			// We have a pointer to a undeclared type. Create a declaration for it now.

			enum SYMBOL_TYPE SymbolType;
			switch (TypeSig->Flags & (TYPE_IS_STRUCTURED | TYPE_IS_ENUM_OR_UNION))
			{
			case TYPE_IS_STRUCTURED:
				SymbolType = SYMBOL_TYPE_STRUCT;
				break;
			case TYPE_IS_STRUCTURED | TYPE_IS_ENUM_OR_UNION:
				SymbolType = SYMBOL_TYPE_UNION;
				break;
			case TYPE_IS_ENUM_OR_UNION:
				SymbolType = SYMBOL_TYPE_ENUM;
				break;
			}

			TypeSymbol = AllocSymbol(SymbolType);
			TypeSymbol->Name = String_Copy_ANSI(TypeSig->TypeName);

			Scope_AddSymbol(Integrator->ProgramTree->RootScope, TypeSymbol);
		}

		return TypeSize;
	}

	// At this point we know we're dealing with a value. Find out the size of the Type Symbol, if any.

	if (TypeSymbol == NULL) return 0; // Undeclared type.

	if (TypeSymbol->Type == SYMBOL_TYPE_STRUCT
		|| TypeSymbol->Type == SYMBOL_TYPE_UNION)
		TypeSig->Size = TypeSymbol->Struct.Size;
	else if (TypeSymbol->Type == SYMBOL_TYPE_ENUM)
		TypeSig->Size = TypeSymbol->Enum.UnderlyingTypeSize;
	else if (TypeSymbol->Type == SYMBOL_TYPE_TYPEDEF)
	{
		TypeSig->Size = TypeSymbol->Typedef.Type->Size;
	}

	return TypeSig->Size; // Will be 0 if the type exists but is incomplete.
}

struct ProgramSymbol* IntegrateASTObjectNode(struct IntegratorProcess* Integrator, struct AST_Node* RootASTNode, struct SymbolScope* Scope);

// Returns an integrated Variable symbol from a corresponding Variable AST object.
// The variable's type and size is resolved, but its final size (if bit count is specified) and offset must be
// resolved by the caller according to its context, and its parent scope must be assigned.
struct ProgramSymbol* IntegrateObj_Variable(struct IntegratorProcess* Integrator, struct AST_Node* VarASTNode, struct SymbolScope* Scope)
{
	ASSERT(VarASTNode != NULL);
	ASSERT(Scope != NULL);

	struct ProgramSymbol* VarSymbol = Scope_FindSymbol(Scope, &VarASTNode->Obj.Name, 0);
	if (VarSymbol != NULL)
	{
		// Check type coherence and redefinition.
		if (!TypeSignaturesEquivalent(VarSymbol->Variable.DeclarationType, VarASTNode->Obj.TypeSignature))
		{
			Integrator_Error(Integrator, VarASTNode->BufferLocation, "Incoherent types in variable '%s' redeclaration.", VarASTNode->Obj.Name.Str);
			return NULL;
		}
		
		if (VarASTNode->Obj.Var.Initializer.Expression != NULL && VarSymbol->Variable.HasInitializer
			|| VarASTNode->Obj.Var.ArraySizes.Size != VarSymbol->Variable.ArraySizes.Size)
		{
			Integrator_Error(Integrator, VarASTNode->BufferLocation, "Variable '%s' redefinition.", VarASTNode->Obj.Name.Str);
			return NULL;
		}
	}

	struct TypeSignature* TypeSig = NULL;
	ui64 VarBitSize = 0;

	if (VarSymbol == NULL)
	{
		struct ProgramSymbol* TypeSymbol = NULL;
		TypeSig = VarASTNode->Obj.TypeSignature;
		VarBitSize = IntegrateTypeSignature(Integrator, Scope, TypeSig, &TypeSymbol) * 8;

		if (VarBitSize == 0)
		{
			if (TypeSymbol != NULL)
			{
				Integrator_Error(Integrator, VarASTNode->BufferLocation, "Incoherent usage of type '%s'.", TypeSig->TypeName.Str);
				goto INTEGRATE_FAIL;
			}

			Integrator_Error(Integrator, VarASTNode->BufferLocation, "Use of incomplete type '%s'.", TypeSig->TypeName.Str);
			goto INTEGRATE_FAIL;
		}
	}
	else
	{
		TypeSig = VarSymbol->Variable.DeclarationType;
		VarBitSize = VarSymbol->Variable.BitSize;
	}

	// Handle array size(s).
	// Resolve array size expressions. Error out if any of the expressions cannot be resolved at compile-time.
	// If the Var Symbol already exists, also error out if the array sizes differ from original declaration.
	struct Vector ArraySizes = Vector_Create(ui64, 0);
	for (int ArraySizeExpIndex = 0; ArraySizeExpIndex < VarASTNode->Obj.Var.ArraySizes.Size; ArraySizeExpIndex++)
	{
		struct Expression* ArraySizeExp = Vector_GetValueAt(VarASTNode->Obj.Var.ArraySizes, struct Expression*, ArraySizeExpIndex);
		ASSERT(ArraySizeExp != NULL);

		i64 EvalResult;
		enum DATATYPE EvalType;
		if (!EvalConstantExpression(Integrator, Scope, ArraySizeExp, &EvalResult, &EvalType))
		{
			Integrator_Error(Integrator, ArraySizeExp->BufferLocation, "Array size expression must be constant value.");
		INTEGRATE_FAIL:
			FreeSymbol(VarSymbol);
			return NULL;
		}

		if (EvalType == DATATYPE_INT32)
		{
			// The most readable conversion code I've ever typed hands down.
			EvalResult = (i64)(*(i32*)(&EvalResult));
			EvalType = DATATYPE_INT64;
		}

		if (EvalType != DATATYPE_INT64)
		{
			Integrator_Error(Integrator, ArraySizeExp->BufferLocation, "Array size expression must be an integral value.");
			goto INTEGRATE_FAIL;
		}

		// Compare result against original var symbol's corresponding array size.
		if (VarSymbol != NULL && Vector_GetValueAt(VarSymbol->Variable.ArraySizes, i64, ArraySizeExpIndex) != EvalResult)
		{
			Integrator_Error(Integrator, ArraySizeExp->BufferLocation, "Incoherent array subscripts with existing declaration.");
			goto INTEGRATE_FAIL;
		}

		Vector_Push(ArraySizes, i64, EvalResult);
		// Multiply size by each array layer's resolved size.
		VarBitSize *= EvalResult;
	}

	// If Var Symbol doesn't already exist, create it now.
	if (VarSymbol == NULL)
	{
		VarSymbol = AllocSymbol(SYMBOL_TYPE_VARIABLE);
		VarSymbol->Name = String_Copy_ANSI(VarASTNode->Obj.Name);
		VarSymbol->Variable.ArraySizes = ArraySizes;
		VarSymbol->Variable.DeclarationType = TypeSig;
		VarSymbol->Variable.BitSize = VarBitSize;
		
		Scope_AddSymbol(Scope, VarSymbol);
	}

	// Check for initializer.
	if (VarASTNode->Obj.Var.Initializer.Expression != NULL)
	{
		VarSymbol->Variable.HasInitializer = 1;
		if (VarASTNode->Obj.Var.InitIsInitializerList)
		{
			VarSymbol->Variable.InitializerList = Vector_Create(struct Expression*, 0);
			Vector_Append(&VarSymbol->Variable.InitializerList, &VarASTNode->Obj.Var.Initializer.List);
		}
		else
		{
			VarSymbol->Variable.InitExpression = VarASTNode->Obj.Var.Initializer.Expression;
		}
	}

	return VarSymbol;
}

// Integrates all statements inside a block statement AST Node. Adds all found Program Instructions into the function's instructions vector, 
// and all found local variables declaration into the specified block scope.
// Check for error after execution.
void IntegrateStatementBlock(struct IntegratorProcess* Integrator, struct ProgramSymbol* FunctionSymbol, struct SymbolScope* BlockScope, struct AST_Node* StatementBlock)
{
	ASSERT(FunctionSymbol != NULL);
	ASSERT(BlockScope != NULL);
	ASSERT(StatementBlock != NULL && StatementBlock->Type == AST_NODE_STATEMENT_BLOCK);

	for (int StatementIndex = 0; StatementIndex < StatementBlock->Statement.Block.Statements.Size; StatementIndex++)
	{
		struct AST_Node* StatementNode = Vector_GetValueAt(StatementBlock->Statement.Block.Statements, struct AST_Node*, StatementIndex);
		ASSERT(StatementNode != NULL);

		struct SymbolScope* BlockSubScope = NULL;
		switch (StatementNode->Type)
		{
		case AST_NODE_STATEMENT_BLOCK:
			BlockSubScope = AllocScope(BlockScope);
			IntegrateStatementBlock(Integrator, FunctionSymbol, BlockSubScope, StatementNode);
			if (Integrator->HasError) return;
			break;
		case AST_NODE_STATEMENT_OBJ_DEC:
			for (int ObjIndex = 0; ObjIndex < StatementNode->Statement.ObjectDeclaration.Objects.Size; ObjIndex++)
			{
				struct AST_Node* ObjDec = Vector_GetValueAt(StatementNode->Statement.ObjectDeclaration.Objects, struct AST_Node*, ObjIndex);
				ASSERT(ObjDec != NULL);

				struct ProgramSymbol* LocalSymbol = IntegrateASTObjectNode(Integrator, ObjDec, BlockScope);
				if (LocalSymbol == NULL)
				{
					Integrator_Error(Integrator, ObjDec->BufferLocation, "Failed to resolve local symbol.");
					return;
				}

				if (LocalSymbol->Type == SYMBOL_TYPE_FUNCTION
					&& LocalSymbol->Function.Scope != NULL)
				{
					Integrator_Error(Integrator, ObjDec->BufferLocation, "Function definition within another function is disallowed.");
					return;
				}

				if (LocalSymbol->Type == SYMBOL_TYPE_VARIABLE)
				{
					Vector_Push(FunctionSymbol->Function.LocalVariables, struct ProgramSymbol*, LocalSymbol);
				}
			}
			break;
		default:
			// TODO: Add support for other statement types.
			break;
		}
	}
}

// Returns an integrated function symbol from an AST Object node.
// If the node has an accompanying definition, the function is fully parsed along with the instructions.
// Otherwise the symbol will only feature its signature and parameters until a definition is found.
struct ProgramSymbol* IntegrateObj_Function(struct IntegratorProcess* Integrator, struct AST_Node* FuncASTNode, struct SymbolScope* Scope)
{
	ASSERT(FuncASTNode != NULL);

	// Look for existing declaration or create one.
	struct ProgramSymbol* FuncSymbol = Scope_FindSymbol(Scope, &FuncASTNode->Obj.Name, 0);

	if (FuncSymbol != NULL)
	{
		// Check consistency on parameters and whether we're running into a redefinition.

		// Check redefinition.
		if (FuncSymbol->Function.Scope != NULL && FuncASTNode->Obj.Func.StatementsBlock != NULL)
		{
			Integrator_Error(Integrator, FuncASTNode->BufferLocation, "Function '%s' redefinition.", FuncASTNode->Obj.Name.Str);
			return NULL;
		}

		// Compare return types.
		if (!TypeSignaturesEquivalent(FuncSymbol->Function.ReturnType, FuncASTNode->Obj.TypeSignature))
		{
			Integrator_Error(Integrator, FuncASTNode->BufferLocation, "Inconsistent return type for function '%s' redeclaration.", FuncASTNode->Obj.Name.Str);
			return NULL;
		}

		// Compare parameter count.
		if (FuncSymbol->Function.ParamTypeSignatures.Size != FuncASTNode->Obj.Func.Params.Size)
		{
			Integrator_Error(Integrator, FuncASTNode->BufferLocation, "Inconsistent param count for function '%s' redeclaration.", FuncASTNode->Obj.Name.Str);
			return NULL;
		}

		// Compare parameter types.
		for (int ParamIndex = 0; ParamIndex < FuncSymbol->Function.ParamTypeSignatures.Size; ParamIndex++)
		{
			struct TypeSignature* SymbolParamTypeSig = Vector_GetValueAt(FuncSymbol->Function.ParamTypeSignatures, struct TypeSignature*, ParamIndex);
			ASSERT(SymbolParamTypeSig != NULL);
			struct AST_Node* ASTFuncParamNode = Vector_GetValueAt(FuncASTNode->Obj.Func.Params, struct AST_Node*, ParamIndex);
			ASSERT(ASTFuncParamNode != NULL);
			ASSERT(ASTFuncParamNode->Obj.TypeSignature != NULL);
			if (!TypeSignaturesEquivalent(SymbolParamTypeSig, ASTFuncParamNode->Obj.TypeSignature))
			{
				Integrator_Error(Integrator, FuncASTNode->BufferLocation, "Inconsistent param types for function '%s' redeclaration.", FuncASTNode->Obj.Name.Str);
				return NULL;
			}
		}
	}
	else
	{
		FuncSymbol = AllocSymbol(SYMBOL_TYPE_FUNCTION);
		FuncSymbol->Name = String_Copy_ANSI(FuncASTNode->Obj.Name);

		// Parse symbol parameters and return type.
		FuncSymbol->Function.ReturnType = FuncASTNode->Obj.TypeSignature;

		FuncSymbol->Function.ParamTypeSignatures = Vector_Create(struct TypeSignature*, 0);
		for (int ParamIndex = 0; ParamIndex < FuncASTNode->Obj.Func.Params.Size; ParamIndex++)
		{
			struct AST_Node* ParamASTNode = Vector_GetValueAt(FuncASTNode->Obj.Func.Params, struct AST_Node*, ParamIndex);
			ASSERT(ParamASTNode != NULL);
			ASSERT(ParamASTNode->Obj.TypeSignature != NULL);

			Vector_Push(FuncSymbol->Function.ParamTypeSignatures, struct TypeSignature*, ParamASTNode->Obj.TypeSignature);
		}

		Scope_AddSymbol(Scope, FuncSymbol);
	}

	if (FuncASTNode->Obj.Func.StatementsBlock == NULL)
	{
		return FuncSymbol;
	}

	FuncSymbol->Function.Scope = AllocScope(Integrator->ProgramTree->RootScope);
	FuncSymbol->Function.LocalVariables = Vector_Create(struct ProgramSymbol*, FuncASTNode->Obj.Func.Params.Size);
	FuncSymbol->Function.Instructions = Vector_Create(struct ProgramInstruction*, 1);

	// Integrate parameters as variables symbols.
	for (int ParamIndex = 0; ParamIndex < FuncASTNode->Obj.Func.Params.Size; ParamIndex++)
	{
		struct AST_Node* ParamVarASTNode = Vector_GetValueAt(FuncASTNode->Obj.Func.Params, struct AST_Node*, ParamIndex);
		ASSERT(ParamVarASTNode != NULL);
		ASSERT(ParamVarASTNode->Type == AST_NODE_OBJ_VAR);

		struct ProgramSymbol* ParamVarSymbol = IntegrateObj_Variable(Integrator, ParamVarASTNode, FuncSymbol->Function.Scope);
		if (ParamVarSymbol == NULL)
		{
			Integrator_Error(Integrator, ParamVarASTNode->BufferLocation, "Failed to build function parameter symbol.");
			return NULL;
		}

		Scope_AddSymbol(FuncSymbol->Function.Scope, ParamVarSymbol);
		Vector_Push(FuncSymbol->Function.LocalVariables, struct ProgramSymbol*, ParamVarSymbol);
	}

	// Integrate function block.
	IntegrateStatementBlock(Integrator, FuncSymbol, FuncSymbol->Function.Scope, FuncASTNode->Obj.Func.StatementsBlock);
	if (Integrator->HasError)
	{
		Integrator_Error(Integrator, FuncASTNode->Obj.Func.StatementsBlock->BufferLocation, "Error parsing function definition block.");
		return NULL;
	}

	return FuncSymbol;
}

ui8 IntegrateStructMemberVariable(struct IntegratorProcess* Integrator, struct ProgramSymbol* StructSymbol, struct ProgramSymbol* MemberSymbol, ui64* StructBitSize)
{
	ASSERT(StructSymbol != NULL);
	ASSERT(MemberSymbol != NULL);

	// Resolve variable offsets, structure size and alignment.
	if (!StructSymbol->Struct.IsUnion)
	{
		if (MemberSymbol->Variable.BitSize % 8 != 0)
		{
			// Variable has a custom bit size not based on a multiple of 8 / bytes.
			// It can be applied on top of current struct bit size, and given a bit offset on top of its byte offset.
			MemberSymbol->Variable.Offset = *StructBitSize / 8;
			MemberSymbol->Variable.BitOffset = *StructBitSize % 8;
			*StructBitSize += MemberSymbol->Variable.BitSize;
		}
		else
		{
			// Variable has a standard byte size. Ensure it is located on a byte boundary related to
			// its desired alignment.

			// Adjust struct size to match alignment boundary required by this member (TODO: Keep track of padding ?)
			*StructBitSize += (MemberSymbol->Variable.BitSize - *StructBitSize % MemberSymbol->Variable.BitSize) % MemberSymbol->Variable.BitSize;

			// Place member, increase struct size.
			MemberSymbol->Variable.Offset = *StructBitSize / 8;
			*StructBitSize += MemberSymbol->Variable.BitSize;
		}		

		// Update struct alignment if required.
		ui32 VarAlign = (MemberSymbol->Variable.BitSize + 7) / 8;
		StructSymbol->Struct.Alignment = max(VarAlign, StructSymbol->Struct.Alignment);
	}
	else
	{
		MemberSymbol->Variable.Offset = 0;
		*StructBitSize = max(*StructBitSize, MemberSymbol->Variable.BitSize);
		StructSymbol->Struct.Alignment = max(StructSymbol->Struct.Alignment, (*StructBitSize + 7) / 8);
	}

	Scope_AddSymbol(StructSymbol->Struct.Scope, MemberSymbol);
}

// Builds the definition for a structure symbol, resolving its final size, members and their offsets,
// and adding it to the global scope if necessary.
// If there is a pre-existing declaration symbol for the struct (with no already-defined size),
// it will take over as the defined symbol.
struct ProgramSymbol* IntegrateObj_Structure(struct IntegratorProcess* Integrator, struct AST_Node* StructASTNode, struct SymbolScope* Scope)
{
	ASSERT(StructASTNode != NULL);

	struct ProgramSymbol* StructSymbol = Scope_FindSymbol(Scope, &StructASTNode->Obj.Name, 0);

	if (StructSymbol == NULL)
	{
		// Symbol didn't previously exist. Create it.
		StructSymbol = AllocSymbol(StructASTNode->Obj.Struct.IsUnion ? SYMBOL_TYPE_UNION : SYMBOL_TYPE_STRUCT);
		StructSymbol->Name = String_Copy_ANSI(StructASTNode->Obj.Name);
		StructSymbol->Struct.IsUnion = StructSymbol->Type == SYMBOL_TYPE_UNION;

		Scope_AddSymbol(Scope, StructSymbol);
	}
	else
	{
		// Check for existing definition & type compatibility.
		if (StructSymbol->Struct.IsUnion != StructASTNode->Obj.Struct.IsUnion)
		{
			Integrator_Error(Integrator, StructASTNode->BufferLocation, "Inconsistent struct / union '%s' redeclaration.", StructASTNode->Obj.Name.Str);
			return NULL;
		}
		if (StructSymbol->Struct.Size > 0)
		{
			Integrator_Error(Integrator, StructASTNode->BufferLocation, "Struct symbol '%s' redefinition.", StructASTNode->Obj.Name.Str);
			return NULL;
		}
	}

	StructSymbol->Struct.Scope = AllocScope(Scope);
	StructSymbol->Struct.Size = 1;
	StructSymbol->Struct.Alignment = 1;

	// Parse member variable nodes.
	ui64 StructBitSize = 0;
	for (int MemberVarIndex = 0; MemberVarIndex < StructASTNode->Obj.Struct.Members.Size; MemberVarIndex++)
	{
		struct AST_Node* MemberASTNode = Vector_GetValueAt(StructASTNode->Obj.Struct.Members, struct AST_Node*, MemberVarIndex);

		if (MemberASTNode->Type == AST_NODE_OBJ_STRUCT)
		{
			// Integrate any sub-structure found into the program's global scope, then copy their members over.

			struct ProgramSymbol* SubStructSymbol = IntegrateASTObjectNode(Integrator, MemberASTNode, Scope); // Integrate into the same parent scope.
			if (Integrator->HasError) goto INTEGRATE_FAIL;
			ASSERT(SubStructSymbol != NULL);

			// Copy all the sub-structure members over to the parent, adjust the offsets (if required) and
			// treat them as a single whole for alignment.

			// Bring the parent structure to a byte boundary if needed.
			StructBitSize += (StructBitSize % 8) % 8;

			for (int SubStructMemberIndex = 0; SubStructMemberIndex < SubStructSymbol->Struct.Scope->Symbols.Size; SubStructMemberIndex++)
			{
				struct ProgramSymbol* SubStructVarSymbol = Vector_GetValueAt(SubStructSymbol->Struct.Scope->Symbols, struct ProgramSymbol*, SubStructMemberIndex);
				
				// The copy can be shallow, but we still do need a copy so we can give the copy different offsets than its original.
				struct ProgramSymbol* MemberSymbol = calloc(1, sizeof(struct ProgramSymbol));
				*MemberSymbol = *SubStructVarSymbol;

				// Offset the offset by the parent structure's current size.
				if (!StructSymbol->Struct.IsUnion)
					MemberSymbol->Variable.Offset += StructBitSize / 8;

				// Add the member to the parent structure scope directly.
				Scope_AddSymbol(StructSymbol->Struct.Scope, MemberSymbol);
			}

			// Update the parent structure size with the substructure's, and update alignment.
			if (!StructSymbol->Struct.IsUnion)
				StructBitSize += SubStructSymbol->Struct.Size * 8;
			else
				StructBitSize = max(StructBitSize, SubStructSymbol->Struct.Size * 8);

			StructSymbol->Struct.Alignment = max(StructSymbol->Struct.Alignment, SubStructSymbol->Struct.Alignment);
			continue;
		}

		struct ProgramSymbol* MemberSymbol = IntegrateObj_Variable(Integrator, MemberASTNode, StructSymbol->Struct.Scope);
		if (MemberSymbol == NULL)
		{
		INTEGRATE_FAIL:
			FreeSymbol(StructSymbol);
			return NULL;
		}

		if (MemberASTNode->Obj.Var.Initializer.Expression != NULL)
		{
			i64 BitSizeOverride = 0;
			enum DATATYPE BitSizeType = 0;
			if (!EvalConstantExpression(Integrator, Scope, MemberASTNode->Obj.Var.Initializer.Expression, &BitSizeOverride, &BitSizeType))
			{
				return 0;
			}

			if (BitSizeType == DATATYPE_INT32)
			{
				BitSizeOverride = (i64)(*(i32*)&BitSizeOverride);
			}

			if (BitSizeType != DATATYPE_INT64)
			{
				Integrator_Error(Integrator,MemberASTNode->Obj.Var.Initializer.Expression->BufferLocation, "Expression must be an integral.");
				return 0;
			}

			MemberSymbol->Variable.BitSize = BitSizeOverride;
		}

		if(!IntegrateStructMemberVariable(Integrator, StructSymbol, MemberSymbol, &StructBitSize))
		{
			goto INTEGRATE_FAIL;
		}
	}

	// Resolve final struct size. Make sure it reaches an alignment boundary.
	StructBitSize += (StructBitSize % 8) % 8;
	StructSymbol->Struct.Size = StructBitSize / 8;
	if (!StructSymbol->Struct.IsUnion)
		StructSymbol->Struct.Size += (StructSymbol->Struct.Size % StructSymbol->Struct.Alignment) % StructSymbol->Struct.Alignment;

	return StructSymbol;
}

struct ProgramSymbol* IntegrateObj_Enum(struct IntegratorProcess* Integrator, struct AST_Node* EnumASTNode, struct SymbolScope* Scope)
{
	ASSERT(EnumASTNode != NULL);

	struct ProgramSymbol* EnumSymbol = Scope_FindSymbol(Scope, &EnumASTNode->Obj.Name, 0);
	if (EnumSymbol == NULL)
	{
		EnumSymbol = AllocSymbol(SYMBOL_TYPE_ENUM);
		EnumSymbol->Name = String_Copy_ANSI(EnumASTNode->Obj.Name);
		Scope_AddSymbol(Scope, EnumSymbol);
	}
	else
	{
		// Check for existing definition.
		if (EnumSymbol->Enum.UnderlyingTypeSize > 0)
		{
			Integrator_Error(Integrator, EnumASTNode->BufferLocation, "Enum symbol '%s' redefinition.", EnumASTNode->Obj.Name.Str);
			return NULL;
		}
	}

	// Go over the value expressions and attempt to evaluate them. Keep track of the highest value.
	i64 NextVal = 0;
	for (int EnumValIndex = 0; EnumValIndex < EnumASTNode->Obj.Enum.Members.Size; EnumValIndex++)
	{
		struct Expression* ValExpression = Vector_GetValueAt(EnumASTNode->Obj.Enum.Members, struct Expression*, EnumValIndex);

		if (ValExpression->Type == EXP_VAR_ACCESS)
		{
			// Expression is simple and only specifies a name. Emit a ENUM_VALUE symbol.
			struct ProgramSymbol* ValSymbol = AllocSymbol(SYMBOL_TYPE_ENUM_VAL);
			ValSymbol->Name = String_Copy_ANSI(ValExpression->Variable.Name);
			ValSymbol->Enum_Member.NumericValue = NextVal++;

			// Push value symbol to enum's values vector.
			Vector_Push(EnumSymbol->Enum.Values, struct ProgramSymbol*, ValSymbol);

			// Also add to enum's own scope.
			Scope_AddSymbol(Scope, ValSymbol);
			continue;
		}

		// ... Otherwise we have a <VAR ACCESS> = <VAL> expression. VAL needs to evaluate to an integral number.
		
		ui64 EvalRes;
		enum DATATYPE EvalType;

		ASSERT(ValExpression->Type == EXP_OP && ValExpression->Op.RightOperand != NULL);
		ASSERT(ValExpression->Op.LeftOperand != NULL && ValExpression->Op.LeftOperand->Type == EXP_VAR_ACCESS);
		if (!EvalConstantExpression(Integrator, Scope, ValExpression->Op.RightOperand, &EvalRes, &EvalType))
		{
			Integrator_Error(Integrator, ValExpression->Op.RightOperand->BufferLocation, "Invalid expression for Enumeration value.");
			return NULL;
		}
	
		if (EvalType == DATATYPE_INT32)
		{
			EvalRes = (i64)(*(i32*)&EvalRes);
		}

		if (EvalType != DATATYPE_INT64)
		{
			Integrator_Error(Integrator, ValExpression->Op.RightOperand->BufferLocation, "Expression must be an integral.");
			return 0;
		}

		// Expression is simple and only specifies a name. Emit a ENUM_VALUE symbol.
		struct ProgramSymbol* ValSymbol = AllocSymbol(SYMBOL_TYPE_ENUM_VAL);
		ValSymbol->Name = String_Copy_ANSI(ValExpression->Op.LeftOperand->Variable.Name);
		ValSymbol->Enum_Member.NumericValue = EvalRes;
		NextVal = EvalRes + 1;

		// Push value symbol to enum's values vector.
		Vector_Push(EnumSymbol->Enum.Values, struct ProgramSymbol*, ValSymbol);

		// Also add to enum's own scope.
		Scope_AddSymbol(Scope, ValSymbol);
	}

	EnumSymbol->Enum.UnderlyingTypeSize = 8; // TODO: Reduce to lower size if possible.

	return EnumSymbol;
}

struct ProgramSymbol* IntegrateObj_Typedef(struct IntegratorProcess* Integrator, struct AST_Node* TypedefASTNode, struct SymbolScope* Scope)
{
	ASSERT(TypedefASTNode != NULL);

	// The underlying type of the node can be any kind of object. The point of the Typedef symbol is to "concatenate" itself to whatever other object is declared
	// to use it as a type.

	struct ProgramSymbol* NewSymbol = Scope_FindSymbol(Scope, &TypedefASTNode->Obj.Name, 0);
	if (NewSymbol != NULL)
	{
		// Redefinition error.
		// TODO: Allow redefinition if it exactly matches existing symbol.
		Integrator_Error(Integrator, TypedefASTNode->BufferLocation, "Typedef '%s' redefinition.", TypedefASTNode->Obj.Name.Str);
		return NULL;
	}

	NewSymbol = AllocSymbol(SYMBOL_TYPE_TYPEDEF);
	NewSymbol->Name = String_Copy_ANSI(TypedefASTNode->Obj.Name);
	NewSymbol->Typedef.Type = TypedefASTNode->Obj.TypeSignature;

	if (!IntegrateTypeSignature(Integrator, Scope, NewSymbol->Typedef.Type, &NewSymbol->Typedef.BaseSymbol))
	{
		Integrator_Error(Integrator, TypedefASTNode->BufferLocation, "Invalid type use with typedef '%s'.", NewSymbol->Name.Str);
		return NULL;
	}

	Scope_AddSymbol(Scope, NewSymbol);
	return NewSymbol;
}

// Integrates one or more new symbol(s) from an AST Object Node and places them within the specified Scope.
struct ProgramSymbol* IntegrateASTObjectNode(struct IntegratorProcess* Integrator, struct AST_Node* ObjASTNode, struct SymbolScope* Scope)
{
	ASSERT(ObjASTNode != NULL);

	struct ProgramSymbol* NewSymbol = NULL;

	if (ObjASTNode->Obj.IsTypedef)
	{
		NewSymbol = IntegrateObj_Typedef(Integrator, ObjASTNode, Scope);
	}
	else
	{
		switch (ObjASTNode->Type)
		{
		case AST_NODE_OBJ_VAR:
			NewSymbol = IntegrateObj_Variable(Integrator, ObjASTNode, Scope);
			break;
		case AST_NODE_OBJ_FUNC:
			NewSymbol = IntegrateObj_Function(Integrator, ObjASTNode, Scope);
			break;
		case AST_NODE_OBJ_STRUCT:
			NewSymbol = IntegrateObj_Structure(Integrator, ObjASTNode, Scope);
			break;
		case AST_NODE_OBJ_ENUM:
			NewSymbol = IntegrateObj_Enum(Integrator, ObjASTNode, Scope);
			break;
		default:
			break;
		}
	}

	if (NewSymbol == NULL)
	{
	INTEGRATE_FAIL:
		Integrator_Error(Integrator, ObjASTNode->BufferLocation, 
			"Failed to integrate object symbol. Object type = %d", ObjASTNode->Type); // TODO: Add Root node to string converter.
		return NULL;
	}

	return NewSymbol;
}

// Main Integrator Process function. Turns the SourceASTs vector within the Process into an Integrated Program Tree (ProgramTree).
// The Integrator itself, the input AST Root nodes and output Program Tree must all be allocated and assigned.
void Integrator_Run(struct IntegratorProcess* Integrator)
{
	ASSERT(Integrator != NULL);
	ASSERT(Integrator->ASTRootNodes != NULL);
	ASSERT(Integrator->ProgramTree != NULL);

	// Allocate root scope and start repeatedly integrating root scope symbols in order of declaration.
	Integrator->ProgramTree->RootScope = AllocScope(NULL);

	for (int RootNodeIndex = 0; RootNodeIndex < Integrator->ASTRootNodes->Size; RootNodeIndex++)
	{
		struct AST_Node* RootNode = *(struct AST_Node**)(Vector_GetPtr(Integrator->ASTRootNodes, RootNodeIndex));
		struct ProgramSymbol* IntegratedSymbol = IntegrateASTObjectNode(Integrator, RootNode, Integrator->ProgramTree->RootScope);
		if (Integrator->HasError) return;
	}
}
