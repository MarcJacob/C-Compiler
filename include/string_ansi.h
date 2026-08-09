// Simple library for handling buffers and strings of ANSI / ASCII single byte characters.

#ifndef STRING_ANSI_INCLUDED
#define STRING_ANSI_INCLUDED

#include "core.h"
#include <stdarg.h>

// Buffer of ANSI / ASCII characters. Used for source code and whatever intermediate file formats are supported down the road.
// The buffer should preferably be accessed using an appropriate Reader structure.
struct CharBuffer_ANSI
{
	char* Str;
	ui32 Size;
};

struct CharBuffer_ANSI LoadFileToBuffer_ANSI(const char* Filename);
void FreeBuffer_ANSI(struct CharBuffer_ANSI* Str);

// Reader for ANSI Char buffers.
// Use the associated functions to initialize and read safely and cleanly.
struct CharBufferReader_ANSI
{
	struct CharBuffer_ANSI* _Buffer;

	ui32 _StartOffset;
	ui32 _CurrentOffset;
};

struct CharBufferReader_ANSI CreateBufferReader_ANSI(struct CharBuffer_ANSI* SourceBuffer);
struct CharBufferReader_ANSI OpenNestedBufferReader_ANSI(struct CharBufferReader_ANSI* Parent);
void CloseNestedBufferReader_ANSI(struct CharBufferReader_ANSI* NestedReader, struct CharBufferReader_ANSI* Parent, i32 Apply);
char CharBufferReader_ReadNext(struct CharBufferReader_ANSI* Reader);
char CharBufferReader_PeekNext(struct CharBufferReader_ANSI* Reader);
char CharBufferReader_ReadUntil(struct CharBufferReader_ANSI* Reader, struct String_ANSI* OutString, const char* StopChars);
i32 CharBufferReader_ReadNextExpected(struct CharBufferReader_ANSI* Reader, const char* ExpectedString);

// Simple container for a dynamic-size sequence of null-terminated ANSI characters so it can easily be used with C String functions.
struct String_ANSI
{
	char* Str;
	ui16 _Capacity;

	ui16 Length;
};

struct String_ANSI String_Create_ANSI(const char* InitChars);
struct String_ANSI String_CreateFormatV_ANSI(const char* StrFormat, va_list args);
struct String_ANSI String_CreateFormat_ANSI(const char* StrFormat, ...);
void String_Free_ANSI(struct String_ANSI* Str);

void String_PushChar_ANSI(struct String_ANSI* Str, char Char);
void String_Push_ANSI(struct String_ANSI* Str, const char* Chars);
void String_Resize_ANSI(struct String_ANSI* Str, ui16 NewSize, ui8 CanShrink);

#define BUFFER_MAX_SIZE_ANSI ((ui32)(~0))
#define STRING_MAX_LENGTH_ANSI ((ui16)(~0)) - 1

// Reads a file fully into a new ANSI Char buffer.
// If there was an error, the buffer will have a NULL memory pointer and its Size member will contain the error code.
struct CharBuffer_ANSI LoadFileToBuffer_ANSI(const char* Filename)
{
	struct CharBuffer_ANSI NewBuffer;
	NewBuffer.Str = NULL;
	NewBuffer.Size = 0;

	FILE* File;
	errno_t Err = fopen_s(&File, Filename, "r");

	if (Err)
	{
		printf("Error reading file '%s' into memory.", Filename);

		NewBuffer.Size = Err;
		return NewBuffer;
	}

	fseek(File, 0, SEEK_END);
	ui32 Filesize = ftell(File);
	ASSERT(Filesize > 0);

	fseek(File, 0, 0);

	NewBuffer.Str = (char*)malloc(Filesize);
	ASSERT(NewBuffer.Str != NULL);

	NewBuffer.Size = Filesize;

	ui64 ReadSize = fread_s(NewBuffer.Str, NewBuffer.Size, 1, Filesize, File);
	ASSERT(ReadSize > 0);

	// Adjust "public" buffer size to match how many bytes were actually read from the file in text mode.
	NewBuffer.Size = ReadSize;
	return NewBuffer;
}

void FreeBuffer_ANSI(struct CharBuffer_ANSI* Str)
{
	ASSERT(Str != NULL);
	ASSERT(Str->Str != NULL);

	free(Str->Str);
	Str->Str = NULL;
	Str->Size = 0;
}

// Creates new "root reader" for an ANSI buffer, starting at offset 0.
struct CharBufferReader_ANSI CreateBufferReader_ANSI(struct CharBuffer_ANSI* SourceBuffer)
{
	ASSERT(SourceBuffer != NULL);

	struct CharBufferReader_ANSI NewReader;
	NewReader._Buffer = SourceBuffer;
	NewReader._CurrentOffset = 0;
	NewReader._StartOffset = 0;

	return NewReader;
}

// Creates "Opens" a "nested" ANSI buffer reader, basically just a copy, 
// following the idea that you may want to read through characters and either "undo" or "confirm" the read into the parent reader.
struct CharBufferReader_ANSI OpenNestedBufferReader_ANSI(struct CharBufferReader_ANSI* Parent)
{
	ASSERT(Parent != NULL);

	struct CharBufferReader_ANSI NewReader;
	NewReader._Buffer = Parent->_Buffer;
	NewReader._CurrentOffset = Parent->_CurrentOffset;
	NewReader._StartOffset = Parent->_CurrentOffset;

	return NewReader;
}

// Zeroes-out the Nested Reader after advancing the Parent to its own offset (if Apply == 1) or leaving it untouched (if Apply == 0).
void CloseNestedBufferReader_ANSI(struct CharBufferReader_ANSI* NestedReader, struct CharBufferReader_ANSI* Parent, i32 Apply)
{
	ASSERT(NestedReader != NULL);
	ASSERT(Parent != NULL);

	// For the nested reader to be a valid "child" of the parent, it must have a STARTING offset GREATER OR EQUAL to its parent's CURRENT offset.
	// It must of course also point to the same buffer.
	ASSERT_MSG(NestedReader->_Buffer == Parent->_Buffer && NestedReader->_StartOffset >= Parent->_CurrentOffset, "Nested buffer reader is not a child of Parent.");

	if (Apply)
	{
		Parent->_CurrentOffset = NestedReader->_CurrentOffset;
	}

	memset(NestedReader, 0, sizeof(*NestedReader));
}

// Advances the reader by a single character and returns it.
char CharBufferReader_ReadNext(struct CharBufferReader_ANSI* Reader)
{
	ASSERT(Reader != NULL && Reader->_Buffer != NULL);

	if (Reader->_CurrentOffset >= Reader->_Buffer->Size) return EOF;

	return Reader->_Buffer->Str[Reader->_CurrentOffset++];
}

// Gets the next character from the reader without advancing it.
char CharBufferReader_PeekNext(struct CharBufferReader_ANSI* Reader)
{
	ASSERT(Reader != NULL && Reader->_Buffer != NULL);

	if (Reader->_CurrentOffset >= Reader->_Buffer->Size) return EOF;

	return Reader->_Buffer->Str[Reader->_CurrentOffset];
}

// Advances the reader, reading into the provided buffer until encountering EOF or one of the characters in the StopChars string.
// Returns the specific character reading stopped at (on top of the buffer's cursor being positioned on it).
// If OutString is non-NULL, will fill in the read characters into it.
char CharBufferReader_ReadUntil(struct CharBufferReader_ANSI* Reader, struct String_ANSI* OutString, const char* StopChars)
{
	ASSERT(Reader != NULL && Reader->_Buffer != NULL);
	ASSERT(StopChars != NULL);

	i32 StopCharsCount = strlen(StopChars);
	ASSERT_MSG(StopCharsCount > 0, "StopChars string must contain at least one non-zero char !");
	
	i32 BufferIndex = 0;
	char Next = CharBufferReader_PeekNext(Reader);
	while (Next != EOF)
	{
		char Next = CharBufferReader_ReadNext(Reader);
		
		for (i32 StopCharIndex = 0; StopCharIndex < StopCharsCount; StopCharIndex++)
		{
			if (Next == StopChars[StopCharIndex])
			{
				return Next;
			}
		}

		if (OutString != NULL)
		{
			String_Push_ANSI(OutString, &Next);
		}
	}

	// Buffer size exceeded.
	return 0;
}

// Advances the reader, provided the next characters form the exact provided string.
// If they don't, the reader does not advance at all.
// Returns whether the the reader was advanced.
i32 CharBufferReader_ReadNextExpected(struct CharBufferReader_ANSI* Reader, const char* ExpectedString)
{
	ASSERT(Reader != NULL && Reader->_Buffer != NULL);
	ASSERT(ExpectedString != NULL);

	struct CharBufferReader_ANSI OpReader = OpenNestedBufferReader_ANSI(Reader);

	i32 ExpectedStringLen = strlen(ExpectedString);
	ASSERT_MSG(ExpectedStringLen > 0, "Expected String must contain at least one non-zero character !");

	i32 CharIndex = 0;
	for (; CharIndex < ExpectedStringLen; CharIndex++)
	{
		char Next = CharBufferReader_ReadNext(&OpReader);
		if (Next != ExpectedString[CharIndex])
		{
			break; // Break out early, triggering a failure scenario for closing the nested reader and return value.
		}
	}

	// Close nested reader, undoing the read if we did not find the expected string.
	CloseNestedBufferReader_ANSI(&OpReader, Reader, CharIndex == ExpectedStringLen);
	return CharIndex == ExpectedStringLen;
}

// Reads the next characters until a non-alphanumeric or underscore is encountered.
// Allows words that start with a number through, so if that is not desirable the user should check the first character themselves first.
// Returns the length of the word that was read.
// If OutString is non-NULL, will fill in the read characters into it.
i32 CharBufferReader_ReadNextWord(struct CharBufferReader_ANSI* Reader, struct String_ANSI* OutString)
{
	ASSERT(Reader != NULL);

	struct CharBufferReader_ANSI OpReader = OpenNestedBufferReader_ANSI(Reader);

	ui16 WordLen = 0;
	for (;;)
	{
		char NextChar = CharBufferReader_PeekNext(&OpReader);
		if ((NextChar >= 'a' && NextChar <= 'z')
			|| (NextChar >= 'A' && NextChar <= 'Z')
			|| (NextChar >= '0' && NextChar <= '9')
			|| (NextChar == '_'))
		{
			if (OutString != NULL)
			{
				String_PushChar_ANSI(OutString, NextChar);
			}
			WordLen++;

			// Consume char.
			CharBufferReader_ReadNext(&OpReader);
		}
		else
		{
			break;
		}
	}

	// Succeed if the word is at least one character long.
	CloseNestedBufferReader_ANSI(&OpReader, Reader, WordLen > 0);
	return WordLen;
}

// Changes the (minimum) capacity of the string so it has room for a useable string of length NewSize.
void String_Resize_ANSI(struct String_ANSI* Str, ui16 NewSize, ui8 CanShrink)
{
	ASSERT(Str != NULL);

	// Handle special case of NewSize == 0, which is effectively just a request to free the string.
	if (NewSize == 0)
	{
		ASSERT(CanShrink);

		free(Str->Str);
		Str->Length = 0;
		Str->_Capacity = 0;
		Str->Str = NULL;

		return;
	}

	// Allocate by groups of 8 bytes at the lowest granularity.
	ui16 NewCapacity = (NewSize + 7) / 8 * 8;

	if ((Str->_Capacity > NewCapacity && CanShrink) || (Str->_Capacity < NewCapacity))
	{
		// Create new buffer, copy old buffer into it if it exists and zero out extra characters.
		char* NewStringBuffer = malloc(NewCapacity);
		if (Str->Str != NULL)
		{
			memcpy(NewStringBuffer, Str->Str, Str->_Capacity);
			free(Str->Str);
			Str->Str = NULL;
		}	
		
		memset(NewStringBuffer + Str->_Capacity, 0, NewCapacity - Str->_Capacity);

		Str->_Capacity = NewCapacity;
		Str->Str = NewStringBuffer;
	}
}

// Adds a single character to the string, resizing it if necessary.
void String_PushChar_ANSI(struct String_ANSI* Str, char Char)
{
	ASSERT(Str != NULL);

	ASSERT(1 + Str->Length < STRING_MAX_LENGTH_ANSI);

	String_Resize_ANSI(Str, Str->Length + 1, 0);
	Str->Str[Str->Length++] = Char;
}

// Adds the given characters to the string, resizing it if necessary.
void String_Push_ANSI(struct String_ANSI* Str, const char* Chars)
{
	ASSERT(Str != NULL);
	ASSERT(Chars != NULL);

	size_t CharsLen = strlen(Chars);
	ASSERT(CharsLen + Str->Length < STRING_MAX_LENGTH_ANSI);

	String_Resize_ANSI(Str, CharsLen + Str->Length, 0);

	memcpy(Str->Str + Str->Length, Chars, CharsLen);
	Str->Length += CharsLen;
}

// Allocates a new ANSI String of exactly correct size and content from the given start characters.
struct String_ANSI String_Create_ANSI(const char* InitChars)
{
	struct String_ANSI NewString = { 0 };
	if (InitChars != NULL)
	{
		String_Push_ANSI(&NewString, InitChars);
	}

	return NewString;
}

// Allocates a new ANSI String of exactly correct size and content from the given start format string and parameters.
struct String_ANSI String_CreateFormatV_ANSI(const char* StrFormat, va_list args)
{
	ASSERT(StrFormat != NULL);

	struct String_ANSI NewString = { 0 };

	String_Resize_ANSI(&NewString, vsnprintf(NULL, 0, StrFormat, args) + 1, 0); // First pass - determine required string size including null terminator.
	vsnprintf(NewString.Str, NewString._Capacity, StrFormat, args); // Second pass - perform actual formatting and copying.
	NewString.Length = NewString._Capacity - 1;

	return NewString;
}

// Allocates a new ANSI String of exactly correct size and content from the given start format string and parameters.
struct String_ANSI String_CreateFormat_ANSI(const char* StrFormat, ...)
{
	ASSERT(StrFormat != NULL);

	va_list args;
	va_start(args, StrFormat);
	struct String_ANSI NewString = String_CreateFormatV_ANSI(StrFormat, args);
	va_end(args);

	return NewString;
}

void String_Free_ANSI(struct String_ANSI* Str)
{
	ASSERT(Str == NULL);
	String_Resize_ANSI(Str, 0, 1);
}
#endif // STRING_ANSI_INCLUDED