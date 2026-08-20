// Main compiler orchestration implementation file.

#include "compiler.h"

// Include stage implementations. This also of course includes the stages' specific symbols which is intended.
#include "tokenizer/tokenizer.c"
#include "parser/parser.c"
// ...

void Compiler_Run(struct CompilerProcess* Compiler)
{
	ASSERT(Compiler != NULL);

	if (Compiler->InputFiles.Size == 0)
	{
		// No inputs. Return immediately with a general error message.
		Compiler->ErrorCode_Global = COMPILER_GENERAL_ERROR;
		Compiler->ErrorMsg = String_Create_ANSI("No input files specified.");
		return;
	}

	// TODO: Support using more input files.

	// Stage 1 - Tokenization.
	struct Vector Tokens = Vector_Create(struct Token, 128);
	{
		struct CharBuffer_ANSI* SourceBuffer = Vector_GetValueAt(Compiler->InputFiles, struct CharBuffer_ANSI*, 0);

		struct TokenizerProcess Tokenizer = { 0 };
		Tokenizer.SourceBuffer = SourceBuffer;
		Tokenizer.Tokens = &Tokens;
		Tokenizer_Run(&Tokenizer);

		// Handle Tokenizer Error if any.
		if (Tokenizer.HasError)
		{
			Compiler->ErrorCode_Global = COMPILER_TOKENIZER_STAGE_ERROR;

			// TODO: Figure out line & col of error instead of raw buffer location + pass filenames to compiler instead of just the source buffers themselves.
			Compiler->ErrorMsg = String_CreateFormat_ANSI("TOKENIZER ERROR (%s, Loc = %d) > %s", "<SRC FILENAME>", Tokenizer.Error.Location, Tokenizer.Error.Message.Str);
			return;
		}
		else
		{
			Tokenizer_PrintTokens(&Tokenizer);
		}
	}

	// Stage 2 - Parser.

	struct Vector ParsedTreeRoots = Vector_Create(struct AST_Node*, 8);
	{
		struct ParserProcess Parser = { 0 };
		Parser.SourceTokens = &Tokens;
		Parser.RootNodes = &ParsedTreeRoots;

		Parser_Run(&Parser);

		// Handle error, if any.
		if (Parser.HasError)
		{
			Compiler->ErrorCode_Global = COMPILER_PARSER_STAGE_ERROR;

			// TODO: Figure out line & col of error instead of raw buffer location + pass filenames to compiler instead of just the source buffers themselves.
			Compiler->ErrorMsg = String_CreateFormat_ANSI("PARSER ERROR (%s, Loc = %d) > %s", "<SRC FILENAME>", Parser.Error.Location, Parser.Error.Message.Str);
			return;
		}
		else
		{
			Parser_PrintTree(&Parser);
		}
	}
	
}
