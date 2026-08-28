Root folder for the project's general include files (.h), files containing symbols that are shared and usable by the entire project's source code. 

core.h is the default file for adding any project-wide symbols that could be used anywhere. Care should be taken when adding something there as it can pollute the namespace.

string_ansi.h is a simple header-only ANSI string and buffer library, also including the ability to load a file, but does not HAVE to work with file content.

vector.h is another header-only mini-library implementing a vector data structure for the purpose of easily working with dynamic-sized arrays of any type.

compiler.h contains the basic CompilerProcess declaration, implemented in compiler.c.

The compiler FOLDER contains a set of compiler object / structures declaration files used by one or more stages as input / output.