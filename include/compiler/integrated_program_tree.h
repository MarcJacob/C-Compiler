
#ifndef INTEGRATED_PROGRAM_TREE_INCLUDED
#define INTEGRATED_PROGRAM_TREE_INCLUDE

#include "core.h"

// All possible types of Program Instruction.
enum INSTRUCTION_TYPE
{
	INSTRUCTION_TYPE_EXPRESSION,	// Instruction executes an expression node / tree.
	INSTRUCTION_TYPE_JUMP,			// Non-conditional jump to another instruction.
	INSTRUCTION_TYPE_COND_JUMP,		// Conditional jump to another instruction (or, optionally, another instruction if conditional is not met).

	INSTRUCTION_TYPE_RETURN,		// Return from current function, optionally featuring an expression whose value is to be returned.
};

// A reference to another instruction within the same function's Instructions vector.
// During Integration, a function's instructions are still being appended to that (growable) vector, so any
// struct ProgramInstruction* taken at that point can be invalidated by a later reallocation: Index is used instead.
// Once the owning function is fully integrated, every Index is resolved back into a Ptr for direct, easier access from then on.
union InstructionRef
{
	ui32 Index;
	struct ProgramInstruction* Ptr;
};

// Defines a single instruction in the program, either an expression or a special control flow instruction within a function / statement block.
// Contains the necessary information to know what is referenced when encountering a symbol or where to jump to.
// Usually does not map to a single assembly code instruction, but rather to a single "atomic thing the program does" which must be translated to 0 .. N assembly code instructions at code generation time.
struct ProgramInstruction
{
	enum INSTRUCTION_TYPE Type;

	struct Expression* Exp; // Expression to execute. What is done with the result after execution, if anything, depends on instruction type.

	// Control flow instruction data.
	union
	{
		struct
		{
			union InstructionRef IfNonZero; // Instruction to jump to if expression is non-zero.
			union InstructionRef IfZero; // Instruction to jump to if expression is zero.
		} ConditionalJump;

		struct
		{
			union InstructionRef JumpTarget; // Instruction to jump to regardless of expression result.
		} Jump;
	};
};

struct ProgramSymbol;
struct SymbolScope
{
	struct SymbolScope* Parent; // Parent Scope. NULL for the Global Scope.
	struct Vector Symbols; // Vector type = ProgramSymbol* Contains all symbols of this scope in order of declaration.
	struct Vector ChildScopes; // Vector type = struct SymbolScope*. Sub-scopes directly nested under this one, in order of creation.
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

			ui64 BitSize; // Size of the variable in bits (so it supports bit count specifier).
			ui64 Offset; // Memory offset for struct member variables.
			ui32 BitOffset; // When non-zero, indicates this variable has a bit count specifier within a structure. The bits are to be added to the standard byte offset.

			ui8 HasInitializer;
			union
			{
				struct Expression* InitExpression;
				struct Vector InitializerList; // Vector type = struct Expression*. Contains the initializer expressions of array or struct members.
			};

		} Variable;

		struct
		{
			struct TypeSignature* ReturnType; // Return type, part of declaration signature.
			struct Vector ParamTypeSignatures; // Vector type = struct TypeSignature*. Type Signatures of parameters in order of declaration, part of declaration signature.


			struct SymbolScope* Scope;	// Contains VARIABLE symbols, specifically parameters and top-level local variables in order of declaration. Filled in only for defined functions.
										// More local variables may exist inside sub-scopes.

			struct Vector LocalVariables; // Vector type = struct ProgramSymbol*. Contains VARIABLE symbols, including parameters and ALL local variables including sub-scopes.
			struct Vector Instructions; // Vector type = struct ProgramInstruction. All instructions in source order.

		} Function;

		struct
		{
			ui8 IsUnion; // If set, the structure's size will equal the largest member's, and all members will have an offset of 0.

			struct SymbolScope* Scope; // Contains VARIABLE symbols.
			ui64 Size; // Total size of the structure including any alignment / padding concerns.
			ui32 Alignment; // Memory alignment of the structure as as whole, equal to lowest member size / alignment.
		} Struct; // Or union.

		struct
		{
			struct Vector Values; // Vector type = ProgramSymbol*. Contains pointers to ENUM_VALUE Symbols.
			ui64 UnderlyingTypeSize; // Size of the underlying integral type.
		} Enum;

		struct
		{
			i64 NumericValue; // Numeric value this enum member resolves to.
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