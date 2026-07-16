Root folder for the project's general include files (.h), files containing symbols that are shared and usable by the entire project's source code. 

core.h is the default file for adding any project-wide symbols that could be used anywhere. Care should be taken when adding something there as it can pollute the namespace.

string_ansi.h is a simple header-only ANSI / ASCII string and buffer library, also including the ability to load a file, but does not HAVE to work with file content.

In the future, the intention is to add a main compiler.h file containing the basic in / out symbols for all stages of the compiler and non-assert error handling, and corresponding .h files for each stage to isolate their symbols from one another.
