#include "tokenizer.h"
#include <stdarg.h>
// Main implementation file for the Tokenizer stage.

ui8 ParseKeyword(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
ui8 ParseIdentifier(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
ui8 ParseSymbol(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
ui8 ParseLiteralNumber(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
ui8 ParseLiteralString(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
ui8 ParseComment(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);

void Tokenizer_Error(struct TokenizerProcess* Tokenizer, ui32 BufferLoc, const char* MsgFormat, ...);
void Tokenizer_Run(struct TokenizerProcess* Tokenizer)
{
	ASSERT(Tokenizer != NULL);
	ASSERT(Tokenizer->SourceBuffer != NULL);
	ASSERT(Tokenizer->Tokens != NULL);

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
		if (	ParseComment(Tokenizer, &SourceReader)
			||	ParseKeyword(Tokenizer, &SourceReader)
			||	ParseIdentifier(Tokenizer, &SourceReader)
			||	ParseSymbol(Tokenizer, &SourceReader)
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

struct KeywordToStringPair
{
	enum TOKEN_KEYWORD Keyword;
	const char* String;
};

// Case-sensitive matching table for keyword token values and their source string equivalent.
struct KeywordToStringPair KEYWORD_TO_STRING_TABLE[] =
{
	{ KEYWORD_VOID, "void" },
	{ KEYWORD_CHAR, "char" },
	{ KEYWORD_SHORT, "short" },
	{ KEYWORD_INT, "int" },
	{ KEYWORD_FLOAT, "float" },
	{ KEYWORD_LONG, "long" },
	{ KEYWORD_DOUBLE, "double" },

	{ KEYWORD_STATIC, "static" },
	{ KEYWORD_UNSIGNED, "unsigned" },
	{ KEYWORD_STRUCT, "struct" },
	{ KEYWORD_ENUM, "enum" },
	{ KEYWORD_UNION, "union" },
	{ KEYWORD_VOLATILE, "volatile" },

	{ KEYWORD_IF, "if" },
	{ KEYWORD_ELSE, "else" },
	{ KEYWORD_FOR, "for" },
	{ KEYWORD_DO, "do" },
	{ KEYWORD_WHILE, "while" },
	{ KEYWORD_SWITCH, "switch" },
	{ KEYWORD_CASE, "case" },
	{ KEYWORD_BREAK, "break" },
	{ KEYWORD_CONTINUE, "continue" },
	{ KEYWORD_RETURN, "return" },

	{ KEYWORD_SIZEOF, "sizeof" },
};

ui8 ParseKeyword(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	struct CharBufferReader_ANSI SourceReader = OpenNestedBufferReader_ANSI(EntryReader);

	static const int TABLE_SIZE = sizeof(KEYWORD_TO_STRING_TABLE) / sizeof(struct KeywordToStringPair);

	// Read next word and see if it matches a keyword exactly.
	int KeywordLoc = SourceReader._CurrentOffset;
	char KeywordBuffer[64];
	memset(KeywordBuffer, 0, sizeof(KeywordBuffer));
	CharBufferReader_ReadNextWord(&SourceReader, KeywordBuffer, sizeof(KeywordBuffer) - 1);

	for (int KeywordStringPairIndex = 0; KeywordStringPairIndex < TABLE_SIZE; KeywordStringPairIndex++)
	{
		const struct KeywordToStringPair* Pair = KEYWORD_TO_STRING_TABLE + KeywordStringPairIndex;

		if (strcmp(KeywordBuffer, Pair->String) == 0)
		{
			// Found keyword. Output token to Tokenizer and return.
			struct Token NewToken = { 0 };
			NewToken.Type = TOKEN_KEYWORD;
			NewToken.BufferLocation = KeywordLoc;
			NewToken.Val.Keyword = Pair->Keyword;

			Vector_PushPtr(Tokenizer->Tokens, &NewToken);

			CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 1);
			return 1;
		}
	}

	// No matches in the keywords table - parsing unsuccessful.
	CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 0);
	return 0;
}

ui8 ParseIdentifier(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	struct CharBufferReader_ANSI SourceReader = OpenNestedBufferReader_ANSI(EntryReader);

	// First ensure the first character of the identifier isn't a number. Then read in the next word as our identifier.
	char FirstChar = CharBufferReader_PeekNext(&SourceReader);
	if (FirstChar >= '0' && FirstChar <= '9')
	{
		// First character of identifier would be a number which is not allowed. Fail now.
		CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 0);
		return 0;
	}

	char IdentifierBuffer[IDENTIFIER_MAX_LENGTH]; // Setting a limit to 128 characters for any identifier which should be enough.
	memset(IdentifierBuffer, 0, sizeof(IdentifierBuffer));

	ui64 BufferLocation = SourceReader._CurrentOffset;
	i32 WordLength = CharBufferReader_ReadNextWord(&SourceReader, IdentifierBuffer, sizeof(IdentifierBuffer) - 1); // Leave room for the zero-terminator.
	if (WordLength > 0)
	{
		// Create new Identifier token.

		struct Token NewToken = { 0 };
		NewToken.Type = TOKEN_IDENTIFIER;
		NewToken.BufferLocation = BufferLocation;
		NewToken.Val.Identifier = (char*)malloc(WordLength + 1);
		
		memcpy(NewToken.Val.Identifier, IdentifierBuffer, WordLength);
		NewToken.Val.Identifier[WordLength] = '\0';

		Vector_PushPtr(Tokenizer->Tokens, &NewToken);

		CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 1);
		return 1;
	}
	else
	{
		// Next characters didn't form a word.
		CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 0);
		return 0;
	}
}

ui8 ParseSymbol(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	struct CharBufferReader_ANSI SourceReader = OpenNestedBufferReader_ANSI(EntryReader);

	// Look for any symbol available in the symbol table.
	
	return 0;
}

ui8 ParseLiteralNumber(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	return 0;
}

ui8 ParseLiteralString(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
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


