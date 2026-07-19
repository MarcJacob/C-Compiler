# Dev Plan

## Step 1 — Code Structure

- Set up core `include/` folder with foundational shared headers. DONE
- Create `main.c` as the program entry point and sole compilation unit. DONE

## Step 2 — Tokenizer (Basic) + File Reader

- File reader: loads a source file from disk into memory as a character buffer + simple ANSI string library. DONE
- Vector: simple vector library to support growable arrays to contain tokens and later the syntax tree. DONE
- Tokenizer: takes any character buffer as input, outputs a token buffer.

## Step 3 — Error Handling Pipeline

- Central mechanism for any compiler stage to report an error (with source location where applicable).
- Collects/formats reported errors and surfaces them as a console message. DONE
- Non-assert: distinct from `ASSERT` (which guards internal invariants) — this handles expected failure cases (bad input, invalid source).

## Step 4 — Tokenizer (Complete)

- Extend the tokenizer to cover the full C token set:
  - All core C operators
  - All core C keywords
  - All C symbols / punctuators
  - Identifiers
- Route tokenizer failures (invalid characters, unterminated literals, etc.) through the error handling pipeline.

## Step 5 — Parser

- Takes any token buffer as input.
- Outputs a syntax tree representing the grammatical structure of the program.

## Step 6 — Symbolizer

- Takes any syntax tree as input.
- Resolves symbols to determine:
  - Memory size
  - Offsets
  - Validity of usage (scope, type compatibility, etc.)

---

*More steps to come.*
