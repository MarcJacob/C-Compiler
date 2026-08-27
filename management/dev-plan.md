# Dev Plan

## Step 1 — Code Structure

- Set up core `include/` folder with foundational shared headers. DONE
- Create `main.c` as the program entry point and sole compilation unit. DONE

## Step 2 — Tokenizer (Basic) + File Reader

- File reader: loads a source file from disk into memory as a character buffer + simple ANSI string library. DONE
- Vector: simple vector library to support growable arrays to contain tokens and later the syntax tree. DONE
- Tokenizer: takes any character buffer as input, outputs a token buffer. [DONE]

## Step 3 — Error Handling Pipeline

- Central mechanism for any compiler stage to report an error (with source location where applicable).
- Collects/formats reported errors and surfaces them as a console message. DONE
- Non-assert: distinct from `ASSERT` (which guards internal invariants) — this handles expected failure cases (bad input, invalid source).

## Step 4 — Tokenizer [DONE]

- Extend the tokenizer to cover the full C token set:
  - All core C operators. [DONE]
  - All core C keywords. [DONE]
  - All C symbols / punctuators. [DONE]
  - Identifiers. [DONE]
  - Literal strings. [DONE]
  - Literal chars. [DONE]
  - Literal numbers (int). [DONE]
  - Literal numbers (float and double).
  - `extern` keyword [DONE]
  - `typedef` keyword. [DONE]
  - `sizeof` keyword. [DONE]
  - `?` symbol (for the ternary operator). [DONE]
- Route tokenizer failures (invalid characters, unterminated literals, etc.) through the error handling pipeline. [DONE]

## Step 5 — Parser [WIP]

- Takes any token buffer as input. [DONE]
- Outputs a syntax tree representing the grammatical structure of the program. [DONE]
- AST node structures (AST_NODE_TYPE, DatatypeDef, AST_Node) defined in include/compiler.h. [DONE]
- Parser process skeleton: ParserProcess structure, Parser_Error, Parser_Run entry point, wired into Compiler_Run. [DONE]
- Grammar / parsing logic to actually build the AST from tokens. [WIP]
  - Datatype parsing (primitive types, specifiers, pointer levels). [DONE]
  - Top-level dispatch loop (function / struct / global variable) wired into Parser_Run. [DONE]
  - Function parsing (identifier, parameters, body). [DONE]
  - Statement parsing (block, if/while, for, expressions, return/break/continue). [DONE]
  - Local variable declaration parsing (inside statement blocks). [DONE]
  - Global variable parsing. [DONE]
  - Array & Array Access operator parsing. [DONE]
  - Struct parsing. [WIP]
    - Named & anonymous struct declaration/definition via the shared datatype-prefix parser, including forward declarations. [DONE]
    - Inline variable declaration(s) following a struct body, including pointer levels. [DONE]
    - Bitfield members (e.g. `int x : 4;`).
  - Union parsing. [DONE]
  - Enum parsing. [DONE]
  - Typedef parsing. [DONE]
  - Expression parsing. [WIP]
    - Function call expressionables. [DONE]
    - `sizeof` operator. [DONE]
    - Cast expressions (`(type)expr`). [DONE]
    - Ternary operator `?:`. [DONE]
    - Initializer lists for arrays/structs (e.g. `int a[3] = {1,2,3};`). [WIP]
  - Parenthesized/function-pointer declarators (e.g. `int (*fp)(int,int);`), unified with function/variable declarator parsing via a shared `ParseDeclarator`/`ParseDeclarators` mechanism. [DONE]
  - Switch statement parsing.
  - Goto statement parsing.
  - `do`/`while` loop parsing.
  - Expression / AST Node decoupling. 
    - Make sure Expression Trees do not contain nodes and that nodes contain expressions directly when possible.
    - Done so that Expression structures can be reused in other stages.

## Step 6 — Integrator [PLANNING WIP]

- Takes a set of Abstract Syntax Trees as input.
- Outputs an Integrated Program Tree as output.
- Resolves symbols to determine:
  - Memory size
  - Offsets
  - Validity of usage (scope, type compatibility, etc.)
- Links together control statements with where they should end up (return, break, continue, goto...)
- Re-structures functions into parameters, local variables, and instructions (with local variables being associated to a specific instruction for its scope and specific line).
    - Support for sub-scopes and shadowing.

## Step 7 - Code Generator

- Takes an Integrated Program Tree as input.
- Turns the tree into a set of program initialization and function instructions in a chosen assembly langage (probably NASM or Intel x64).

## Step 8 - Assembler & Linker (Placeholder)

- Use existing assembler & linker for now. Project ends when the compiler can feed correct Assembly into that.

## Step 9 - Preprocessor

- Abstract the concept of source for tokenizer.
    - Any kind of source needs to be able to return "positional info" given a buffer location.
- Preprocessor takes sources as input and outputs Preprocessed string buffers (with range-based with origin source conversion table to convert buffer locations.)
- #define = textual substitution, creates a range where all created characters share the same location as the define.
    - Macros: Same idea, handling parameters (that are themselves first developped if defined).
- #if / #ifdef / #else / #endif: modifies a stateful "stack" of ifs determining if encountered characters are kept / affect preprocessor.
- #include: includes the contents of another file (and triggers preprocessing on it), creates a range with an offset to lookup into the included file.

---

## Backlog (To be added to existing or new steps later).

- Error handling: Associate errors with their exact file, line and column, and print a snippet of the source line to show the error in context.
- Handle multiple input files.
- Preprocessor.
- Parser: Rework declarator parsing so that functions can be DEFINED anywhere a full symbol declaration declarator is allowed, even inline with a struct / enum def or a multi variable declaration (insanity).
- Parser: More flexible specifier keyword order + check compatibility (like forbidding "static extern").

---

 