#include "integrator.h"
#include <stdarg.h>

// Main implementation file for the Integrator stage.

void Integrator_Error(struct IntegratorProcess* Integrator, ui32 BufferLoc, const char* Format, ...)
{
	if (Integrator->HasError) return; // Most specific error only.

	Integrator->HasError = 1;
	Integrator->Error.Location = BufferLoc;

	va_list args;
	va_start(args, Format);
	Integrator->Error.Message = String_CreateFormatV_ANSI(Format, args);
	va_end(args);
}

// Main Integrator Process function. Turns the SourceASTs vector within the Process into an Integrated Program Tree (IPT).
void Integrator_Run(struct IntegratorProcess* Integrator)
{
	Integrator_Error(Integrator, 0, "Integrator unimplemented.");
}
