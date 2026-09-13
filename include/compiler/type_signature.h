
#ifndef TYPE_SIGNATURE_INCLUDED
#define TYPE_SIGNATURE_INCLUDED

// Values for primitive types + an extra value indicating the type is user-defined. 
enum DATATYPE
{
	DATATYPE_UNKNOWN,
	DATATYPE_VOID,
	DATATYPE_CHAR,
	DATATYPE_SHORT,
	DATATYPE_INT32,
	DATATYPE_INT64,
	DATATYPE_FLOAT,
	DATATYPE_DOUBLE,

	DATATYPE_USER_DEFINED, // Indicates the type is complex and relates to a program object.
};

// Flags modifying the behavior / definition of a type.
enum TYPE_SIG_FLAGS
{
	TYPE_IS_UNSIGNED = 1 << 0,
	TYPE_IS_STATIC = 1 << 1,
	TYPE_IS_EXTERN = 1 << 2,
	TYPE_IS_CONST = 1 << 3,
	TYPE_IS_VOLATILE = 1 << 4,
	TYPE_IS_STRUCTURED = 1 << 5, // If set, this type contains sub-symbols. 
	TYPE_IS_ENUM_OR_UNION = 1 << 6, // If set, this is an enum if STRUCTURED is 0, or a union if STRUCTURED is 1.
};

// Data Type information for a program Object.
struct TypeSignature
{
	enum DATATYPE Type;
	enum TYPE_SIG_FLAGS Flags;

	ui16 Size; // Total size in bytes. Resolved during integration except for primitive types. Does NOT account for array sizes !
	struct String_ANSI TypeName; // String representation of the type / actual type name for USER_DEFINED types.

	ui8 PointerLevel; // How many dereferences are required to reach the base data. If this is a function pointer, applies to the return value.

	ui8 IsFunctionPointer; // Is this a function pointer ?
	struct
	{
		ui8 PointerLevel; // How many dereferences are required to reach function (callable when == 1, always > 0).
		struct Vector ParamTypes; // Vector type = TypeSignature*. Type signature of parameters.
	} FuncPtr;

	struct Vector ArraySizes; // Contains the sizes of the array dimensions carried by this type, if any. Vector type = i64. Determined on Integration.
};

#define POINTER_SIZE (_WIN64 ? 8 : 4)

static inline struct TypeSignature GetPrimitiveTypeSignature_Void() 
{
	struct TypeSignature Def = { 0 };
	Def.Size = 0;
	Def.Type = DATATYPE_VOID;
	return Def;
}

static inline struct TypeSignature GetPrimitiveTypeSignature_Char() 
{
	struct TypeSignature Def = { 0 };
	Def.Size = 1;
	Def.Type = DATATYPE_CHAR;
	return Def;
}

static inline struct TypeSignature GetPrimitiveTypeSignature_Short()
{
	struct TypeSignature Def = { 0 };
	Def.Size = 2;
	Def.Type = DATATYPE_SHORT;
	return Def;
}

static inline struct TypeSignature GetPrimitiveTypeSignature_Int32() 
{
	struct TypeSignature Def = { 0 };
	Def.Size = 4;
	Def.Type = DATATYPE_INT32;
	return Def;
}

static inline struct TypeSignature GetPrimitiveTypeSignature_Int64() 
{
	struct TypeSignature Def = { 0 };
	Def.Size = 8;
	Def.Type = DATATYPE_INT64;
	return Def;
}

static inline struct TypeSignature GetPrimitiveTypeSignature_Float() 

{
	struct TypeSignature Def = { 0 };
	Def.Size = 4;
	Def.Type = DATATYPE_FLOAT;
	return Def;
}

static inline struct TypeSignature GetPrimitiveTypeSignature_Double() 
{
	struct TypeSignature Def = { 0 };
	Def.Size = 8;
	Def.Type = DATATYPE_DOUBLE;
	return Def;
}

static inline struct TypeSignature GetPrimitiveTypeSignature_String() 
{
	struct TypeSignature Def = { 0 };
	Def.PointerLevel = 1;
	Def.Size = POINTER_SIZE;
	Def.Type = DATATYPE_CHAR;
	Def.Flags = TYPE_IS_CONST;
	return Def;
}

static inline struct TypeSignature* AllocTypeSignature()
{
	struct TypeSignature* New = calloc(1, sizeof(struct TypeSignature));
	ASSERT(New != NULL);
	return New;
}

static inline struct TypeSignature* AllocTypeSignatureCopy(struct TypeSignature* SrcType)
{
	ASSERT(SrcType != NULL);

	struct TypeSignature* New = AllocTypeSignature();
	*New = *SrcType;

	if (New->IsFunctionPointer)
	{
		New->FuncPtr.ParamTypes = Vector_Create(struct TypeSignature*, SrcType->FuncPtr.ParamTypes.Size);
		// Perform recursive deep copy.
		for (int ParamIndex = 0; ParamIndex < SrcType->FuncPtr.ParamTypes.Size; ParamIndex++)
		{
			Vector_Push(New->FuncPtr.ParamTypes, struct TypeSignature*,
				AllocTypeSignatureCopy(Vector_GetValueAt(SrcType->FuncPtr.ParamTypes, struct TypeSignature*, ParamIndex)));
		}
	}

	if (SrcType->ArraySizes.Size > 0)
	{
		New->ArraySizes = Vector_Copy(&SrcType->ArraySizes);
	}

	return New;
}

static inline void FreeTypeSignature(struct TypeSignature* TypeSig)
{
	if (TypeSig == NULL) return;

	if (TypeSig->IsFunctionPointer)
	{
		for (int ParamIndex = 0; ParamIndex < TypeSig->FuncPtr.ParamTypes.Size; ParamIndex++)
		{
			FreeTypeSignature(Vector_GetValueAt(TypeSig->FuncPtr.ParamTypes, struct TypeSignature*, ParamIndex));
		}
		Vector_Destroy(&TypeSig->FuncPtr.ParamTypes);
	}

	Vector_Destroy(&TypeSig->ArraySizes);
}

// Returns a newly-allocated type signature from the specified primitive DATATYPE enum value.
static inline struct TypeSignature* AllocPrimitiveTypeSignature(enum DATATYPE PrimitiveType)
{
	struct TypeSignature* Def = AllocTypeSignature();
	ASSERT(Def != NULL);

	switch (PrimitiveType)
	{
	case DATATYPE_VOID:
		*Def = GetPrimitiveTypeSignature_Void();
		break;
	case DATATYPE_CHAR:
		*Def = GetPrimitiveTypeSignature_Char();
		break;
	case DATATYPE_SHORT:
		*Def = GetPrimitiveTypeSignature_Short();
		break;
	case DATATYPE_INT32:
		*Def = GetPrimitiveTypeSignature_Int32();
		break;
	case DATATYPE_INT64:
		*Def = GetPrimitiveTypeSignature_Int64();
		break;
	case DATATYPE_FLOAT:
		*Def = GetPrimitiveTypeSignature_Float();
		break;
	case DATATYPE_DOUBLE:
		*Def = GetPrimitiveTypeSignature_Double();
		break;
	default:
		ASSERT_MSG(0, "Attempted to get a primitive type signature for a non-primitive DATATYPE value.");
		break;
	}

	return Def;
}

// Returns whether the passed type signature is a primitive value of the specified type.
static inline ui8 TypeSignature_IsPrimitive(const struct TypeSignature* TypeSig, enum DATATYPE PrimitiveType)
{
	ASSERT(TypeSig != NULL);
	ASSERT(PrimitiveType != DATATYPE_USER_DEFINED);
	return TypeSig->Type == PrimitiveType && TypeSig->PointerLevel == 0 && !TypeSig->IsFunctionPointer;
}

static inline ui8 TypeSignature_IsVoid(const struct TypeSignature* TypeSig)
{
	ASSERT(TypeSig != NULL);
	return TypeSignature_IsPrimitive(TypeSig, DATATYPE_VOID);
}

static inline ui8 TypeSignature_IsInteger(const struct TypeSignature* TypeSig)
{
	ASSERT(TypeSig != NULL);

	return !TypeSig->IsFunctionPointer && TypeSig->PointerLevel == 0
		&& (TypeSig->Type == DATATYPE_CHAR
			|| TypeSig->Type == DATATYPE_SHORT
			|| TypeSig->Type == DATATYPE_INT32
			|| TypeSig->Type == DATATYPE_INT64);
}

static inline ui8 TypeSignature_IsNumeric(const struct TypeSignature* TypeSig)
{
	ASSERT(TypeSig != NULL);
	return TypeSignature_IsInteger(TypeSig)
		|| TypeSignature_IsPrimitive(TypeSig, DATATYPE_FLOAT)
		|| TypeSignature_IsPrimitive(TypeSig, DATATYPE_DOUBLE);
}

static inline ui8 TypeSignature_GetNumericRank(const struct TypeSignature* TypeSig)
{
	ASSERT(TypeSig != NULL);

	if (TypeSignature_IsPrimitive(TypeSig, DATATYPE_CHAR)) return 1;
	if (TypeSignature_IsPrimitive(TypeSig, DATATYPE_SHORT)) return 2;
	if (TypeSignature_IsPrimitive(TypeSig, DATATYPE_INT32)) return 3;
	if (TypeSignature_IsPrimitive(TypeSig, DATATYPE_INT64)) return 4;
	if (TypeSignature_IsPrimitive(TypeSig, DATATYPE_FLOAT)) return 5;
	if (TypeSignature_IsPrimitive(TypeSig, DATATYPE_DOUBLE)) return 6;
	return 0;
}

// Returns a human-readable name for a datatype: its specified type name for USER_DEFINED types (struct/union/enum/typedef), or a fixed string for primitive types.
static inline const char* TypeSignature_GetName(const struct TypeSignature* TypeSig)
{
	if (TypeSig == NULL)
	{
		return "?";
	}

	if (TypeSig->Type == DATATYPE_USER_DEFINED)
	{
		return TypeSig->TypeName.Length == 0 ? "<anonymous>" : TypeSig->TypeName.Str;
	}

	switch (TypeSig->Type)
	{
	default:
	case DATATYPE_UNKNOWN: return "?";
	case DATATYPE_VOID: return "void";
	case DATATYPE_CHAR: return "char";
	case DATATYPE_SHORT: return "short";
	case DATATYPE_INT32: return "int";
	case DATATYPE_INT64: return "long";
	case DATATYPE_FLOAT: return "float";
	case DATATYPE_DOUBLE: return "double";
	}
}

// Returns whether the two type signatures are equal / equivalent.
// "Equivalent" in this case means that the types are exactly the same memory size and interpretation. This is NOT a compatibility test, IE two pointers
// of different types or levels will NOT be considered equivalent.
// DecayArray will make arrays of any dimension count be considered an extra pointer level instead.
static inline ui8 TypeSignaturesEquivalent(const struct TypeSignature* A, const struct TypeSignature* B, ui8 DecayArray)
{
	ASSERT(A != NULL && B != NULL);

	if (A == B) return 1;

	ui8 PointerLevelA = A->PointerLevel + (A->ArraySizes.Size > 0 && DecayArray);
	ui8 PointerLevelB = B->PointerLevel + (B->ArraySizes.Size > 0 && DecayArray);

	// Basic properties check
	ui8 Equivalent = A->Type == B->Type
		&& A->Flags == B->Flags
		&& PointerLevelA == PointerLevelB 
		&& A->Size == B->Size
		&& A->IsFunctionPointer == B->IsFunctionPointer;

	// Type name check, if relevant.
	if (Equivalent && (A->TypeName.Length > 0 || B->TypeName.Length > 0))
	{
		Equivalent = A->TypeName.Length == B->TypeName.Length && (strcmp(A->TypeName.Str, B->TypeName.Str) == 0);
	}

	// Function pointer properties check, if relevant.
	if (Equivalent && (A->IsFunctionPointer || B->IsFunctionPointer))
	{
		Equivalent = A->IsFunctionPointer && B->IsFunctionPointer
			&& (A->FuncPtr.PointerLevel == B->FuncPtr.PointerLevel);

		// Compare parameters.
		if (Equivalent && (A->FuncPtr.ParamTypes.Size > 0 || B->FuncPtr.ParamTypes.Size > 0))
		{
			Equivalent = A->FuncPtr.ParamTypes.Size == B->FuncPtr.ParamTypes.Size;
			for (int ParamIndex = 0; Equivalent && ParamIndex < A->FuncPtr.ParamTypes.Size; ParamIndex++) 
			{
				const struct TypeSignature* AParamType = Vector_GetValueAt(A->FuncPtr.ParamTypes, struct TypeSignature*, ParamIndex);
				const struct TypeSignature* BParamType = Vector_GetValueAt(B->FuncPtr.ParamTypes, struct TypeSignature*, ParamIndex);
				Equivalent = TypeSignaturesEquivalent(AParamType, BParamType, 1);
			}
		}
	}

	// Check array sizes if not decayed.
	if (!DecayArray)
		if (A->ArraySizes.Size != B->ArraySizes.Size)
			return 0;
		else
			for (int i = 0; i < A->ArraySizes.Size; i++)
			{
				i64 ArraySizeA = Vector_GetValueAt(A->ArraySizes, i64, i);
				i64 ArraySizeB = Vector_GetValueAt(B->ArraySizes, i64, i);

				if (ArraySizeA != ArraySizeB) return 0;
			}

	return Equivalent;
}

#endif // TYPE_SIGNATURE_INCLUDED
