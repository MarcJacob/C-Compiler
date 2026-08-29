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

	ui64 StaticMemSize; // Total size of static memory taken by this program.

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

#endif // INTEGRATOR_INCLUDED
