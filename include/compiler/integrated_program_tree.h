
#ifndef INTEGRATED_PROGRAM_TREE_INCLUDED
#define INTEGRATED_PROGRAM_TREE_INCLUDE

#include "core.h"

struct ProgramSymbol;
struct SymbolScope
{
	struct Vector Symbols; // Vector type = ProgramSymbol* Contains all symbols of this scope in order of declaration.
};

// Enumerates the possible type of any given ProgramSymbol within a ProgramTree.
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

struct ProgramSymbol
{
	enum SYMBOL_TYPE Type;
	struct String_ANSI Name;

	union
	{
		struct
		{
			struct TypeSignature* DeclarationType; // Type signature this variable was resolved to have.
			struct Vector ArraySizes; // Vector type = ui64. Array sizes specified.

			ui64 BitSize; // Size of the variable in bits (so it supports bit count specifier).
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
			ui8 IsUnion; // If set, the structure's size will equal the largest member's, and all members will have an offset of 0.

			struct SymbolScope Scope; // Contains VARIABLE symbols.
			ui64 Size; // Total size of the structure including any alignment / padding concerns.
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
			struct ProgramSymbol* BaseSymbol; // Base Typedef / Struct / Union symbol of this typedef if any.
			struct TypeSignature* Type; // Signature for this Typedef, applied as is or "additively" with the Base ProgramSymbol.
		} Typedef;
	};
};

// Root of IST (Integrated Program Tree) containing the top-level scope.
struct IntegratedProgramTree
{
	struct SymbolScope* RootScope;
};

#endif // INTEGRATED_PROGRAM_TREE_INCLUDE