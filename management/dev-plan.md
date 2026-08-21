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

## Step 4 — Tokenizer (Complete) [DONE]

- Extend the tokenizer to cover the full C token set:
  - All core C operators [DONE]
  - All core C keywords [DONE]
  - All C symbols / punctuators [DONE]
  - Identifiers [DONE]
  - Literal strings [DONE]
  - Literal chars [DONE]
  - Literal numbers (int) [DONE]
- Route tokenizer failures (invalid characters, unterminated literals, etc.) through the error handling pipeline. [DONE]

## Step 5 — Parser [WIP]

- Takes any token buffer as input.
- Outputs a syntax tree representing the grammatical structure of the program.
- AST node structures (AST_NODE_TYPE, DatatypeDef, AST_Node) defined in include/compiler.h. [DONE]
- Parser process skeleton: ParserProcess structure, Parser_Error, Parser_Run entry point, wired into Compiler_Run. [DONE]
- Grammar / parsing logic to actually build the AST from tokens. [WIP]
  - Datatype parsing (primitive types, specifiers, pointer levels). [DONE]
  - Top-level dispatch loop (function / struct / global variable) wired into Parser_Run. [DONE]
  - Function parsing (identifier, parameters, body). [DONE]
  - Statement parsing (block, if/while, for, expressions, return/break/continue with break/continue resolved to their enclosing loop and return resolved to its enclosing function). [DONE]
  - Switch statement parsing.
  - Goto statement parsing.
  - Local variable declaration parsing (inside statement blocks).
  - Global variable parsing. [DONE]
  - Array & Array Access operator parsing.
  - Struct parsing.
  - Expression parsing. [WIP]
    - Function call expressionables.

## Step 6 — Symbolizer

- Takes any syntax tree as input.
- Resolves symbols to determine:
  - Memory size
  - Offsets
  - Validity of usage (scope, type compatibility, etc.)

---

## Backlog

- Tokenizer: Literal numbers — float and double parsing.
- Error handling: Associate errors with their exact file, line and column, and print a snippet of the source line to show the error in context.
- Compiler: Handle multiple input files.
- Expression parsing: Complete `Symbol_GetOpParseRules` for all operators and handle right-associativity (e.g. `=` and compound assignments) in `HandleOperatorPrecedence`. [DONE]
- Expression parsing: Support unary `-` (now de-ambiguated from `SYMBOL_OP_AMB_MINUS` via `Symbol_IsLeftUnaryOp`, so `-a` parses correctly). [DONE]

---

*More steps to come.*
