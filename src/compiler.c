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
			// TODO: String-based implementation + Add header to error message to indicate stage & location.
			Compiler->ErrorCode_Global = COMPILER_TOKENIZER_STAGE_ERROR;
			Compiler->ErrorCode_Stage = Tokenizer.Error.Code;
			memcpy(Compiler->ErrorMsg, Tokenizer.Error.Message, sizeof(Tokenizer.Error.Message));
			return;
		}
		else
		{
			// Print all the parsed tokens.
			for (int TokenIndex = 0; TokenIndex < Tokens.Size; TokenIndex++)
			{
				struct Token* Tok = Vector_GetPtr(&Tokens, TokenIndex);

				switch (Tok->Type)
				{
				case TOKEN_COMMENT:
					printf("<COMMENT>\n");
					break;
				case TOKEN_IDENTIFIER:
					printf("<IDENTIFIER: '%s'>\n", Tok->Val.Identifier);
					break;
				case TOKEN_KEYWORD:
				{
					static const int KeywordTableSize = sizeof(KEYWORD_TO_STRING_TABLE) / sizeof(struct KeywordToStringPair);
					for (int KeywordIndex = 0; KeywordIndex < KeywordTableSize; KeywordIndex++)
					{
						if (KEYWORD_TO_STRING_TABLE[KeywordIndex].Keyword == Tok->Val.Keyword)
						{
							printf("<KEYWORD: '%s'>\n", KEYWORD_TO_STRING_TABLE[KeywordIndex].String);
							break;
						}
					}
					break;
				}
				case TOKEN_SYMBOL:
				{
					static const int SymbolTableSize = sizeof(SYMBOL_TO_STRING_TABLE) / sizeof(struct SymbolToStringPair);
					for (int SymbolIndex = 0; SymbolIndex < SymbolTableSize; SymbolIndex++)
					{
						if (SYMBOL_TO_STRING_TABLE[SymbolIndex].Symbol == Tok->Val.Symbol)
						{
							printf("<SYMBOL: '%s'>\n", SYMBOL_TO_STRING_TABLE[SymbolIndex].String);
							break;
						}
					}
					break;
				}
				case TOKEN_LITERAL_STRING:
					printf("<LITERAL_STRING: \"%s\">\n", Tok->Val.LiteralString);
					break;
				case TOKEN_LITERAL_CHAR:
					printf("<LITERAL_CHAR: '%c'>\n", Tok->Val.LiteralCharacter);
					break;
				case TOKEN_LITERAL_NUMBER_INT:
					printf("<LITERAL_NUMBER_INT: %lld>\n", Tok->Val.LiteralNumber.Integer);
					break;
				case TOKEN_LITERAL_NUMBER_FLOAT:
					printf("<LITERAL_NUMBER_FLOAT: %f>\n", Tok->Val.LiteralNumber.Float);
					break;
				case TOKEN_LITERAL_NUMBER_DOUBLE:
					printf("<LITERAL_NUMBER_DOUBLE: %lf>\n", Tok->Val.LiteralNumber.Double);
					break;
				}
			}
		}
	}
	
}
