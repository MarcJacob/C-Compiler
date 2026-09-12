# Integrator source folder

The Integrator is a compiler pipeline stage taking a set of Abstract Syntax Trees (AST Nodes) as input and producing an Integrated Program Tree as output.

The Integrated Program Tree is the final abstract representation of the program before code generation, with all elements being checked in placement and overall coherence against one another.

This is where non-evident syntax and structural errors are caught: missing / incomplete types, break clause outside a looping block, symbol redefinition, and so on.

Outside error-handling the goal is to combine all provided AST Nodes into a single coherent tree with each sub-node being a symbol or an instruction making up the final program with all related information immediately accessible.

The Integrator code is split into multiple implementation files which are directly included into *integrator.c*, which itself contains *Integrator_Run*, the main entry point function.

# Integration strategy

Every root AST Node, at this point, should be an Object (Variable, Function, Struct, Union, Enum or Typedef).

Each Object is converted into a matching Symbol, triggering the recursive integration of sub-symbols (struct members, function instructions, types...).
The Abstract Syntax Tree should provide enough guarantees that we don't need to really "try" anything and provide cleanup code when something fails. Instead just emit an error and return, and let the failed symbol / scope / instruction stay where it is (linked to a scope or function).

An Object has been successfully integrated once its corresponding symbol(s) has been placed inside the *Scope Tree* starting from the program's root scope.
The Scope system allows looking for a pre-integrated symbol given a name, optionally going up the hierarchy until the root scope is reached.

Most objects can be integrated as an incomplete symbol, a declaration, before their definition is found. To this end not every AST Node gets integrated as its own symbol, as it can instead contribute to an existing symbol pre-created by a preceding declaration (with all coherency checks being passed).

The Program Tree must be directly translatable into a translation unit's worth of assembly code, with the guarantee that there are no possible user input errors left.
