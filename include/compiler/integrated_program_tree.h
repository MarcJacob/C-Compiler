
#ifndef INTEGRATED_PROGRAM_TREE_INCLUDED
#define INTEGRATED_PROGRAM_TREE_INCLUDE

#include "core.h"

struct Symbol;
struct SymbolScope
{
	struct Vector Symbols; // Vector type = Symbol* Contains all symbols of this scope in order of declaration.
};

// Enumerates the possible type of any given Symbol within a IPT.
// Loosely corresponds to Object types from ASTs.
enum SYMBOL_TYPE
{
	SYMBOL_TYPE_VARIABLE,
	SYMBOL_TYPE_FUNCTION,
	SYMBOL_TYPE_STRUCT,
	SYMBOL_TYPE_UNION,
	SYMBOL_TYPE_ENUM,
	SYMBOL_TYPE_ENUM_VAL,
	SYMBOL_TYPE_TYPEDEF
};

struct Symbol
{
	enum SYMBOL_TYPE Type;
	struct String_ANSI Name;

	ui64 Size; // Size of this symbol in memory.

	union
	{
		struct
		{
			struct Symbol* ParentScope; // Parent scope of this variable.
			struct TypeSignature* DeclarationType; // Type signature this variable was resolved to have.

			ui64 Offset; // Memory offset (function / structure var) or Address offset (global var) of this variable.
		} Variable;

		struct
		{
			ui64 ProgAddress; // Program-relative address of this function.

			struct SymbolScope Scope; // Contains VARIABLE symbols.

			// ... TODO: Instructions graph.

		} Function;

		struct
		{
			struct SymbolScope Scope; // Contains VARIABLE symbols.
		} Struct; // Or union.

		struct
		{
			struct SymbolScope Values; // Contains ENUM_VALUE symbols.
		} Enum;

		struct
		{
			ui64 NumericValue; // Numeric value this enum member resolves to.
		} Enum_Member;

		struct
		{
			struct Symbol* BaseSymbol; // Base Typedef / Struct / Union symbol of this typedef if any.
			struct TypeSignature* Type; // Signature for this Typedef, applied as is or "additively" with the Base Symbol.
		} Typedef;
	};
};

// Root of IST (Integrated Program Tree) containing the top-level scope.
struct IntegratedProgramTree
{
	struct SymbolScope* RootScope;
};

#endif // INTEGRATED_PROGRAM_TREE_INCLUDE