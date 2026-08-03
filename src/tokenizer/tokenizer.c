#include "tokenizer.h"
#include <stdarg.h>
// Main implementation file for the Tokenizer stage.

ui8 ParseKeyword(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
ui8 ParseIdentifier(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
ui8 ParseSymbol(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
ui8 ParseComment(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);

void Tokenizer_Error(struct TokenizerProcess* Tokenizer, ui32 BufferLoc, const char* MsgFormat, ...);
void Tokenizer_Run(struct TokenizerProcess* Tokenizer)
{
	ASSERT(Tokenizer != NULL);
	ASSERT(Tokenizer->SourceBuffer != NULL);

	// Create root reader, then start the top-level iteration process.

	struct CharBufferReader_ANSI SourceReader = CreateBufferReader_ANSI(Tokenizer->SourceBuffer);

	char NextSourceChar = CharBufferReader_PeekNext(&SourceReader);
	while (NextSourceChar != EOF)
	{
		// Discard newlines and whitespaces which are meaningless at this point.
		while (NextSourceChar == ' ' || NextSourceChar == '\n')
		{
			CharBufferReader_ReadNext(&SourceReader);
			NextSourceChar = CharBufferReader_PeekNext(&SourceReader);
			if (NextSourceChar == EOF) goto TOKENIZER_END;
		}

		// Parsing order. If any function succeeds, it is a signal that at least one token has been added and / or that some characters were parsed.
		if (	ParseKeyword(Tokenizer, &SourceReader)
			||	ParseIdentifier(Tokenizer, &SourceReader)
			||	ParseSymbol(Tokenizer, &SourceReader)
			||	ParseComment(Tokenizer, &SourceReader)
			)
		{
			// Successful parse.
			NextSourceChar = CharBufferReader_PeekNext(&SourceReader);
		}
		else
		{
			// Failed parse.
			// If no error was output, add a generic one here.
			Tokenizer_Error(Tokenizer, SourceReader._CurrentOffset, "Tokenizer failed to parse source character '%c'.", NextSourceChar);
			break;
		}
	}

TOKENIZER_END:
	return;
}

void Tokenizer_Error(struct TokenizerProcess* Tokenizer, ui32 BufferLoc, const char* MsgFormat, ...)
{
	Tokenizer->HasError = 1;

	va_list args;
	va_start(args, MsgFormat);
	vsprintf_s(Tokenizer->Error.Message, sizeof(Tokenizer->Error.Message), MsgFormat, args);
	va_end(args);
}

ui8 ParseKeyword(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	struct CharBufferReader_ANSI SourceReader = OpenNestedBufferReader_ANSI(EntryReader);

	// Look for any keyword in the keyword table.

	return 0;
}

ui8 ParseIdentifier(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	struct CharBufferReader_ANSI SourceReader = OpenNestedBufferReader_ANSI(EntryReader);

	// Look for any valid sequence of Alphanumerics until reaching a non-alphanumeric, a whitespace or a newline.

	return 0;
}

ui8 ParseSymbol(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	struct CharBufferReader_ANSI SourceReader = OpenNestedBufferReader_ANSI(EntryReader);

	// Look for any symbol available in the symbol table.
	
	return 0;
}

ui8 ParseComment(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	struct CharBufferReader_ANSI SourceReader = OpenNestedBufferReader_ANSI(EntryReader);

	// Look for either "//" or "/*" string and parse the comment accordingly (IE Ignore it and output no tokens. Consider whether there's any reason to
	// keep Comments as tokens later).

	if (CharBufferReader_ReadNextExpected(&SourceReader, "//"))
	{
		// One-line comment - read characters until a Newline or EOF is reached.
		char NextChar;
		do
		{
			NextChar = CharBufferReader_ReadNext(&SourceReader);
		} while (NextChar != '\n' && NextChar != EOF);

		// Successfully parsed a single-line comment.
		CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 1);
		return 1;
	}
	else if (CharBufferReader_ReadNextExpected(&SourceReader, "/*"))
	{
		// Multi-line comment - read characters until the "*/" string or EOF is reached.
		char NextChar;
		do
		{
			NextChar = CharBufferReader_ReadNext(&SourceReader);

			if (NextChar == '*')
			{
				NextChar = CharBufferReader_ReadNext(&SourceReader);
				if (NextChar == '/')
				{
					break; // Multiline comment finished.
				}
			}
		} while (NextChar != EOF);

		// Successfully parsed a multi-line comment.
		CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 1);
		return 1;
	}

	CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 0);
	return 0; // Failed to parse a comment.
}


