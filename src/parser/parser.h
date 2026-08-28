// Core Symbols for Parser stage.

#ifndef PARSER_INCLUDED
#define PARSER_INCLUDED

#include "compiler.h"

// Main orchestration structure, passed to most parser functions to provide "global" functionality such as error reporting.
struct ParserProcess
{
	// Input
	struct Vector* SourceTokens; // Vector type = struct Token
	ui32 TokenIndex; // Index of next token to be read.

	// Output
	struct Vector* RootNodes; // Vector type = struct AST_Node*. Main output.

	ui8 HasError; // Whether the Parser is currently in an error state.
	struct
	{
		ui32 Location; // Index of character where error happened, if applicable.
		struct String_ANSI Message;
	} Error;
};

// Sets the HasError flag on the Parser process and fills in the error message.
// From there on the Parser Process should finish as soon as possible.
void Parser_Error(struct ParserProcess* Parser, ui32 BufferLoc, const char* MsgFormat, ...);

struct Token* Parser_PeekToken(struct ParserProcess* Parser);
struct Token* Parser_ConsumeToken(struct ParserProcess* Parser);
ui32 Parser_GetLastTokenBufferLoc(struct ParserProcess* Parser);

struct AST_Node* AllocNewASTNode(enum AST_NODE_TYPE NodeType);
void FreeASTNode(struct AST_Node* Node);
void FreeNodeVector(struct Vector* NodeVec);

// Prints every root AST node tree held by the Parser process to stdout, using indentation to represent node hierarchy.
void Parser_PrintTree(struct ParserProcess* Parser);

#define POINTER_SIZE (_WIN64 ? 8 : 4)

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Void() 
{
	struct DatatypeDef Def = { 0 };
	Def.Size = 0;
	Def.Type = DATATYPE_VOID;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Char() 
{
	struct DatatypeDef Def = { 0 };
	Def.Size = 1;
	Def.Type = DATATYPE_CHAR;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Short()
{
	struct DatatypeDef Def = { 0 };
	Def.Size = 2;
	Def.Type = DATATYPE_SHORT;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Int32() 
{
	struct DatatypeDef Def = { 0 };
	Def.Size = 4;
	Def.Type = DATATYPE_INT32;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Int64() 
{
	struct DatatypeDef Def = { 0 };
	Def.Size = 8;
	Def.Type = DATATYPE_INT64;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Float() 

{
	struct DatatypeDef Def = { 0 };
	Def.Size = 4;
	Def.Type = DATATYPE_FLOAT;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_Double() 
{
	struct DatatypeDef Def = { 0 };
	Def.Size = 8;
	Def.Type = DATATYPE_DOUBLE;
	return Def;
}

static inline struct DatatypeDef GetPrimitiveDatatypeDef_String() 
{
	struct DatatypeDef Def = { 0 };
	Def.PointerLevel = 1;
	Def.Size = POINTER_SIZE;
	Def.Type = DATATYPE_CHAR;
	Def.Flags = DATATYPE_IS_CONST;
	return Def;
}

// Attempts to parse the next few tokens into a DatatypeDef structure.
static ui8 ParseDatatypeDef(struct ParserProcess* Parser, struct DatatypeDef* OutDatatypeDef);

// Parses an expression node containing all operations and sub-expressions between current token and the next instance of the specified end symbol.
// If NULL is returned, then the parser has encountered an error or failed to read any expressionable tokens.
// ConsumeStopCharacter determines whether the end token is consumed (comma, semicolon, closing parenthesis...).
struct AST_Node* ParseExpressionASTNode(struct ParserProcess* Parser, ui8 StopAtComma, ui8 ConsumeStopCharacter);

// Frees an expression node / tree.
// NOTE: Migrate this to a proper "Expressions" file independent from parser.
void FreeExpression(struct Expression* Expression);

// Specialized parser specifically made to parse an array access operator expressionable (missing its left operand).
// Expects the first token to be an opening bracket. Stops at the first corresponding closing bracket found and consumes it.
// The resulting operator expression node will require a left operand as the thing being array-accessed.
struct Expression* ParseExpressionable_ArrayAccess(struct ParserProcess* Parser);

// Root statement parsing function, used when parsing a function definition.
struct AST_Node* ParseBlockStatementNode(struct ParserProcess* Parser);
struct AST_Node* ParseStatementNode(struct ParserProcess* Parser);

// Gathers all statement nodes directly or indirectly contained inside the given root statement node into the provided Out vector.
void GetAllStatements(struct AST_Node* RootStatement, struct Vector* Out);

// Parses a single variable or function object node of the given return type. 
// The allowed initializers type parameters allow triggering an error when encountering a disallowed type.
struct AST_Node* ParseObject_VarFunc(struct ParserProcess* Parser, struct DatatypeDef* ReturnType, ui8 AllowEmpty, ui8 AllowInitializer, ui8 AllowBitCount);

// Root parsing function, used to parse the start of ASTs.
// Parses objects until a "break" is reached, usually a semicolon.
ui8 ParseNextRootObjects(struct ParserProcess* Parser);

#endif // PARSER_INCLUDED
