
#ifndef ABSTRACT_SYNTAX_TREES_INCLUDED
#define ABSTRACT_SYNTAX_TREES_INCLUDED

#include "core.h"
#include "tokens.h" // TODO: Create a separate "operator" type from token symbols so this doesn't directly depend on tokens.
#include "type_signature.h"

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

inline ui8 Keyword_IsPrimitiveType(enum TOKEN_KEYWORD Keyword)
{
	return Keyword >= KEYWORD_VOID && Keyword <= KEYWORD_DOUBLE;
}

inline ui8 Keyword_IsTypeSpecifier(enum TOKEN_KEYWORD Keyword)
{
	return Keyword >= KEYWORD_STATIC && Keyword <= KEYWORD_VOLATILE;
}

// All possible values for the type of an AST Node.
enum AST_NODE_TYPE
{
	AST_NODE_OBJ_VAR,				// Global, Local, Structure or Param Variable. Covers values, pointers and function pointers.
	AST_NODE_OBJ_FUNC,				// Function definition or declaration.
	AST_NODE_OBJ_STRUCT,			// Structure / Union definition or declaration.
	AST_NODE_OBJ_ENUM,				// Enumeration definition or declaration.

	AST_NODE_EXPRESSION,			// Expression with or without a compile-time result located inside instructions and variable definitions.
	AST_NODE_STATEMENT_EXP,			// A single statement node executing an expression tree.
	AST_NODE_STATEMENT_CONTROL,		// A single statement executing a flow control keyword (return, break, continue...)
	AST_NODE_STATEMENT_BLOCK,		// Container statement for other statements.
	AST_NODE_STATEMENT_IF,			// Non-looping condition statement executing the next statement only if a condition expression returns > 0, or an else statement if specified.
	AST_NODE_STATEMENT_WHILE,		// Looping condition statement executing the next statement only if a condition expression returns > 0 and attempting re-entry.
	AST_NODE_STATEMENT_FOR,			// Looping condition similar to WHILE with specific Init and Post-Loop expression statements.
	AST_NODE_STATEMENT_OBJ_DEC,		// Declares one or more variable symbols associated with a specific base type.
};

struct Expression;

// Node composing an Abstract Syntax Tree.
struct AST_Node
{
	enum AST_NODE_TYPE Type;
	ui32 BufferLocation; // Source buffer location associated with this node.
	// TODO: Consider making this a full-blown token pointer / copy of the "main token" behind this node instead,
	// so as much information as possible can be kept.

	union
	{
		// Specific structure for Object types (variables, functions & function pointers, structs / unions and enums).
		// Sometimes contained independently from an AST Node Tree, to express a specific type / symbol.
		struct	
		{	
			struct TypeSignature* TypeSignature; // Contains the object's signature in terms of what base type it "produces", IE the stored type of variables and the return type of functions.
			struct String_ANSI Name; // Symbol name for this object. During integration this is what is evaluated to determine if two objects refer to the same thing (on top of shadowing).
			ui8 IsTypedef; // Indicates this object is a template for other objects to base themselves on at declaration.

			struct
			{
				ui8 InitIsInitializerList; // If set, then must be initialized with Initializer List.
				union
				{
					struct Expression* Expression; // Initializer expression. Corresponds to bit count override if variable is a struct member.
					struct Vector List; // List of expressions corresponding to array members if array, otherwise struct members.
				} Initializer;

				struct Vector ArraySizes; // Vector type = AST_Node*. Sequence of array size expressions. If empty, this variable isn't an array.
			} Var;

			struct
			{
				struct AST_Node* StatementsBlock; // Root instruction block if this is a function definition.

				// Vector of sub-objects, representing param variables.
				struct Vector Params; // Vector type = AST_Node*.
			} Func;

			// Struct declaration / definition.
			struct
			{
				struct Vector Members;		// Vector type = AST_Node*. Sub-structures Variable Objects.
				ui8 IsUnion : 1;			// Whether this structure acts as a union or collection of its members.
			} Struct;

			// Enum declaration / definition.
			// Contains a set of expression nodes which declare symbols associated to a specific number automatically (vector index) or 
			// to whatever they are assigned to.
			struct
			{
				struct Vector Members; // Vector type = Expression*. Each expression must be a single VAR_ACCESS or an assignment with a VAR ACCESS as left operand.
			} Enum;

		} Obj;

		// Root type for any statement found inside functions.
		struct
		{
			struct AST_Node* Parent; // Parent node / "scoping" node. NULL = Global scope.
			union
			{
				struct
				{
					struct AST_Node* EntryCondition; // Expression node that should resolve to > 0 for initial entry into the block.

					struct AST_Node* ExecStatement; // Statement to be executed on successful entry.
					struct AST_Node* ExecStatement_Else;	// Statement to be executed on entry failure.
				} If;

				struct
				{
					struct AST_Node* EntryCondition; // If non-NULL, expression node that should resolve to > 0 for initial entry into the block.
					struct AST_Node* LoopCondition; // Expression node that should resolve to > 0 for re-entry.

					struct AST_Node* ExecStatement; // Statement executed on each loop.
				} While;

				struct
				{
					struct AST_Node* InitExpression; // Initial expression statement to be ran regardless before initial entry is attempted.
					struct AST_Node* LoopCondition; // Expression statement that should resolve to > 0 for initial and repeated entry.
					struct AST_Node* PostLoopExpression; // Expression statement to be executed after each loop before re-entry is attempted.

					struct AST_Node* ExecStatement; // Statement executed on each loop.
				} For;

				// Container for an indefinite amount of sub-instructions.
				struct
				{
					struct Vector Statements; // Sub-instructions contained in the block, in order of declaration.
				} Block;

				// "Flow Control" statement affecting the program's execution flow (goto, return, break, continue...).
				// Links a keyword to a sub-expression (if relevant).
				struct
				{
					enum TOKEN_KEYWORD Keyword;
					struct AST_Node* Expression;
				} Control;

				// Container for a set of objects being declared in the context of a block.
				struct
				{
					struct Vector Objects; // Vector type AST_Node*. Collection of Object nodes.
				} ObjectDeclaration;

				struct AST_Node* Expression; // Free-standing expression to be executed.
			};
		} Statement;

		struct Expression* Expression;
	};
};

// Recursively prints the contents of a AST_Node into standard output.
void PrintNode(struct AST_Node* Node, ui32 Depth);

#endif // ABSTRACT_SYNTAX_TREES_INCLUDED