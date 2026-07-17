// Simple library for handling buffers and strings of ANSI / ASCII single byte characters.

#ifndef STRING_ANSI_INCLUDED
#define STRING_ANSI_INCLUDED

#include "core.h"

// Buffer of ANSI / ASCII characters. Used for source code and whatever intermediate file formats are supported down the road.
// The buffer should preferably be accessed using an appropriate Reader structure.
struct CharBuffer_ANSI
{
	char* _Mem;
	ui32 Size;
};

#define BUFFER_MAX_SIZE_ANSI ((ui32)(~0))

// Reads a file fully into a new ANSI Char buffer.
// If there was an error, the buffer will have a NULL memory pointer and its Size member will contain the error code.
struct CharBuffer_ANSI LoadFileToBuffer_ANSI(const char* Filename)
{
	struct CharBuffer_ANSI NewBuffer;
	NewBuffer._Mem = NULL;
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

	NewBuffer._Mem = (char*)malloc(Filesize);
	ASSERT(NewBuffer._Mem != NULL);

	NewBuffer.Size = Filesize;

	ui64 ReadSize = fread_s(NewBuffer._Mem, NewBuffer.Size, 1, Filesize, File);
	ASSERT(ReadSize > 0);

	// Adjust "public" buffer size to match how many bytes were actually read from the file in text mode.
	NewBuffer.Size = ReadSize;
	return NewBuffer;
}

void FreeBuffer_ANSI(struct CharBuffer_ANSI* _Mem)
{
	ASSERT(_Mem != NULL);
	ASSERT(_Mem->_Mem != NULL);

	free(_Mem->_Mem);
	_Mem->_Mem = NULL;
	_Mem->Size = 0;
}

// Reader for ANSI Char buffers.
// Use the associated functions to initialize and read safely and cleanly.
struct CharBufferReader_ANSI
{
	struct CharBuffer_ANSI* _Buffer;

	ui32 _StartOffset;
	ui32 _CurrentOffset;
};

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

// Zeroes-out the Nested Reader after advancing the Parent to its own offset (if Undo == 0) or leaving it untouched (if Undo == 1).
struct CharBufferReader_ANSI CloseNestedBufferReader_ANSI(struct CharBufferReader_ANSI* NestedReader, struct CharBufferReader_ANSI* Parent, i32 Undo)
{
	ASSERT(NestedReader != NULL);
	ASSERT(Parent != NULL);

	// For the nested reader to be a valid "child" of the parent, it must have a STARTING offset GREATER OR EQUAL to its parent's CURRENT offset.
	// It must of course also point to the same buffer.
	ASSERT(NestedReader->_Buffer == Parent->_Buffer && NestedReader->_StartOffset >= Parent->_CurrentOffset, "Nested buffer reader is not a child of Parent.");

	if (!Undo)
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

	return Reader->_Buffer->_Mem[Reader->_CurrentOffset++];
}

// Gets the next character from the reader without advancing it.
char CharBufferReader_PeekNext(struct CharBufferReader_ANSI* Reader)
{
	ASSERT(Reader != NULL && Reader->_Buffer != NULL);

	if (Reader->_CurrentOffset >= Reader->_Buffer->Size) return EOF;

	return Reader->_Buffer->_Mem[Reader->_CurrentOffset];
}

// Advances the reader, reading into the provided buffer until encountering EOF or one of the characters in the StopChars string.
// Does NOT read the encountered StopChar into the buffer, but returns it.
// If Buffer is too small, stops reading at that point and returns 0.
char CharBufferReader_ReadUntil(struct CharBufferReader_ANSI* Reader, char* Buffer, i32 BufferSize, const char* StopChars)
{
	ASSERT(Reader != NULL && Reader->_Buffer != NULL);
	ASSERT(Buffer != NULL && BufferSize > 0);
	ASSERT(StopChars != NULL);

	i32 StopCharsCount = strlen(StopChars);
	ASSERT_MSG(StopCharsCount > 0, "StopChars string must contain at least one non-zero char !");
	
	i32 BufferIndex = 0;
	while (BufferIndex < BufferSize)
	{
		char Next = CharBufferReader_ReadNext(Reader);
		
		for (i32 StopCharIndex = 0; StopCharIndex < StopCharsCount; StopCharIndex++)
		{
			if (Next == StopChars[StopCharIndex])
			{
				return Next;
			}
		}

		// Next is not a stop character. Put it in the buffer.
		Buffer[BufferIndex++] = Next;
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
	CloseNestedBufferReader_ANSI(&OpReader, Reader, CharIndex < ExpectedStringLen);

	return CharIndex == ExpectedStringLen;
}

// TODO: Implement String type (working very much like a vector of char).

#endif // STRING_ANSI_INCLUDED