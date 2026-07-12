# Dev Plan

## Step 1 — Code Structure

- Set up core `include/` folder with foundational shared headers.
- Create `main.c` as the program entry point.

## Step 2 — Tokenizer (Basic) + File Reader

- File reader: loads a source file from disk into memory as a character buffer.
- Tokenizer: takes any character buffer as input, outputs a token buffer.

## Step 3 — Tokenizer (Complete)

- Extend the tokenizer to cover the full C token set:
  - All C operators
  - All C keywords
  - All C symbols / punctuators
  - Identifiers

## Step 4 — Parser

- Takes any token buffer as input.
- Outputs a syntax tree representing the grammatical structure of the program.

## Step 5 — Symbolizer

- Takes any syntax tree as input.
- Resolves symbols to determine:
  - Memory size
  - Offsets
  - Validity of usage (scope, type compatibility, etc.)

---

*More steps to come.*
