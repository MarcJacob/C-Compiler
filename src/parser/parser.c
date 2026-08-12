#include "parser.h"

void Parser_Error(struct ParserProcess* Parser, ui32 BufferLoc, const char* Format, ...);

void Parser_Run(struct ParserProcess* Parser)
{
	Parser_Error(Parser, 0, "Parser not implemented.");
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