// Main compiler orchestration implementation file.

#include "compiler.h"

// Include stage implementations. This also of course includes the stages' specific symbols which is intended.
#include "tokenizer/tokenizer.c"
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
					printf("<IDENTIFIER: '%s'>\n", Tok->Val.Identifier.Str);
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
					printf("<LITERAL_STRING: \"%s\">\n", Tok->Val.LiteralString.Str);
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
