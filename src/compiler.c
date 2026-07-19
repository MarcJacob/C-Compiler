// Main compiler orchestration implementation file.

#include "compiler.h"

// Include stage implementations. This also of course includes the stages' specific symbols which is intended.
#include "tokenizer/tokenizer.c"
// ...

static const char NoInputFilesErrorMsg[] = "No input files specified.";

void Compiler_Run(struct CompilerProcess* Compiler)
{
	ASSERT(Compiler != NULL);
	ASSERT(Compiler->InputFiles._Mem != NULL);

	if (Compiler->InputFiles.Size == 0)
	{
		// No inputs. Return immediately with a general error message.
		Compiler->ErrorCode_Global = COMPILER_GENERAL_ERROR;
		memcpy(Compiler->ErrorMsg, NoInputFilesErrorMsg, sizeof(NoInputFilesErrorMsg));
		return;
	}

	// TODO: Support using more input files.

	// Stage 1 - Tokenization.
	struct Vector Tokens = Vector_Create(struct Token, 128);
	{
		struct TokenizerProcess Tokenizer = { 0 };
		Tokenizer.SourceBuffer = Vector_GetValueAt(Compiler->InputFiles, struct CharBuffer_ANSI*, 0);
		Tokenizer.Tokens = &Tokens;
		Tokenizer_Run(&Tokenizer);

		// Check for errors and return if anything wrong happened.
		if (Tokenizer.HasError)
		{
			// TODO: String-based implementation + Add header to error message to indicate stage.
			Compiler->ErrorCode_Global = COMPILER_TOKENIZER_STAGE_ERROR;
			Compiler->ErrorCode_Stage = Tokenizer.Error.Code;
			memcpy(Compiler->ErrorMsg, Tokenizer.Error.Message, sizeof(Tokenizer.Error.Message));
			return;
		}
	}
	
}
