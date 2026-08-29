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

void PrintSymbol(struct ProgramSymbol* ProgramSymbol)
{

}

void Integrator_PrintTree(struct IntegratorProcess* Integrator)
{
	ASSERT(Integrator != NULL);
	ASSERT(Integrator->ProgramTree != NULL);

	printf("\n===== INTEGRATOR OUTPUT =====\n\n");

	// Print root scope symbols.
	for (int RootSymbolIndex = 0; RootSymbolIndex < Integrator->ProgramTree->RootScope->Symbols.Size; RootSymbolIndex++)
	{
		
	}
}

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
	}
}

struct SymbolScope* AllocScope()
{
	struct SymbolScope* NewScope = calloc(1, sizeof(struct SymbolScope));
	ASSERT(NewScope != NULL);
	NewScope->Symbols = Vector_Create(struct ProgramSymbol*, 1);
	return NewScope;
}

void FreeScope(struct SymbolScope* Scope)
{
	if (Scope == NULL);

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

// Returns the resolved size of a passed in type signature.
// Returns 0 if trying to use an incomplete type, non-pointer signature.
// 
// If the type is primitive or a pointer then this is trivial and just returns the type's size.
// If not, then the whole program tree as it currently exists will be searched to find a matching symbol.
// In that case if a symbol is successfuly found, it is returned through the OutTypeSymbol parameter.
// If not, a new symbol declaration is created, added to the Program Tree's root scope and returned through OutTypeSignature
// Note: This only happens for pointer type signatures. Non-pointer type signatures that use an undeclared type will trigger an error.
ui64 IntegrateTypeSignature(struct IntegratorProcess* Integrator, struct TypeSignature* TypeSig, struct ProgramSymbol* OutTypeSymbol)
{
	ASSERT(TypeSig != NULL);

	ui64 TypeSize = 0;
	if (TypeSig->IsFunctionPointer || TypeSig->PointerLevel)
	{
		TypeSize = POINTER_SIZE;
		if (TypeSig->Type != DATATYPE_USER_DEFINED)
		{
			// Type is pointer to primitive.
			OutTypeSymbol = NULL;
			return TypeSize;
		}
	}
	else if (TypeSig->Type != DATATYPE_USER_DEFINED)
	{
		// Type is non-pointer primitive.
		TypeSize = TypeSig->Size;
		OutTypeSymbol = NULL;

		return TypeSize;
	}

	// From here we're dealing with a non-primitive type.
	// If it's a pointer, we try to find a matching symbol but may create one from scratch, effectively
	// making this the declaration site for it.
	// If not, we MUST find a matching DECLARED / RESOLVED type symbol (Struct / Union, Typedef or Enum).

	return 0; // TEMP Unimplemented handling of complex types.
}

ui8 EvalConstantExpression(struct IntegratorProcess* Integrator, struct Expression* Expression, i64* OutResult, enum DATATYPE* OutResultType);

// Resolves an operation between two expressions, recursively.
ui8 EvalConstantOpExpression(struct IntegratorProcess* Integrator, enum TOKEN_SYMBOL Op, struct Expression* LeftOperand, struct Expression* RightOperand,
	ui64* OutResult, enum DATATYPE* OutResultType)
{
	ASSERT(OutResult != NULL);
	ASSERT(
		   Symbol_IsLeftUnaryOp(Op) && RightOperand != NULL
		|| Symbol_IsRightUnaryOp(Op) && LeftOperand != NULL
		|| Symbol_IsBinaryOp(Op) && LeftOperand != NULL && RightOperand != NULL
	);

	ui32 BufferLoc = LeftOperand != NULL ? LeftOperand->BufferLocation : RightOperand->BufferLocation;

	i64 LeftOpRes, RightOpRes;
	enum DATATYPE LeftOpResType, RightOpResType;
	if (LeftOperand != NULL 
		&& !EvalConstantExpression(Integrator, LeftOperand, &LeftOpRes, &LeftOpResType)) return 0;
	if (RightOperand != NULL 
		&& !EvalConstantExpression(Integrator, RightOperand, &RightOpRes, &RightOpResType)) return 0;

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
ui8 EvalConstantExpression(struct IntegratorProcess* Integrator, struct Expression* Expression, i64* OutResult, enum DATATYPE* OutResultType)
{
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
		// Invalid base cases
	case EXP_LITERAL_STRING:
	case EXP_NOP:
	case EXP_FUNC_CALL:
	case EXP_VAR_ACCESS: // TODO: This can work if the "variable" is actually an enum value name.
	default:
		Integrator_Error(Integrator, Expression->BufferLocation, "Expression must be constant integral.");
		return 0;
		// Complex cases
	case EXP_OP:
		return EvalConstantOpExpression(Integrator, Expression->Op.OperatorSymbol, Expression->Op.LeftOperand, Expression->Op.RightOperand, OutResult, OutResultType);
	}
}

// Returns an integrated Variable symbol from a corresponding Variable AST object.
// The variable's type and size is resolved, but its final size (if bit count is specified) and offset must be
// resolved by the caller according to its context, and its parent scope must be assigned.
struct ProgramSymbol* BuildSymbol_Variable(struct IntegratorProcess* Integrator, struct AST_Node* VarASTNode, ui8 IsStructMember)
{
	ASSERT(VarASTNode != NULL);

	struct ProgramSymbol* VarSymbol = AllocSymbol(SYMBOL_TYPE_VARIABLE);
	VarSymbol->Name = String_Copy_ANSI(VarASTNode->Obj.Name);

	// Handle Type Signature.
	struct ProgramSymbol* TypeSymbol = NULL;
	VarSymbol->Variable.DeclarationType = AllocTypeSignatureCopy(VarASTNode->Obj.TypeSignature);
	VarSymbol->Variable.BitSize = IntegrateTypeSignature(Integrator, VarSymbol->Variable.DeclarationType, TypeSymbol);

	// Handle array size(s).
	// Resolve array size expressions. Error out if any of the expressions cannot be resolved at compile-time.
	for (int ArraySizeExpIndex = 0; ArraySizeExpIndex < VarASTNode->Obj.Var.ArraySizes.Size; ArraySizeExpIndex++)
	{
		struct Expression* ArraySizeExp = Vector_GetValueAt(VarASTNode->Obj.Var.ArraySizes, struct Expression*, ArraySizeExpIndex);
		ASSERT(ArraySizeExp != NULL);

		i64 EvalResult;
		enum DATATYPE EvalType;
		if (!EvalConstantExpression(Integrator, ArraySizeExp, &EvalResult, &EvalType))
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

		Vector_Push(VarSymbol->Variable.ArraySizes, ui64, EvalResult);
		// Multiply size by each array layer's resolved size.
		VarSymbol->Variable.BitSize *= EvalResult;
	}

	if (VarSymbol->Variable.BitSize == 0)
	{
		Parser_Error(Integrator, VarASTNode->BufferLocation, "Use of incomplete type '%s'.", VarSymbol->Variable.DeclarationType->TypeName.Str);
		return NULL;
	}

	VarSymbol->Variable.BitSize *= 8; // Bytes -> Bits conversion.
	return VarSymbol;
}

void IntegrateRootASTNode(struct IntegratorProcess* Integrator, struct AST_Node* RootASTNode)
{
	ASSERT(RootASTNode != NULL);

	struct ProgramSymbol* NewSymbol = NULL;

	switch (RootASTNode->Type)
	{
	case AST_NODE_OBJ_VAR:
		NewSymbol = BuildSymbol_Variable(Integrator, RootASTNode, 0);
		if (NewSymbol == NULL) goto INTEGRATE_FAIL;

		NewSymbol->Variable.Offset = 0; // TODO: Handle global var offsets.
		break;
	case AST_NODE_OBJ_STRUCT:
		break;
	default:
		// TEMP: Do nothing.
		break;
	}

	if (NewSymbol == NULL)
	{
	INTEGRATE_FAIL:
		Integrator_Error(Integrator, RootASTNode->BufferLocation, 
			"Failed to integrate root object symbol. Object type = %d", RootASTNode->Type); // TODO: Add Root node to string converter.
		return;
	}
	Scope_AddSymbol(Integrator->ProgramTree->RootScope, NewSymbol);
}

// Main Integrator Process function. Turns the SourceASTs vector within the Process into an Integrated Program Tree (ProgramTree).
// The Integrator itself, the input AST Root nodes and output Program Tree must all be allocated and assigned.
void Integrator_Run(struct IntegratorProcess* Integrator)
{
	ASSERT(Integrator != NULL);
	ASSERT(Integrator->ASTRootNodes != NULL);
	ASSERT(Integrator->ProgramTree != NULL);

	// Allocate root scope and start repeatedly integrating root scope symbols in order of declaration.
	Integrator->ProgramTree->RootScope = AllocScope();

	for (int RootNodeIndex = 0; RootNodeIndex < Integrator->ASTRootNodes->Size; RootNodeIndex++)
	{
		struct AST_Node* RootNode = *(struct AST_Node**)(Vector_GetPtr(Integrator->ASTRootNodes, RootNodeIndex));
		IntegrateRootASTNode(Integrator, RootNode);
		if (Integrator->HasError) return;
	}
}
