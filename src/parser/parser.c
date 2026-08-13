#include "parser.h"


// Helpers

// Emits an Error into the ParserProcess structure. From there on the Parser Process should finish as soon as possible.
void Parser_Error(struct ParserProcess* Parser, ui32 BufferLoc, const char* Format, ...);

// Returns the next Token to be read without advancing the internal reading cursor.
// Returns NULL when out of tokens.
struct Token* Parser_PeekToken(struct ParserProcess* Parser)
{
	if (Parser->SourceTokens->Size <= Parser->TokenIndex) return NULL;

	return Vector_GetPtr(Parser->SourceTokens, Parser->TokenIndex);
}

// Returns the next Token to be read and advances the internal reading cursor.
// Returns NULL when out of tokens.
struct Token* Parser_NextToken(struct ParserProcess* Parser)
{
	if (Parser->SourceTokens->Size <= Parser->TokenIndex) return NULL;

	return Vector_GetPtr(Parser->SourceTokens, Parser->TokenIndex++);
}

// Used to report buffer location when encountering unexpected EOF.
ui32 Parser_GetLastTokenBufferLoc(struct ParserProcess* Parser)
{
	ASSERT(Parser->SourceTokens->Size > 0);

	return ((struct Token*)Vector_GetPtr(Parser->SourceTokens, Parser->TokenIndex - 1))->BufferLocation;
}

// Attempts to parse the next few tokens into a DatatypeDef structure.
ui8 ParseDatatypeDef(struct ParserProcess* Parser, struct DatatypeDef* OutDatatypeDef)
{
	// Zero out output immediately.
	memset(OutDatatypeDef, 0, sizeof(*OutDatatypeDef));

	struct Token* NextToken = Parser_NextToken(Parser);
	if (NextToken == NULL)
	{
		goto PARSE_FAIL_EOF;
	}

	ui32 StartIndex = Parser->TokenIndex;

	// Check value of first token. It needs to be a primitive type or one of the user defined type keywords (struct, union, enum).
	// TODO: Support using identifiers for typedef'd types.
	if (NextToken->Type != TOKEN_KEYWORD
		|| (!Keyword_IsPrimitiveType(NextToken->Val.Keyword)
			&& !Keyword_IsTypeSpecifier(NextToken->Val.Keyword)
			&& NextToken->Val.Keyword != KEYWORD_STRUCT
			&& NextToken->Val.Keyword != KEYWORD_UNION
			&& NextToken->Val.Keyword != KEYWORD_ENUM))
	{
		// Not a type.
		goto PARSE_FAIL;
	PARSE_FAIL_EOF:
		Parser_Error(Parser, Parser_GetLastTokenBufferLoc(Parser), "Unexpected EOF when parsing data type.");
	PARSE_FAIL:
		Parser->TokenIndex = StartIndex;
		memset(OutDatatypeDef, 0, sizeof(*OutDatatypeDef));
		return 0;
	}

	// Check for static-ness.
	if (NextToken->Val.Keyword == KEYWORD_STATIC)
	{
		OutDatatypeDef->Flags = DATATYPE_IS_STATIC;
		NextToken = Parser_NextToken(Parser);
		if (NextToken == NULL)
		{
			goto PARSE_FAIL_EOF;
		}
	}

	// Handle structured / enumerated types...
	if (NextToken->Val.Keyword == KEYWORD_STRUCT)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Struct parsing is unimplemented.");
		goto PARSE_FAIL;
	}
	if (NextToken->Val.Keyword == KEYWORD_UNION)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Union parsing is unimplemented.");
		goto PARSE_FAIL;
	}
	if (NextToken->Val.Keyword == KEYWORD_ENUM)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Enum parsing is unimplemented.");
		goto PARSE_FAIL;
	}

	// ... Otherwise we are parsing a primitive type.

	// Determine volatility.
	if (NextToken->Val.Keyword == KEYWORD_VOLATILE)
	{
		OutDatatypeDef->Flags |= DATATYPE_IS_VOLATILE;
		NextToken = Parser_NextToken(Parser);
		if (NextToken == NULL)
		{
			goto PARSE_FAIL_EOF;
		}
	}

	// Determine signage.
	OutDatatypeDef->Flags |= DATATYPE_IS_UNSIGNED * (NextToken->Val.Keyword == KEYWORD_UNSIGNED);
	ui8 SignKeywordPresent = 0;
	if (NextToken->Val.Keyword == KEYWORD_SIGNED
		|| NextToken->Val.Keyword == KEYWORD_UNSIGNED)
	{
		SignKeywordPresent = 1;
		NextToken = Parser_NextToken(Parser);
		if (NextToken == NULL)
		{
			goto PARSE_FAIL_EOF;
		}

		// If signage keyword is present and next token isn't a primitive type, then the signage alone represents a 32 bits integer.
		if (NextToken->Type != TOKEN_KEYWORD || !Keyword_IsPrimitiveType(NextToken->Val.Keyword))
		{
			OutDatatypeDef->Type = DATATYPE_INT32;
			OutDatatypeDef->Size = 4;
		}
	}

	// Next determine size and broad type, if the next token is another keyword.
	if (NextToken->Type == TOKEN_KEYWORD)
	{
		switch (NextToken->Val.Keyword)
		{
		case KEYWORD_VOID:
			if (SignKeywordPresent)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier combination.");
				goto PARSE_FAIL;
			}
			OutDatatypeDef->Type = DATATYPE_VOID;
			OutDatatypeDef->Size = _WIN64 * 4 + 4; // Assume this will be a pointer of a least a single level of indirection.
			break;
		case KEYWORD_CHAR:
			OutDatatypeDef->Type = DATATYPE_CHAR;
			OutDatatypeDef->Size = 1;
			break;
		case KEYWORD_SHORT:
			OutDatatypeDef->Type = DATATYPE_SHORT;
			OutDatatypeDef->Size = 2;
			break;
		case KEYWORD_INT:
			OutDatatypeDef->Type = DATATYPE_INT32;
			OutDatatypeDef->Size = 4;
			break;
		case KEYWORD_LONG:
			OutDatatypeDef->Type = DATATYPE_INT64;
			OutDatatypeDef->Size = 8;
			break;
		case KEYWORD_FLOAT:
			if (SignKeywordPresent)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier combination.");
				goto PARSE_FAIL;
			}

			OutDatatypeDef->Type = DATATYPE_FLOAT;
			OutDatatypeDef->Size = 4;
			break;
		case KEYWORD_DOUBLE:
			if (SignKeywordPresent)
			{
				Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier combination.");
				goto PARSE_FAIL;
			}

			OutDatatypeDef->Type = DATATYPE_DOUBLE;
			OutDatatypeDef->Size = 8;
			break;
		default:
			Parser_Error(Parser, NextToken->BufferLocation, "Unexpected keyword.");
			goto PARSE_FAIL;
		}

		NextToken = Parser_NextToken(Parser);
		if (NextToken == NULL)
		{
			goto PARSE_FAIL_EOF;
		}

		// Specifically in the case of LONG, move it back to 32 bits if only a single long keyword is present.
		if (OutDatatypeDef->Type == DATATYPE_INT64)
		{
			if (NextToken->Type != TOKEN_KEYWORD || NextToken->Val.Keyword != KEYWORD_LONG)
			{
				// Retrograde back to INT32. TODO: Handle long double.
				OutDatatypeDef->Type = DATATYPE_INT32;
				OutDatatypeDef->Size = 4;
			}
			else
			{
				// Otherwise skip over the second long token.
				NextToken = Parser_NextToken(Parser);
			}
		}
	}

	// By this point we MUST have a valid size.
	if (OutDatatypeDef->Size == 0)
	{
		// Something didn't make sense...
		Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier combination.");
		goto PARSE_FAIL;
	}

	// Parse pointer levels.
	while (NextToken->Type == TOKEN_SYMBOL && NextToken->Val.Symbol == SYMBOL_STAR)
	{
		NextToken = Parser_NextToken(Parser);
		if (NextToken == NULL)
		{
			goto PARSE_FAIL_EOF;
		}

		OutDatatypeDef->PointerLevel++;
	}

	// Check specific error case - non-pointer VOID type.
	if (OutDatatypeDef->Type == DATATYPE_VOID && OutDatatypeDef->PointerLevel == 0)
	{
		Parser_Error(Parser, NextToken->BufferLocation, "Invalid type specifier combination.");
		goto PARSE_FAIL;
	}

PARSE_SUCCESS:
	return 1;
}

// Root Parser functions

// Attempts to parse a new AST, covering a Global Variable symbol declaration and optionally its definition.
ui8 ParseGlobalVariable(struct ParserProcess* Parser);

// Attempts to parse a new AST, covering a Function symbol declaration and optionally its definition.
ui8 ParseFunction(struct ParserProcess* Parser);

// Attempts to parse a new AST, covering a Struct declaration and optionally its definition.
ui8 ParseStruct(struct ParserProcess* Parser);

// Main Parser Process function. Turns the SourceTokens vector within the Process into a set of Abstract Syntax Trees (ASTs) whose roots will be put
// in the RootNodes vector in the Process.
void Parser_Run(struct ParserProcess* Parser)
{
	ASSERT(Parser->SourceTokens != NULL);
	ASSERT(Parser->RootNodes != NULL);

	while (Parser_PeekToken(Parser) != NULL)
	{
		// Attempt to parse AST Node trees in an arbitrary order.
		if (	ParseFunction(Parser)
			||	ParseStruct(Parser)
			||	ParseGlobalVariable(Parser))
		{
			// Successfully parsed node tree.
		}
		else
		{
			if (!Parser->HasError)
			{
				ui32 ParserLoc = Parser_GetLastTokenBufferLoc(Parser);
				if (Parser_PeekToken(Parser) != NULL)
				{
					ParserLoc = Parser_PeekToken(Parser)->BufferLocation;
				}
				Parser_Error(Parser, ParserLoc, "Unknown error while parsing.");
			}

			break;
		}
	}
}

void Parser_Error(struct ParserProcess* Parser, ui32 BufferLoc, const char* Format, ...)
{
	Parser->HasError = 1;
	Parser->Error.Location = BufferLoc;

	va_list args;
	va_start(args, Format);
	Parser->Error.Message = String_CreateFormatV_ANSI(Format, args);
	va_end(args);
}

ui8 ParseGlobalVariable(struct ParserProcess* Parser)
{
	// UNIMPLEMENTED.
	return 0;
}

ui8 ParseFunction(struct ParserProcess* Parser)
{
	int StartTokenIndex = Parser->TokenIndex;

	struct DatatypeDef ReturnType;
	if (!ParseDatatypeDef(Parser, &ReturnType))
	{
		// Not a function.
		return 0;
	}

	// TODO: Parse function identifier and parameters for declaration. Attempt to parse a body with it for a full definition.
	return 0;
}

ui8 ParseStruct(struct ParserProcess* Parser)
{
	// UNIMPLEMENTED.
	return 0;
}
