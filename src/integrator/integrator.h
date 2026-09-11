// Core Symbols for Integrator stage.

#ifndef INTEGRATOR_INCLUDED
#define INTEGRATOR_INCLUDED

#include "compiler/integrated_program_tree.h"

struct IntegratorProcess
{
	// Input
	struct Vector* ASTRootNodes; // Vector type struct AST_Node*. Input Abstract Syntax Trees.

	// Output
	struct IntegratedProgramTree* ProgramTree; // Output Integrated Program Tree.

	ui8 HasError; // Whether the Integrator is currently in an error state.
	struct
	{
		ui32 Location; // Index of character where error happened, if applicable.
		struct String_ANSI Message;
	} Error;
};

// Sets the HasError flag on the Integrator Process and fills in the error message.
// From there on the Integrator Process should finish as soon as possible.
void Integrator_Error(struct IntegratorProcess* Integrator, ui32 BufferLoc, const char* MsgFormat, ...);

void PrintSymbol(struct ProgramSymbol* Symbol, ui32 Depth);

// Prints the contents of an IST to standard out.
void Integrator_PrintTree(struct IntegratorProcess* Integrator);

// Attempts to evaluate an expression as a constant expression. Outputs the value (using OutResult as an 8-bytes as memory to be correctly interpreted) and the value type.
ui8 EvalConstantExpression(struct IntegratorProcess* Integrator, struct SymbolScope* Scope, struct Expression* Expression, i64* OutResult, enum DATATYPE* OutResultType);

struct ProgramSymbol* IntegrateObj_Variable(struct IntegratorProcess* Integrator, struct AST_Node* VarASTNode, struct SymbolScope* Scope);

// Creates a new Program Symbol from an AST Object Node object and adds it to the passed scope.
struct ProgramSymbol* IntegrateASTObjectNode(struct IntegratorProcess* Integrator, struct AST_Node* ObjASTNode, struct SymbolScope* Scope);

struct ProgramSymbol* AllocSymbol(enum SYMBOL_TYPE Type);
struct SymbolScope* AllocScope(struct SymbolScope* Parent);
void FreeScope(struct SymbolScope* Scope);

void Scope_AddSymbol(struct SymbolScope* Scope, struct ProgramSymbol* Symbol);
struct ProgramSymbol* Scope_FindSymbol(const struct SymbolScope* Scope, const struct String_ANSI* Name, ui8 SearchParent);

// Allocates a new program instruction within the passed function symbol's internal instructions buffer and returns it.
struct ProgramInstruction* AllocInstruction(struct ProgramSymbol* FunctionSymbol, enum INSTRUCTION_TYPE Type);

#endif // INTEGRATOR_INCLUDED
