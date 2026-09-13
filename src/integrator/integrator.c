#include "integrator.h"
#include <stdarg.h>

#include "integrator_expressions.c"
#include "integrator_functions.c"
#include "integrator_logging.c"

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

void Integrator_DiscardError(struct IntegratorProcess* Integrator)
{
	if (!Integrator->HasError) return;

	Integrator->HasError = 0;
	Integrator->Error.Location = 0;
	String_Free_ANSI(&Integrator->Error.Message);
}

struct ProgramSymbol* AllocSymbol(enum SYMBOL_TYPE Type)
{
	struct ProgramSymbol* NewSymbol = calloc(1, sizeof(struct ProgramSymbol));
	ASSERT(NewSymbol != NULL);
	NewSymbol->Type = Type;

	switch (Type)
	{
	case SYMBOL_TYPE_ENUM:
		NewSymbol->Enum.Values = Vector_Create(struct ProgramSymbol*, 2);
		break;
	default:
		break;
	}

	return NewSymbol;
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

// Returns the resolved size of a passed in type signature. If array sizes were pre-assigned, they are IGNORED !
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

	if (TypeSig->IsFunctionPointer || TypeSig->PointerLevel)
	{
		TypeSig->Size = POINTER_SIZE;
		if (TypeSig->Type != DATATYPE_USER_DEFINED)
		{
			// Type is pointer to primitive.
			goto TYPE_INTEGRATE_SUCCESS;
		}
	}
	else if (TypeSig->Type != DATATYPE_USER_DEFINED)
	{
		// Type is non-pointer primitive. Its size is already set.
		goto TYPE_INTEGRATE_SUCCESS;
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

		TypeSig->Size = POINTER_SIZE;
		goto TYPE_INTEGRATE_SUCCESS;
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

TYPE_INTEGRATE_SUCCESS:
	return TypeSig->Size; // Will be 0 if the type exists but is incomplete.
}

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
		if (!TypeSignaturesEquivalent(VarSymbol->Variable.DeclarationType, VarASTNode->Obj.TypeSignature, 1))
		{
			Integrator_Error(Integrator, VarASTNode->BufferLocation, "Incoherent types in variable '%s' redeclaration.", VarASTNode->Obj.Name.Str);
			return NULL;
		}
		
		if (VarASTNode->Obj.Var.Initializer.Expression != NULL && VarSymbol->Variable.HasInitializer)
		{
			Integrator_Error(Integrator, VarASTNode->BufferLocation, "Variable '%s' redefinition.", VarASTNode->Obj.Name.Str);
			return NULL;
		}
	}

	// Handle array size(s).
	// Resolve array size expressions. Error out if any of the expressions cannot be resolved at compile-time.
	// If the Var Symbol already exists, also error out if the array sizes differ from original declaration.
	struct Vector ArraySizes = Vector_Create(i64, 0);
	for (int ArraySizeExpIndex = 0; ArraySizeExpIndex < VarASTNode->Obj.Var.ArraySizes.Size; ArraySizeExpIndex++)
	{
		struct Expression* ArraySizeExp = Vector_GetValueAt(VarASTNode->Obj.Var.ArraySizes, struct Expression*, ArraySizeExpIndex);
		ASSERT(ArraySizeExp != NULL);

		i64 EvalResult;
		enum DATATYPE EvalType;
		if (!EvalConstantExpression(Integrator, Scope, ArraySizeExp, &EvalResult, &EvalType))
		{
			Integrator_Error(Integrator, ArraySizeExp->BufferLocation, "Array size expression must be constant value.");
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
			return NULL;
		}

		if (EvalResult == 0 && ArraySizeExpIndex > 0)
		{
			Integrator_Error(Integrator, ArraySizeExp->BufferLocation, "Only the first array subscript can be empty.");
			return NULL;
		}

		// Compare result against original var symbol's corresponding array size.
		if (VarSymbol != NULL && (ArraySizeExpIndex > 0 || EvalResult != 0))
		{
			if (Vector_GetValueAt(VarSymbol->Variable.DeclarationType->ArraySizes, i64, ArraySizeExpIndex) != EvalResult)
			{
				Integrator_Error(Integrator, ArraySizeExp->BufferLocation, "Incoherent array subscripts with existing declaration.");
				return NULL;
			}
		}

		Vector_Push(ArraySizes, i64, EvalResult);
	}

	struct TypeSignature* TypeSig = NULL;
	if (VarSymbol == NULL)
	{
		TypeSig = AllocTypeSignatureCopy(VarASTNode->Obj.TypeSignature);
	}
	else
	{
		TypeSig = VarSymbol->Variable.DeclarationType;
	}

	ui8 DefinedArray = ArraySizes.Size > 0 && Vector_GetValueAt(ArraySizes, i64, 0) > 0; // If the first dimension of the array has a specified size, then this is a full definition.
	ui8 ExistingArrayDefined = VarSymbol != NULL && (Vector_GetValueAt(TypeSig->ArraySizes, i64, 0) > 0);

	// If Var Symbol doesn't already exist, create it now.
	if (VarSymbol == NULL)
	{
		VarSymbol = AllocSymbol(SYMBOL_TYPE_VARIABLE);
		VarSymbol->Name = String_Copy_ANSI(VarASTNode->Obj.Name);
		VarSymbol->Variable.DeclarationType = TypeSig;

		Scope_AddSymbol(Scope, VarSymbol);

		// Integrate type into program as a symbol and make sure it is usable.
		struct ProgramSymbol* TypeSymbol = NULL;
		VarSymbol->Variable.BitSize = IntegrateTypeSignature(Integrator, Scope, TypeSig, &TypeSymbol) * 8;
		if (VarSymbol->Variable.BitSize == 0)
		{
			if (TypeSymbol != NULL)
			{
				Integrator_Error(Integrator, VarASTNode->BufferLocation, "Incoherent usage of type '%s'.", TypeSig->TypeName.Str);
				return NULL;
			}

			Integrator_Error(Integrator, VarASTNode->BufferLocation, "Use of incomplete type '%s'.", TypeSig->TypeName.Str);
			return NULL;
		}

		// Give the type its array sizes.
		TypeSig->ArraySizes = ArraySizes;
	}
	else if (DefinedArray)
	{
		*Vector_GetPtrAt(TypeSig->ArraySizes, i64, 0) = Vector_GetValueAt(ArraySizes, i64, 0);
		Vector_Destroy(&ArraySizes);
	}

	// Compute final var size if we now have a defined array.
	if (DefinedArray && !ExistingArrayDefined)
	{
		for (int i = 0; i < TypeSig->ArraySizes.Size; i++)
		{
			VarSymbol->Variable.BitSize *= Vector_GetValueAt(TypeSig->ArraySizes, i64, i);
		}
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

void IntegrateStructMemberVariable(struct IntegratorProcess* Integrator, struct ProgramSymbol* StructSymbol, struct ProgramSymbol* MemberSymbol, ui64* StructBitSize)
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
			if (Integrator->HasError) return NULL;
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

		IntegrateStructMemberVariable(Integrator, StructSymbol, MemberSymbol, &StructBitSize);
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
