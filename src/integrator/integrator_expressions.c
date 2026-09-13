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

// Wraps the passed expression inside a new Cast expression, casting it to the target type.
// The types are assumed to be compatible. TODO: Emit warning when reducing bit size.
// If the expression's return type and the target type are equivalent, nothing happens.
void WrapExpressionInCast(struct Expression* Expression, struct TypeSignature* TargetType)
{
	ASSERT(Expression != NULL);
	ASSERT(TargetType != NULL);

	if (TypeSignaturesEquivalent(Expression->ResultType, TargetType)) return;

	// Move the expression to a new spot in memory and replace the previous spot with the cast expression. That way, anything that pointed to it will automatically point to the cast instead.
	struct Expression* NewPtr = AllocExpression();
	ASSERT(NewPtr != NULL);
	*NewPtr = *Expression;

	memset(Expression, 0, sizeof(struct Expression));
	Expression->Type = EXP_OP_CAST;
	Expression->BufferLocation = NewPtr->BufferLocation;
	Expression->ResultType = TargetType;
	Expression->Cast.Operand = NewPtr;
}

// Checks the passed expression's result type against the target type for compatibility in the context of a cast.
// The expression's type is NOT resolved if it maps to a typedef.
// IsExplicit set at 1 prevents implicit conversion warnings from being emitted.
// IsExplicit set at 0 has the function wrap the expression in a cast expression to the target type, if needed.
// Returns compatibility / equivalence between the types. If 1, it is safe to wrap the expression in a cast to the target type.
ui8 EnsureExpressionCompatibility(struct IntegratorProcess* Integrator, struct TypeSignature* TargetType, struct Expression* Expression, ui8 IsExplicit)
{
	ASSERT(TargetType != NULL);
	ASSERT(Expression != NULL);

	struct TypeSignature* SourceType = Expression->ResultType;

	// If either type is void, reject immediately as they should never be involved in any cast operation.
	if (TypeSignature_IsVoid(TargetType) || TypeSignature_IsVoid(SourceType)) return 0;

	// If the types are straight-up equivalent, no further operations are necessary.
	if (TypeSignaturesEquivalent(TargetType, SourceType)) return 1;

	// If the two types are pointer types, simply check if they have the same level.
	// If not, consider them incompatible. TODO: Warning system, output a warning about indirection level and accept.

	if (TargetType->PointerLevel > 0 && SourceType->PointerLevel > 0)
	{
		if (TargetType->PointerLevel == SourceType->PointerLevel)
		{
			if (!IsExplicit)
			{
				// TODO: Emit warning if non-explicit and not casting from void-pointer.
				WrapExpressionInCast(Expression, TargetType);
			}
			return 1;
		}
		Integrator_Error(Integrator, Expression->BufferLocation, "Cannot implicitly convert between pointer types of different indirection levels.");
		return 0;
	}

	// If one of the two types is a function pointer then they must be strictly equivalent.
	// They've already been checked for equivalence, so reaching this point has to be a failure case.
	if (TargetType->IsFunctionPointer || SourceType->IsFunctionPointer)
	{
		Integrator_Error(Integrator, Expression->BufferLocation, "Cannot implicitly convert function pointer of type '%s' to type '%s'.",
			TypeSignature_GetName(SourceType), TypeSignature_GetName(TargetType));
		return 0;
	}

	// We're left with non-equivalent value types, leaving usual numeric conversions.
	if (TargetType->Type == DATATYPE_USER_DEFINED || SourceType->Type == DATATYPE_USER_DEFINED)
	{
		Integrator_Error(Integrator, Expression->BufferLocation, "Cannot implicitly convert value of type '%s' to type '%s'.",
			TypeSignature_GetName(SourceType), TypeSignature_GetName(TargetType));
		return 0;
	}

	// Primitive conversions. TODO: Emit warning when going to lower-sized type.

	// Integer <-> Integer.
	if (TypeSignature_IsInteger(SourceType) && TypeSignature_IsInteger(TargetType))
	{
		if (!IsExplicit)
		{
			if (SourceType->Size > TargetType->Size)
			{
				// TODO: Emit warning if non-explicit.
			}
			WrapExpressionInCast(Expression, TargetType);
		}
		return 1;
	}

	// Integer <-> Float
	if (TypeSignature_IsInteger(SourceType) && TypeSignature_IsPrimitive(TargetType, DATATYPE_FLOAT)
		|| TypeSignature_IsInteger(TargetType) && TypeSignature_IsPrimitive(SourceType, DATATYPE_FLOAT))
	{
		if (!IsExplicit)
		{
			if (SourceType->Size > TargetType->Size)
			{
				// TODO: Emit warning if non-explicit.
			}
			WrapExpressionInCast(Expression, TargetType);
		}
		return 1;
	}

	// Integer <-> Double
	if (TypeSignature_IsInteger(SourceType) && TypeSignature_IsPrimitive(TargetType, DATATYPE_DOUBLE)
		|| TypeSignature_IsInteger(TargetType) && TypeSignature_IsPrimitive(SourceType, DATATYPE_DOUBLE))
	{
		if (!IsExplicit)
		{
			if (SourceType->Size < TargetType->Size)
			{
				// TODO: Emit warning if non-explicit.
			}
			WrapExpressionInCast(Expression, TargetType);
		}
		return 1;
	}

	// Double <-> Float
	if (TypeSignature_IsPrimitive(SourceType, DATATYPE_FLOAT) && TypeSignature_IsPrimitive(TargetType, DATATYPE_DOUBLE)
		|| TypeSignature_IsPrimitive(TargetType, DATATYPE_DOUBLE) && TypeSignature_IsPrimitive(SourceType, DATATYPE_FLOAT))
	{
		if (!IsExplicit)
		{
			if (SourceType->Size < TargetType->Size)
			{
				// TODO: Emit warning if non-explicit.
			}
			WrapExpressionInCast(Expression, TargetType);
		}
		return 1;
	}

	Integrator_Error(Integrator, Expression->BufferLocation, "Invalid implicit conversion from %s to %s.",
		TypeSignature_GetName(SourceType), TypeSignature_GetName(TargetType));
	return 0;
}

// Ensure that the passed operator expression (specifically one of the access operators) is valid and resolves its final type,
// and integrates its right operand with correct scoping logic if required.
// The left operand must already be integrated !
ui8 IntegrateAccessOpExpression(struct IntegratorProcess* Integrator, struct Expression* AccessOpExpression)
{
	ASSERT(AccessOpExpression != NULL);

	enum TOKEN_SYMBOL AccessOp = AccessOpExpression->Op.OperatorSymbol;
	ASSERT(AccessOp == SYMBOL_OP_ARRAY_ACCESS || AccessOp == SYMBOL_OP_STRUCT_ACCESS || AccessOp == SYMBOL_OP_STRUCT_DEREF);

	// Handle array access operator.
	if (AccessOp == SYMBOL_OP_ARRAY_ACCESS)
	{
		// Right operand is the access index, and must be an integer.
		// Left operand is the accessed array, which must be a pointer or specifically a variable with array size(s).
		// TODO: Handle multi-dimensional arrays. We can't right now because we'd end up with a complex array access expression tree,
		// when we should really just have the array access operator just contain the whole chain.
		// We could read through the whole tree here and re-constitute that chain but I think it's cleaner to do so in the Parser.
		// Check right operand.
		if (!TypeSignature_IsInteger(AccessOpExpression->Op.RightOperand->ResultType))
		{
			Integrator_Error(Integrator, AccessOpExpression->BufferLocation, "Array access index must be an integer.");
			return 0;
		}

		// By default the array access just returns the same type as whatever the left expression returns.
		AccessOpExpression->ResultType = AllocTypeSignatureCopy(AccessOpExpression->Op.LeftOperand->ResultType);

		// Check left operand. It must either be a pointer value or a variable in scope which is itself a pointer or an array.
		if (AccessOpExpression->Op.LeftOperand->ResultType->PointerLevel == 0)
		{
			// Not a pointer value expression / pointer variable.
			if (AccessOpExpression->Op.LeftOperand->Type != EXP_VAR_ACCESS)
			{
				Integrator_Error(Integrator, AccessOpExpression->Op.LeftOperand->BufferLocation, "Expected pointer value or array variable.");
				return 0;
			}

			struct ProgramSymbol* VarSymbol = AccessOpExpression->Op.LeftOperand->Variable.Symbol;
			ASSERT(VarSymbol != NULL);

			if (VarSymbol->Variable.ArraySizes.Size == 0)
			{
				Integrator_Error(Integrator, AccessOpExpression->Op.LeftOperand->BufferLocation, 
					"Variable '%s' is not an array or pointer.", VarSymbol->Name.Str);
				return 0;
			}

			// Result Type stays the variable's.
		}
		else // Left operand is a pointer.
		{
			// Array access is used as deref with offset, so the result type has to lose a pointer level.
			AccessOpExpression->ResultType->PointerLevel--;
		}

		// Array access checks passed.
		return 1;
	}

	// Handle struct access operators.
	// For the operands to be valid, the left operand must be a structured type and defined.
	// The right operand must then be integrated within the structured type's scope *exclusively*.
}

// Returns whether the result of an expression is an lvalue, an actual place in stack or heap memory which can be assigned a value.
ui8 EnsureExpressionResultAssignability(struct IntegratorProcess* Integrator, struct Expression* Expression)
{
	ASSERT(Expression);

	// To be assignable, an expression must be an access to a non-array variable,
	// an array access operator, or a dereference operator.
	// Check recursively on the right operand of struct member access operators.
	// In all case the underlying type must also be non-const.

	if (Expression->ResultType->Flags & TYPE_IS_CONST)
	{
		Integrator_Error(Integrator, Expression->BufferLocation, "Cannot assign to const value.");
		return 0;
	}

	// If assigning to a variable directly, then it must not be an array.
	if (Expression->Type == EXP_VAR_ACCESS)
	{
		struct ProgramSymbol* VarSymbol = Expression->Variable.Symbol;
		ASSERT(VarSymbol != NULL);

		if (VarSymbol->Variable.ArraySizes.Size > 0)
		{
			Integrator_Error(Integrator, Expression->BufferLocation, "Cannot assign to array variable. Specify which index to assign to.");
			return 0;
		}

		return 1;
	}

	if (Expression->Type == EXP_OP)
	{
		if (Expression->Op.OperatorSymbol == SYMBOL_OP_STRUCT_ACCESS
			|| Expression->Op.OperatorSymbol == SYMBOL_OP_STRUCT_DEREF)
		{
			return EnsureExpressionResultAssignability(Integrator, Expression->Op.RightOperand);
		}

		// Always allow assignment to deref or array access operator result, as in the circumstance of an
		// assignment they are interpreted as offset address lookups.
		return Expression->Op.OperatorSymbol == SYMBOL_OP_DEREF
			|| Expression->Op.OperatorSymbol == SYMBOL_OP_ARRAY_ACCESS;
	}

	return 0;
}

// Resolves type compatibility between the operands of a binary operator expression.
// Numeric operands are promoted to their common type. Assignment operators use the
// left operand's type as their target. Comparison and logical operators produce int.
// The function also resolves implicit casts through EnsureExpressionCompatibility().
ui8 EnsureOpExpressionOperandTypesCompatibility(struct IntegratorProcess* Integrator, struct Expression* OpExpression)
{
	ASSERT(OpExpression != NULL);
	ASSERT(OpExpression->Op.LeftOperand != NULL && OpExpression->Op.RightOperand != NULL);

	struct Expression* LeftOperand = OpExpression->Op.LeftOperand;
	struct Expression* RightOperand = OpExpression->Op.RightOperand;
	struct TypeSignature* LeftType = LeftOperand->ResultType;
	struct TypeSignature* RightType = RightOperand->ResultType;
	ASSERT(LeftType != NULL && RightType != NULL);

	// Handle assignment operators. The left type must not be void, and the right type must be equivalent or compatible.
	if (Symbol_IsAssignmentOp(OpExpression->Op.OperatorSymbol))
	{
		if (!EnsureExpressionResultAssignability(Integrator, LeftOperand))
			return 0;

		if (!EnsureExpressionCompatibility(Integrator, LeftType, RightOperand, 0)) 
			return 0;

		OpExpression->ResultType = LeftType;
		return 1;
	}

	// The comma operator evaluates to its right operand and does not require
	// the two operand types to be compatible.
	if (OpExpression->Op.OperatorSymbol == SYMBOL_OP_COMMA)
	{
		OpExpression->ResultType = RightType;
		return 1;
	}

	// Handle equivalent types. 
	// A comparison or logical operator between them always returns a in32 value (0 or 1).
	// Other operators use the type of the left operand.
	if (TypeSignaturesEquivalent(LeftType, RightType))
	{
		OpExpression->ResultType = Symbol_IsComparisonOp(OpExpression->Op.OperatorSymbol)
			|| Symbol_IsLogicalOp(OpExpression->Op.OperatorSymbol)
			? AllocPrimitiveTypeSignature(DATATYPE_INT32)
			: LeftType;
		return 1;
	}

	// Comparison between pointers. The pointer types do not need to be cast-compatible
	// since we'll just be comparing the raw adresses (TODO: Still log a warning if they are not compatible).
	if (Symbol_IsComparisonOp(OpExpression->Op.OperatorSymbol)
		&& LeftType->PointerLevel > 0 && RightType->PointerLevel > 0)
	{
		OpExpression->ResultType = AllocPrimitiveTypeSignature(DATATYPE_INT32);
		return 1;
	}

	// At this point we've dealt with pointers, assignments and comparators / logical operators.
	// Any remaining valid scenario needs the types to both be numerical.
	if (!TypeSignature_IsNumeric(LeftType) || !TypeSignature_IsNumeric(RightType))
	{
		Integrator_Error(Integrator, OpExpression->BufferLocation,
			"Incompatible operand types '%s' and '%s' for operator '%s'.",
			TypeSignature_GetName(LeftType), TypeSignature_GetName(RightType),
			Symbol_ToString(OpExpression->Op.OperatorSymbol));
		return 0;
	}

	// Ensure both operands are integers when dealing with an Integer operator.
	if (Symbol_IsIntegerOp(OpExpression->Op.OperatorSymbol)
		&& (!TypeSignature_IsInteger(LeftType) || !TypeSignature_IsInteger(RightType)))
	{
		Integrator_Error(Integrator, OpExpression->BufferLocation,
			"Operator '%s' requires integral operands.",
			Symbol_ToString(OpExpression->Op.OperatorSymbol));
		return 0;
	}

	// From here we need to determine which operand's type will take priority.
	// Check the numeric type ranks, use the higher ranked one, ensure compatibility.
	struct TypeSignature* CommonType = TypeSignature_GetNumericRank(LeftType) >= TypeSignature_GetNumericRank(RightType)
		? LeftType
		: RightType;

	if (!EnsureExpressionCompatibility(Integrator, CommonType, LeftOperand, 0)) return 0;
	if (!EnsureExpressionCompatibility(Integrator, CommonType, RightOperand, 0)) return 0;

	// Finally, determine result type: int32 for comparison / logical like above, or the common type we just determined.
	OpExpression->ResultType = Symbol_IsComparisonOp(OpExpression->Op.OperatorSymbol)
		|| Symbol_IsLogicalOp(OpExpression->Op.OperatorSymbol)
		? AllocPrimitiveTypeSignature(DATATYPE_INT32)
		: CommonType;
	return 1;
}

// Returns whether a unary operator's operand is valid for it and resolves its result type.
ui8 EnsureUnaryOpExpressionValidity(struct IntegratorProcess* Integrator, struct Expression* UnaryOpExpression)
{
	Integrator_Error(Integrator, UnaryOpExpression->BufferLocation, "Unary op expression integration not implemented.");
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

		// If operator is an access operator, first integrate left operand as normal,
		// then go into the access operand validity logic which will take care of integrating the right
		// operand correctly in case it needs special scoping logic.
		if (Expression->Op.OperatorSymbol == SYMBOL_OP_ARRAY_ACCESS
			|| Expression->Op.OperatorSymbol == SYMBOL_OP_STRUCT_ACCESS
			|| Expression->Op.OperatorSymbol == SYMBOL_OP_STRUCT_DEREF)
		{
			ASSERT(Expression->Op.LeftOperand != NULL);
			if ((Expression->Op.LeftOperand = IntegrateExpression(Integrator, Scope, Expression->Op.LeftOperand)) == NULL)
				return NULL;

			if (!IntegrateAccessOpExpression(Integrator, Expression)) return NULL;
		}
		else
		{
			// Integrate operand expressions.
			if (Expression->Op.LeftOperand != NULL)
				if ((Expression->Op.LeftOperand = IntegrateExpression(Integrator, Scope, Expression->Op.LeftOperand)) == NULL)
					return NULL;
			if (Expression->Op.RightOperand != NULL)
				if ((Expression->Op.RightOperand = IntegrateExpression(Integrator, Scope, Expression->Op.RightOperand)) == NULL)
					return NULL;

			// If operator is binary, resolve their mutual compatibility.
			if (Expression->Op.LeftOperand != NULL && Expression->Op.RightOperand != NULL)
			{
				// Function will pick the expression's result type if the operands are compatible.
				if (!EnsureOpExpressionOperandTypesCompatibility(Integrator, Expression)) return NULL;
			}
			else
			{
				// Otherwise resolve the operator expression's type as a unary operator.
				if (!EnsureUnaryOpExpressionValidity(Integrator, Expression)) return NULL;
			}
		}
		break;
	case EXP_OP_CAST:
		// Integrate operand expression, then check for type compatibility between target type signature and operand's.
		if ((Expression->Cast.Operand = IntegrateExpression(Integrator, Scope, Expression->Cast.Operand)) == NULL)
			return NULL;
		
		// Ensure the explicit cast works.
		if (!EnsureExpressionCompatibility(Integrator, Expression->ResultType, Expression->Cast.Operand, 1))
			return NULL;
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

		Expression->Variable.Symbol = Symbol;
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

			if (!EnsureExpressionCompatibility(Integrator, ParamTargetType, ParamExpression, 0))
			{
				Integrator_Error(Integrator, ParamExpression->BufferLocation, "Invalid Function call parameter: Cannot implicitly convert from type '%s' to '%s'.",
					TypeSignature_GetName(ParamExpression->ResultType), TypeSignature_GetName(ParamTargetType));
				return NULL;
			}
		}
		break;
	}

	return Expression;
}
