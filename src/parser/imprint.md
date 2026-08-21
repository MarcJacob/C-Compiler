# Structure

This folder contains the full implementation of the Parser Process, which turns a vector of Tokens into a vector of root AST_Node pointers.

The main entry point is Parser_Run in parser.c. Don't hesitate to split the implementation into multiple .c files, all included from parser.c directly.

expression\_parsing.c contains all the expression parsing code, and has a single entrypoint with the ParseExpressionNode function.

# Strategy & Guidelines

Avoid static / global state. If some is genuinely needed, keep it inside the ParserProcess structure and pass that around to every function — this also gives every function access to error reporting via Parser_Error, which fills in HasError / Error.Location / Error.Message and is in turn surfaced by the Compiler process.

AST_Node types and their associated data (DatatypeDef, etc.) are defined centrally in include/compiler.h, not here.

Parsing strategy is "recursive descent", trying / pre-determining various branching paths using a simple function format:
	- Cache Token Index of ParserProcess
	- On error / non-error failure ("Denial"), return 0 / NULL (depending on root parsing vs sub-node parsing) and switch ParserProcess back to starting index.
	- On success, return 1 or new Node and add where relevant (root of new tree for a global symbol, or to an existing node).

Every function prototype / definition (if short) exists at the top of the file. Implementations are below Parser_Run entrypoint.

Global symbol parsing functions (global variables, functions, structured type...) start with ParseGlobal_ prefix.

Some parsing sub-functions can return an error, which should be indicated in its comment. If that happens, the caller must return up the callstack and return 0 / NULL ASAP.

On error, just call Parser_Error and provide the best buffer location possible. If an error is already present, it will NOT be overwritten.

# Intentions

It is likely the ParseGlobal\_ functions will be moved to their own files in the future as their get more complex.

There is a strong possibility locations for errors and nodes themselves will be replaced by token pointers instead of direct source buffer location, so less information is lost. However the decision must then be made on whether they store a pointer to token (and trust the associated memory doesn't get freed) or whether they store a full deep copy.
