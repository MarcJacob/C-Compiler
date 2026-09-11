// Main implementation file for function symbol & instructions integration.

#include "integrator.h"

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


