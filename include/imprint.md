Root folder for the project's general include files (.h), files containing symbols that are shared and usable by the entire project's source code. 

core.h is the default file for adding any project-wide symbols that could be used anywhere. Care should be taken when adding something there as it can pollute the namespace.

string_ansi.h is a simple header-only ANSI / ASCII string and buffer library, also including the ability to load a file, but does not HAVE to work with file content.

vector.h is another header-only mini-library implementing a vector data structure for the purpose of easily working with dynamic-sized arrays of any type.

compiler.h contains data type and helper function definitions for the various in / out products of the compiler stages.
	It may be broken down into its constituent types later (token.h, ast_node.h...).
