This folder contains the full implementation of the Parser Process, which turns a vector of Tokens into a vector of root AST_Node pointers.

The main entry point is Parser_Run in parser.c. Don't hesitate to split the implementation into multiple .c files, all included from parser.c directly.

Avoid static / global state. If some is genuinely needed, keep it inside the ParserProcess structure and pass that around to every function — this also gives every function access to error reporting via Parser_Error, which fills in HasError / Error.Location / Error.Message and is in turn surfaced by the Compiler process.

AST_Node types and their associated data (DatatypeDef, etc.) are defined centrally in include/compiler.h, not here.

Parsing strategy (recursive descent vs. other approaches, how expression operator precedence is handled) is not yet decided.
