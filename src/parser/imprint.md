IMPRINT \<Root>/src/parser/

Stamp 2026-08-24 | (GIT SHA 9cbefe956748e22c2cb260faa37356b45a8fdfb2) | Marc Jacob

# Structure

This folder contains the full implementation of the Parser Process, which turns a vector of Tokens into a vector of root AST_Node pointers.

The main entry point is Parser_Run in parser.c. Don't hesitate to split the implementation into multiple .c files, all included from parser.c directly.

parser\_expressions.c contains all the expression parsing code, and has a single entry point with the ParseExpressionNode function.

parser\_statements.c contains all the statement parsing code, and exposes all of its non-static functions as entry points.

parser\_structs_enums.c contains all the structure parsing code, and exposes a single entry point with ParseGlobal_Struct_Union_Enum function.

parser\_logging.c contains all the logging code for debugging the contents of a parsed AST to stdout, and exposes a single entry point with the Parser_PrintTree function.
	- Mostly authored by AI.

# What is the parser ?

The parser is the process taking in a sequence of tokens as input and giving back one or more Abstract Syntax Trees as output.

The policy of the parser when it comes to catching errors is "*Could* it work in this local context ?".

If ever something tries to get parsed in a way that couldn't *possibly* work, it may error. This involves "local" checks:
	- Expression format, not allowing two binary operators next to one another, not allowing unterminated parenthesis scopes...
	- Expecting function definitions to terminate their block before the file ends or the next function begins.

It is *not* supposed to check for overall program consistency. This means some specific error scenarios should remain undetected by it:
	- Undeclared symbol usage (the parser is not aware of previously declared symbols or what type they resolve to).
	- Break statements outside a for or while or switch statement (parser is not usually aware of the immediate context and never aware of parent contexts / blocks).
	- Invalid assignments (parser is not aware of what type functions or expressions or variables resolve to, or the compatibility of even primitive types among one another).

The point of the parser is to produce syntax trees that can easily be *integrated*, which is specifically the step that will check whether the program DOES work in the global context.

# Strategy & Guidelines

Avoid static / global state. If some is genuinely needed, keep it inside the ParserProcess structure and pass that around to every function — this also gives every function access to error reporting via Parser_Error, which fills in HasError / Error.Location / Error.Message and is in turn surfaced by the Compiler process.

AST_Node types and their associated data (DatatypeDef, etc.) are defined centrally in include/compiler.h, not here.

Parsing strategy is "recursive descent", trying / pre-determining various branching paths using a simple function format:
	- Cache Token Index of ParserProcess
	- On error / non-error failure ("Denial"), return 0 / NULL (depending on root parsing vs sub-node parsing) and switch ParserProcess back to starting index.
	- On success, return 1 or new Node and add where relevant (root of new tree for a global symbol, or to an existing node).

Some parsing sub-functions can return an error, which should be indicated in its comment. If that happens, the caller must return up the callstack and return 0 / NULL ASAP.

On error, just call Parser_Error and provide the best buffer location possible. If an error is already present, it will NOT be overwritten.

Parsing follows a tree structure, starting from parsing a datatype definition followed by an object declarator. If that fails, fall back on structure, union, enum or typedef parsing.

# Intentions

There is a strong possibility locations for errors and nodes themselves will be replaced by token pointers instead of direct source buffer location, so less information is lost. However the decision must then be made on whether they store a pointer to token (and trust the associated memory doesn't get freed) or whether they store a full deep copy.

It is likely the parser will be broken down into more files for every major node category (objs, structures / unions / enums, expressions, statements, typedefs) by the time the parser is complete.
