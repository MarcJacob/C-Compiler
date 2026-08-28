
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

	ui16 Size; // Total size in bytes. Resolved during integration except for primitive types.
	struct String_ANSI TypeName; // String representation of the type / actual type name for USER_DEFINED types.

	ui8 PointerLevel; // How many dereferences are required to reach the base data. If this is a function pointer, applies to the return value.

	ui8 IsFunctionPointer; // Is this a function pointer ?
	struct
	{
		ui8 PointerLevel; // How many dereferences are required to reach function (callable when == 1, always > 0).
		struct Vector ParamTypes; // Vector type = TypeSignature*. Type signature of parameters.
	} FuncPtr;
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
}

#endif // TYPE_SIGNATURE_INCLUDED
