#include "tokenizer.h"
#include <stdarg.h>
// Main implementation file for the Tokenizer stage.

ui8 ParseKeyword(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
ui8 ParseIdentifier(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
ui8 ParseSymbol(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
ui8 ParseLiteralNumber(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
ui8 ParseLiteralString(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
ui8 ParseLiteralChar(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader);
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
		// Discard newlines, whitespaces and any other character which are meaningless at this point.
		while (NextSourceChar == ' ' || NextSourceChar == '\n' || NextSourceChar == '\t')
		{
			CharBufferReader_ReadNext(&SourceReader);
			NextSourceChar = CharBufferReader_PeekNext(&SourceReader);
			if (NextSourceChar == EOF) goto TOKENIZER_END;
		}

		// Parsing order. If any function succeeds, it is a signal that at least one token has been added and / or that some characters were parsed.
		if (	ParseComment(Tokenizer, &SourceReader)
			||	ParseLiteralString(Tokenizer, &SourceReader)
			||	ParseLiteralChar(Tokenizer, &SourceReader)
			||	ParseLiteralNumber(Tokenizer, &SourceReader)
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
			if (!Tokenizer->HasError)
			{
				Tokenizer_Error(Tokenizer, SourceReader._CurrentOffset, "Tokenizer failed to parse source character '%c'.", NextSourceChar);
			}
			break;
		}
	}

TOKENIZER_END:
	return;
}

void Tokenizer_Error(struct TokenizerProcess* Tokenizer, ui32 BufferLoc, const char* MsgFormat, ...)
{
	Tokenizer->HasError = 1;
	Tokenizer->Error.Location = BufferLoc;

	va_list args;
	va_start(args, &MsgFormat);
	Tokenizer->Error.Message = String_CreateFormatV_ANSI(MsgFormat, args);
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
};

ui8 ParseKeyword(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	struct CharBufferReader_ANSI SourceReader = OpenNestedBufferReader_ANSI(EntryReader);

	static const int TABLE_SIZE = sizeof(KEYWORD_TO_STRING_TABLE) / sizeof(struct KeywordToStringPair);

	// Read next word and see if it matches a keyword exactly.
	ui32 KeywordLoc = SourceReader._CurrentOffset;
	struct String_ANSI KeywordStr = String_Create_ANSI(NULL);
	if (CharBufferReader_ReadNextWord(&SourceReader, &KeywordStr) == 0)
	{
		// Failed to read a word.
	PARSE_FAIL:
		CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 0);
		return 0;
	}

	for (int KeywordStringPairIndex = 0; KeywordStringPairIndex < TABLE_SIZE; KeywordStringPairIndex++)
	{
		const struct KeywordToStringPair* Pair = KEYWORD_TO_STRING_TABLE + KeywordStringPairIndex;

		if (strcmp(KeywordStr.Str, Pair->String) == 0)
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
	goto PARSE_FAIL;
}

ui8 ParseIdentifier(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	struct CharBufferReader_ANSI SourceReader = OpenNestedBufferReader_ANSI(EntryReader);

	// First ensure the first character of the identifier isn't a number. Then read in the next word as our identifier.
	char FirstChar = CharBufferReader_PeekNext(&SourceReader);
	if (FirstChar >= '0' && FirstChar <= '9')
	{
		// First character of identifier would be a number which is not allowed. Fail now.
PARSE_FAIL:
		CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 0);
		return 0;
	}

	struct String_ANSI IdentifierStr = String_Create_ANSI(NULL);

	ui64 BufferLocation = SourceReader._CurrentOffset;
	i32 WordLength = CharBufferReader_ReadNextWord(&SourceReader, &IdentifierStr); // Leave room for the zero-terminator.
	if (WordLength > 0)
	{
		// Create new Identifier token.

		struct Token NewToken = { 0 };
		NewToken.Type = TOKEN_IDENTIFIER;
		NewToken.BufferLocation = BufferLocation;
		NewToken.Val.Identifier = IdentifierStr;
		
		Vector_PushPtr(Tokenizer->Tokens, &NewToken);

		CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 1);
		return 1;
	}
	else
	{
		// Next characters didn't form a word.
		goto PARSE_FAIL;
	}
}

// Longer symbols are listed before any shorter symbol they share a prefix with (eg. "--" before "-"),
// since ParseSymbol takes the first match found in this table. Table itself is defined in compiler.h
// as SYMBOL_TO_STRING_TABLE, shared with Symbol_ToString for use anywhere in the program.
ui8 ParseSymbol(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	struct CharBufferReader_ANSI SourceReader = OpenNestedBufferReader_ANSI(EntryReader);

	static const int TABLE_SIZE = sizeof(SYMBOL_TO_STRING_TABLE) / sizeof(struct SymbolToStringPair);

	int SymbolLoc = SourceReader._CurrentOffset;

	for (int SymbolToStringPairIndex = 0; SymbolToStringPairIndex < TABLE_SIZE; SymbolToStringPairIndex++)
	{
		const struct SymbolToStringPair* Pair = SYMBOL_TO_STRING_TABLE + SymbolToStringPairIndex;

		if (CharBufferReader_ReadNextExpected(&SourceReader, Pair->String))
		{
			// Found symbol. Output token to Tokenizer and return.
			struct Token NewToken = { 0 };
			NewToken.Type = TOKEN_SYMBOL;
			NewToken.BufferLocation = SymbolLoc;
			NewToken.Val.Symbol = Pair->Symbol;

			Vector_PushPtr(Tokenizer->Tokens, &NewToken);

			CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 1);
			return 1;
		}
	}

	// No matches in the keywords table - parsing unsuccessful.
	CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 0);
	return 0;
}

// Parses a literal number of any supported type / base.
// NOTE: Negative numbers are strictly speaking not supported at the tokenizer level. Instead, the preceding negation sign is read as a symbol / operator,
// and should turn the overall expression formed from it and the following number into a negative number at parsing time.
ui8 ParseLiteralNumber(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	struct CharBufferReader_ANSI SourceReader = OpenNestedBufferReader_ANSI(EntryReader);

	char NextChar = CharBufferReader_PeekNext(&SourceReader);

	// Check first character and early leave if this isn't a number at all. 
	if (NextChar < '0' || NextChar > '9')
	{
		// Not a number.
PARSE_FAIL:
		CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 0);
		return 0;
	}

	// Determine base.
	ui8 IsHex = CharBufferReader_ReadNextExpected(&SourceReader, "0x");
	ui8 IsBinary = !IsHex && CharBufferReader_ReadNextExpected(&SourceReader, "0b");
	ui8 IsOctal = !IsHex && !IsBinary && CharBufferReader_PeekNext(&SourceReader) == '0';
	if (IsOctal)
	{
		// Need an extra check that requires looking ahead by 1 character but stepping back if it turns out this is just a single 0.
		CharBufferReader_ReadNext(&SourceReader);
		IsOctal = CharBufferReader_PeekNext(&SourceReader) >= '0' && CharBufferReader_PeekNext(&SourceReader) <= '9';
		if (!IsOctal)
		{
			SourceReader._CurrentOffset--; // Step back one character.
		}
	}

	ui8 IsDecimal = !IsHex && !IsBinary && !IsOctal;

	char NumStrBuffer[256];
	memset(NumStrBuffer, 0, sizeof(NumStrBuffer));

	ui32 TokenLoc = SourceReader._CurrentOffset;

	int FigureCount = 0;
	for (;;FigureCount++)
	{
		if (FigureCount >= sizeof(NumStrBuffer) - 1)
		{
			// Too many figures.
			Tokenizer_Error(Tokenizer, TokenLoc, "Failed to parse number '%s' due to too many figures.", NumStrBuffer);
			goto PARSE_FAIL;
		}

		NextChar = CharBufferReader_PeekNext(&SourceReader);
		ui8 ValidChar =		(IsBinary && (NextChar == '0' || NextChar == '1'))
					|| (	(IsDecimal || IsHex) && NextChar >= '0' && NextChar <= '9')
					|| (	IsHex && (NextChar >= 'A' && NextChar <= 'F') || (NextChar >= 'a' && NextChar <= 'f'))
					|| (	IsOctal && NextChar >= '0' && NextChar <= '7');


		if (!ValidChar)
		{
			// Read a non-valid char for the number type / base we're currently parsing.
			// In most cases we just stop reading and continue, but in some specific cases we need to error out instead.

			if (IsOctal && (NextChar == '8' || NextChar == '9')) // Error case - Figure 8 or 9 found while parsing an Octal number.
			{
				Tokenizer_Error(Tokenizer, TokenLoc, "Invalid format for Octal number.");
				goto PARSE_FAIL;
			}
			
			break; // If none of the above error cases matched, break out of the loop !
		}

		// Consume number character.
		CharBufferReader_ReadNext(&SourceReader);

		NumStrBuffer[FigureCount] = NextChar;
	}

	// TODO: Support any number type.
	i64 ParsedNumber = _strtoi64(NumStrBuffer, NULL, IsBinary * 2 + IsHex * 16 + IsOctal * 8);
	if (ParsedNumber == _I64_MAX || ParsedNumber == _I64_MIN)
	{
		// Parsed number overflowed.
		Tokenizer_Error(Tokenizer, TokenLoc, "Failed to parse number '%s' due to overflow.", NumStrBuffer);
		goto PARSE_FAIL;
	}

	// Successfully parsed a decimal whole number. Create new token and push to Tokenizer.
	struct Token NewToken = { 0 };
	NewToken.Type = TOKEN_LITERAL_NUMBER_INT;
	NewToken.BufferLocation = TokenLoc;
	NewToken.Val.LiteralNumber.Integer = ParsedNumber;

	Vector_PushPtr(Tokenizer->Tokens, &NewToken);

	CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 1);
	return 1;
}

// Escape sequence resolver used by Literal String and Character parsing. Returns whether the escaped character was recognized, and populates OutChar
// with the escaped character.
ui8 ResolveEscapeCharacter(char EscapedChar, char* OutChar)
{
	switch (EscapedChar)
	{
	case '\'':
	case '"':
	case '\\':
		*OutChar = EscapedChar;
		return 1;
	case 'n':
		*OutChar = '\n';
		return 1;
	case 't':
		*OutChar = '\t';
		return 1;
	default:
		return 0;
	}
}

ui8 ParseLiteralString(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	struct CharBufferReader_ANSI SourceReader = OpenNestedBufferReader_ANSI(EntryReader);

	// Look for a " character, then parse all characters into the string until the closing " is encountered.
	if (!CharBufferReader_ReadNextExpected(&SourceReader, "\""))
	{
PARSE_FAIL:
		CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 0);
		return 0;
	}

	ui32 StringLoc = SourceReader._CurrentOffset;

	struct String_ANSI LiteralStr = String_Create_ANSI(NULL);

	for (;;)
	{
		if (LiteralStr.Length >= STRING_MAX_LENGTH_ANSI)
		{
			Tokenizer_Error(Tokenizer, StringLoc, "Literal string exceeds maximum length.");
			goto PARSE_FAIL;
		}

		char NextChar = CharBufferReader_ReadNext(&SourceReader);

		if (NextChar == '"')
		{
			break;
		}
		else if (NextChar == '\n' || NextChar == EOF)
		{
			Tokenizer_Error(Tokenizer, StringLoc, "Unterminated literal string.");
			goto PARSE_FAIL;
		}
		else if (NextChar == '\\')
		{
			// Escaped character. Read next one to determine which it is.
			NextChar = CharBufferReader_ReadNext(&SourceReader);

			if (!ResolveEscapeCharacter(NextChar, &NextChar))
			{
				// Unrecognized escaped character. Log an error and fail out.
				Tokenizer_Error(Tokenizer, StringLoc, "Unrecognized escaped character '%c'.", NextChar);
				goto PARSE_FAIL;
			}
		}

		// Write character to literal string. Resize the literal string here if necessary so we avoid constantly re-sizing it while pushing chars.
		if (LiteralStr._Capacity - 1 == LiteralStr.Length)
		{
			if (LiteralStr._Capacity < STRING_MAX_LENGTH_ANSI / 2)
			{
				String_Resize_ANSI(&LiteralStr, LiteralStr._Capacity * 2, 0);
			}
			else
			{
				String_Resize_ANSI(&LiteralStr, STRING_MAX_LENGTH_ANSI / 2, 0);
			}

			String_PushChar_ANSI(&LiteralStr, NextChar);
		}
	}

	// Shrink literal string down to just the size it needs.
	// TODO: Make a function that explictly does this for any string ? "ShrinkToFit" ? 
	String_Resize_ANSI(&LiteralStr, LiteralStr.Length + 1, 1);

	// Successfully parsed a literal string. Output token to Tokenizer and return.
	struct Token NewToken = { 0 };
	NewToken.Type = TOKEN_LITERAL_STRING;
	NewToken.BufferLocation = StringLoc;
	NewToken.Val.LiteralString = LiteralStr;

	Vector_PushPtr(Tokenizer->Tokens, &NewToken);

	CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 1);
	return 1;
}

ui8 ParseLiteralChar(struct TokenizerProcess* Tokenizer, struct CharBufferReader_ANSI* EntryReader)
{
	struct CharBufferReader_ANSI SourceReader = OpenNestedBufferReader_ANSI(EntryReader);

	// Look for a ' character, then parse a single character  Octal numbers cannot have the figure 8 or 9 in them !(after a possible \ escape) and expect the next one to be another '.
	if (CharBufferReader_ReadNext(&SourceReader) != '\'')
	{
	PARSE_FAIL:
		CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 0);
		return 0;
	}

	ui64 CharLoc = SourceReader._CurrentOffset;
	char NextChar = CharBufferReader_ReadNext(&SourceReader);
	if (NextChar == '\\')
	{
		// Escaped character. Read next one to determine which it is.
		NextChar = CharBufferReader_ReadNext(&SourceReader);

		if (!ResolveEscapeCharacter(NextChar, &NextChar))
		{
			// Unrecognized escaped character. Log an error and fail out.
			Tokenizer_Error(Tokenizer, CharLoc, "Unrecognized escaped character '%c'.", NextChar);
			goto PARSE_FAIL;
		}
	}
	else if (NextChar == '\'')
	{
		// Unexpected closure.
		Tokenizer_Error(Tokenizer, CharLoc, "Character literal must be non-empty.");
		goto PARSE_FAIL;
	}

	// Character is valid and has been escaped if necessary. Now we expect the closing character.
	if (CharBufferReader_ReadNext(&SourceReader) != '\'')
	{
		// Unterminated character literal.
		Tokenizer_Error(Tokenizer, CharLoc, "Unterminated character literal.");
		goto PARSE_FAIL;
	}

	// Successfully parsed character token. Create Literal Char token and add to the Tokenizer.
	struct Token NewToken = { 0 };
	NewToken.BufferLocation = CharLoc;
	NewToken.Type = TOKEN_LITERAL_CHAR;
	NewToken.Val.LiteralCharacter = NextChar;

	Vector_PushPtr(Tokenizer->Tokens, &NewToken);

	CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 1);
	return 1;
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
					// Successfully parsed a multi-line comment.
					CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 1);
					return 1;
				}
			}
		} while (NextChar != EOF);
		
		// Reaching here means the multi-line comment was not closed appropriately. Output an error and continue to failure.
		Tokenizer_Error(Tokenizer, SourceReader._CurrentOffset, "Unterminated multi-line comment.");
	}

	CloseNestedBufferReader_ANSI(&SourceReader, EntryReader, 0);
	return 0; // Failed to parse a comment.
}

// Prints every token currently held by the Tokenizer process to stdout, one line per token.
void Tokenizer_PrintTokens(struct TokenizerProcess* Tokenizer)
{
	ASSERT(Tokenizer != NULL);
	ASSERT(Tokenizer->Tokens != NULL);

	printf("===== TOKENIZER OUTPUT =====\n\n");

	for (int TokenIndex = 0; TokenIndex < Tokenizer->Tokens->Size; TokenIndex++)
	{
		struct Token* Tok = Vector_GetPtr(Tokenizer->Tokens, TokenIndex);

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
			printf("<SYMBOL: '%s'>\n", Symbol_ToString(Tok->Val.Symbol));
			break;
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


