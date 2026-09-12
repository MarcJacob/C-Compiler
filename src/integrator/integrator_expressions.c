// Main implementation file for constant expression resolution.

#include "integrator.h"

// Resolves an operation between two expressions, recursively.
ui8 EvalConstantOpExpression(struct IntegratorProcess* Integrator, struct SymbolScope* Scope, enum TOKEN_SYMBOL Op, struct Expression* LeftOperand, struct Expression* RightOperand,
	ui64* OutResult, enum DATATYPE* OutResultType)
{
	ASSERT(OutResult != NULL);
	ASSERT(
		   Symbol_IsLeftUnaryOp(Op) && RightOperand != NULL
		|| Symbol_IsRightUnaryOp(Op) && LeftOperand != NULL
		|| Symbol_IsBinaryOp(Op) && LeftOperand != NULL && RightOperand != NULL
	);

	ui32 BufferLoc = LeftOperand != NULL ? LeftOperand->BufferLocation : RightOperand->BufferLocation;

	i64 LeftOpRes = 0, RightOpRes = 0;
	enum DATATYPE LeftOpResType, RightOpResType;
	if (LeftOperand != NULL
		&& !EvalConstantExpression(Integrator, Scope, LeftOperand, &LeftOpRes, &LeftOpResType)) return 0;
	if (RightOperand != NULL
		&& !EvalConstantExpression(Integrator, Scope, RightOperand, &RightOpRes, &RightOpResType)) return 0;

	// TODO: Check compatibility between result types & perform appropriate casts.
	// For now we just error out if we ever get anything other than an INT64.
	if (LeftOpResType != DATATYPE_INT64 || RightOpResType != DATATYPE_INT64)
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
ui8 EvalConstantExpression(struct IntegratorProcess* Integrator, struct SymbolScope* Scope, struct Expression* Expression, i64* OutResult, enum DATATYPE* OutResultType)
{
	ASSERT(Scope != NULL);
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
	case EXP_VAR_ACCESS:
	{
		struct ProgramSymbol* Symbol = Scope_FindSymbol(Scope, &Expression->Variable.Name, 1);
		if (Symbol == NULL)
		{
			Integrator_Error(Integrator, Expression->BufferLocation, "Undeclared identifier '%s' in constant expression.", Expression->Variable.Name.Str);
			return 0;
		}
		if (Symbol->Type != SYMBOL_TYPE_ENUM_VAL)
		{
			Integrator_Error(Integrator, Expression->BufferLocation, "'%s' is not usable in a constant expression.", Expression->Variable.Name.Str);
			return 0;
		}

		*OutResult = Symbol->Enum_Member.NumericValue;
		*OutResultType = DATATYPE_INT64;
		return 1;
	}
		// Invalid base cases
	case EXP_LITERAL_STRING:
	case EXP_NOP:
	case EXP_FUNC_CALL:
	default:
		Integrator_Error(Integrator, Expression->BufferLocation, "Expression must be constant integral.");
		return 0;
		// Complex cases
	case EXP_OP:
		return EvalConstantOpExpression(Integrator, Scope, Expression->Op.OperatorSymbol, Expression->Op.LeftOperand, Expression->Op.RightOperand, OutResult, OutResultType);
	}
}

// Returns whether the passed type signatures are compatible in the context of a cast from SourceType to TargetType.
ui8 TypeSignaturesCompatible(struct TypeSignature* TargetType, struct TypeSignature* SourceType)
{
	ASSERT(TargetType != NULL);
	ASSERT(SourceType != NULL);

	return 0;
}

// Wraps the passed expression inside a new Cast expression, casting it to the target type.
// The types are assumed to be compatible. TODO: Emit warning when reducing bit size.
void WrapExpressionInCast(struct Expression* Expression, struct TypeSignature* TargetType)
{
	ASSERT(Expression != NULL);
	ASSERT(TargetType != NULL);

	// Move the expression to a new spot in memory and replace the previous spot with the cast expression. That way, anything that pointed to it will automatically point to the cast instead.
	struct Expression* NewPtr = AllocExpression();
	ASSERT(NewPtr != NULL);
	*NewPtr = *Expression;

	memset(Expression, 0, sizeof(struct Expression));
	Expression->Type = EXP_OP_CAST;
	Expression->BufferLocation = NewPtr->BufferLocation;
	Expression->ResultType = TargetType;
}

// Resolves type compatibility between the operands of a binary operator expression.
// If the operand types are the same, nothing is changed. If they are implicitly compatible, one of the operands gets wrapped inside a Cast expression (TODO: Output a warning in cases where type size goes down).
// Otherwise an error is produced. Returns whether the compatibility was successfully resolved.
ui8 ResolveOpExpressionOperandTypesCompatibility(struct IntegratorProcess* Integrator, struct Expression* OpExpression)
{
	ASSERT(OpExpression != NULL);
	ASSERT(OpExpression->Op.LeftOperand != NULL && OpExpression->Op.RightOperand != NULL);

	if (TypeSignaturesEquivalent(OpExpression->Op.LeftOperand->ResultType, OpExpression->Op.RightOperand->ResultType))
	{
		OpExpression->ResultType = OpExpression->Op.LeftOperand->ResultType;
		return 1;
	}

	Integrator_Error(Integrator, OpExpression->BufferLocation, "Op expression operands type compatibility check unimplemented.");
	return 0;
}

// Goes through an expression tree recursively and resolves the expression's symbolic links and final type(s).
// Checks for symbolic integrity of the expression but does not check type compatibility.
// Will also attempt to resolve constant expressions. If successful, the integrated expression is not the passed expression itself but a new, literal one.
// Returns NULL if there was an error, or the integrated expression if successful.
struct Expression* IntegrateExpression(struct IntegratorProcess* Integrator, struct SymbolScope* Scope, struct Expression* Expression)
{
	ASSERT(Expression != NULL);

	// Before doing anything else, check for early return due to expression being a trivial type with no requirement for integration.
	switch (Expression->Type)
	{
	case EXP_LITERAL_CHAR:
	case EXP_LITERAL_INT:
	case EXP_LITERAL_FLOAT:
	case EXP_LITERAL_DOUBLE:
	case EXP_LITERAL_STRING:
	case EXP_NOP: // May be encountered in special cases, like a sizeof operand.
		return Expression;
	}

	// Attempt to simply evaluate the expression as constant. If successful we can just return a new literal expression with the result.
	ui64 ConstResult;
	enum DATATYPE ConstType;
	if (EvalConstantExpression(Integrator, Scope, Expression, &ConstResult, &ConstType))
	{
		struct TypeSignature* Type = AllocPrimitiveTypeSignature(ConstType);
		ASSERT(Type != NULL && Type->Size > 0);

		struct Expression* ConstExpression = AllocExpression();
		ConstExpression->BufferLocation = Expression->BufferLocation;

		// Determine expression literal type. We know it has to be primitive and non-void.
		// NOTE: I'm starting to think that identifying the type of literal in the expression's type 
		// when we have the Result Type to work with is a little pointless.
		switch (ConstType)
		{
		case DATATYPE_CHAR:
			ConstExpression->Type = EXP_LITERAL_CHAR;
			break;
		case DATATYPE_SHORT:
		case DATATYPE_INT32:
		case DATATYPE_INT64:
			ConstExpression->Type = EXP_LITERAL_INT;
			break;
		case DATATYPE_FLOAT:
			ConstExpression->Type = EXP_LITERAL_FLOAT;
			break;
		case DATATYPE_DOUBLE:
			ConstExpression->Type = EXP_LITERAL_DOUBLE;
			break;
		}

		ConstExpression->ResultType = AllocPrimitiveTypeSignature(ConstType);
		memcpy(&ConstExpression->Literal, &ConstResult, Type->Size);
		return ConstExpression;
	}
	else if (Integrator->HasError)
	{
		// "Catch" any error here and discard it, unless the entry expression is a sizeof which MUST be resolvable to a constant.
		if (Expression->Type == EXP_OP_SIZEOF) return NULL;

		Integrator_DiscardError(Integrator);
	}

	struct ProgramSymbol* Symbol = NULL;
	switch (Expression->Type)
	{
	case EXP_OP:
		// Integrate operand expressions.
		if (Expression->Op.LeftOperand != NULL)
			if ((Expression->Op.LeftOperand = IntegrateExpression(Integrator, Scope, Expression->Op.LeftOperand)) == NULL)
				return NULL;
		if (Expression->Op.RightOperand != NULL)
			if ((Expression->Op.RightOperand = IntegrateExpression(Integrator, Scope, Expression->Op.RightOperand)) == NULL)
				return NULL;

		// If operator is binary, resolve their mutual compatibility.
		if (Expression->Op.LeftOperand != NULL && Expression->Op.RightOperand != NULL)
			if (!ResolveOpExpressionOperandTypesCompatibility(Integrator, Expression)) return NULL;
		break;
	case EXP_OP_CAST:
		// Integrate operand expression, then check for type compatibility between target type signature and operand's.
		if ((Expression->Cast.Operand = IntegrateExpression(Integrator, Scope, Expression->Cast.Operand)) == NULL)
			return NULL;
		if (!TypeSignaturesCompatible(Expression->ResultType, Expression->Cast.Operand->ResultType))
		{
			Integrator_Error(Integrator, Expression->BufferLocation, 
				"Incompatible types for cast: '%s' <- '%s'.", 
				TypeSignature_GetName(Expression->ResultType), TypeSignature_GetName(Expression->Cast.Operand->ResultType));
			return NULL;
		}
		break;
	case EXP_VAR_ACCESS:
		// Look for the variable symbol, link the expression to it and take its declaration type as the expression's return type.

		Symbol = Scope_FindSymbol(Scope, &Expression->Variable.Name, 1);
		if (Symbol == NULL)
		{
			Integrator_Error(Integrator, Expression->BufferLocation, "Unknown symbol '%s' in expression.", Expression->Variable.Name.Str);
			return NULL;
		}
		
		if (Symbol->Type == SYMBOL_TYPE_VARIABLE)
		{
			Expression->ResultType = Symbol->Variable.DeclarationType;
		}
		else if (Symbol->Type == SYMBOL_TYPE_ENUM_VAL)
		{
			Expression->ResultType = AllocPrimitiveTypeSignature(DATATYPE_INT64); // TEMP: All enumeration values are assumed to be 64 bit integers.
		}
		else
		{
			Integrator_Error(Integrator, Expression->BufferLocation, "Invalid usage of symbol '%s' in expression.", Expression->Variable.Name.Str);
			return NULL;
		}
		break;
	case EXP_FUNC_CALL:
		// Look for function symbol and use its return type as the expression's result type.
		// Then integrate each param expression and check their type against the function symbol's.

		Symbol = Scope_FindSymbol(Scope, &Expression->FunctionCall.FunctionName, 1);
		if (Symbol == NULL)
		{
			Integrator_Error(Integrator, Expression->BufferLocation, "Unknown symbol '%s' in expression.", Expression->FunctionCall.FunctionName.Str);
			return NULL;
		}
		if (Symbol->Type != SYMBOL_TYPE_FUNCTION)
		{
			Integrator_Error(Integrator, Expression->BufferLocation, "Non-function symbol '%s' used as function in expression.", Expression->FunctionCall.FunctionName.Str);
			return NULL;
		}
		Expression->FunctionCall.Symbol = Symbol;
		Expression->ResultType = Symbol->Function.ReturnType;

		if (Expression->FunctionCall.Params.Size < Symbol->Function.ParamTypeSignatures.Size)
		{
			Integrator_Error(Integrator, Expression->BufferLocation, "Too few arguments in function call.");
			return NULL;
		}
		else if (Expression->FunctionCall.Params.Size > Symbol->Function.ParamTypeSignatures.Size)
		{
			Integrator_Error(Integrator, Expression->BufferLocation, "Too many arguments in function call.");
			return NULL;
		}

		// Integrate and check param expressions.
		for (int ParamIndex = 0; ParamIndex < Expression->FunctionCall.Params.Size; ParamIndex++)
		{
			struct Expression* ParamExpression = Vector_GetValueAt(Expression->FunctionCall.Params, struct Expression*, ParamIndex);
			ASSERT(ParamExpression != NULL);
			struct TypeSignature* ParamTargetType = Vector_GetValueAt(Symbol->Function.ParamTypeSignatures, struct TypeSignature*, ParamIndex);
			ASSERT(ParamTargetType != NULL);

			if ((ParamExpression = IntegrateExpression(Integrator, Scope, ParamExpression)) == NULL)
				return 0;

			if (!TypeSignaturesEquivalent(ParamTargetType, ParamExpression->ResultType))
			{
				if (!TypeSignaturesCompatible(ParamTargetType, ParamExpression->ResultType))
				{
					Integrator_Error(Integrator, ParamExpression->BufferLocation, "Invalid Function call parameter: Cannot implicitly convert from type '%s' to '%s'.",
						TypeSignature_GetName(ParamExpression->ResultType), TypeSignature_GetName(ParamTargetType));
					return NULL;
				}

				// Wrap the param expression in a cast to the parameter target type.
				WrapExpressionInCast(ParamExpression, ParamTargetType);
			}
		}
		break;
	}

	return Expression;
}
