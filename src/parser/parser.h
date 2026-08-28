// Core Symbols for Parser stage.

#ifndef PARSER_INCLUDED
#define PARSER_INCLUDED

#include "compiler/abstract_syntax_trees.h"
#include "compiler/expressions.h"

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

struct AST_Node* AllocASTNode(enum AST_NODE_TYPE NodeType);
void FreeASTNode(struct AST_Node* Node);
void FreeASTNodeVector(struct Vector* NodeVec);

// Prints every root AST node tree held by the Parser process to stdout, using indentation to represent node hierarchy.
void Parser_PrintTree(struct ParserProcess* Parser);

// Attempts to parse the "base" of a TypeSignature with the next few tokens.
static ui8 ParseTypeSignature(struct ParserProcess* Parser, struct TypeSignature* OutDatatypeDef);

// Parses an expression tree.
// If NULL is returned, then the parser has encountered an error. When no expression can be parsed until next end character,
// will return NOP expression.
// ConsumeStopCharacter determines whether the end token is consumed (comma, semicolon, closing parenthesis...).
struct Expression* ParseRootExpression(struct ParserProcess* Parser, ui8 StopAtComma, ui8 ConsumeStopChar);

// Parses an expression node containing all operations and sub-expressions between current token and the next instance of the specified end symbol.
// If NULL is returned, then the parser has encountered an error.
// ConsumeStopCharacter determines whether the end token is consumed (comma, semicolon, closing parenthesis...).
struct AST_Node* ParseExpressionASTNode(struct ParserProcess* Parser, ui8 StopAtComma, ui8 ConsumeStopCharacter);

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
struct AST_Node* ParseObject_VarFunc(struct ParserProcess* Parser, struct TypeSignature* TypeSignature, ui8 AllowEmpty, ui8 AllowInitializer, ui8 AllowBitCount);

// Root parsing function, used to parse the start of ASTs.
// Parses objects until a "break" is reached, usually a semicolon.
ui8 ParseNextRootObjects(struct ParserProcess* Parser);

#endif // PARSER_INCLUDED
