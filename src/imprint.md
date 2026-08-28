Root folder for the project's source code, meaning implementation (.c) files and local includes (.h) that are not going to be shared to children or siblings in the folder hierarchy. 

The project is made up of any number of files, but must always "collapse" into a single translation unit around the main.c file.

main.c should only contain the "front end" of the program, launching the compiler stage(s) required from user input, and include all the other .c files.

compiler.c contains core compiler orchestration, making use of all stages as appropriate, and implementing error routing: extracting each stage's error state and building the full compiler error state from it, ready to be logged to the user.

The implementation files of each stage should live in their own folder.

For each stage, the structure is simple: some input is fed into a Process structure and runs through a \<Stage>_Run function. Once the function is done, either some error flag is set on the process structure or some pre-assigned output product is ready for consumption.

Design-wise, stages may (and must, to some degree) share symbols. For any given stage, the input data they use should always limit itself to one "layer" before itself.
	- Take this process pipeline : ... -> Preprocessed Source code (= Source Buffers) -> Tokenizer (= Tokens) -> Parser (= Syntax Trees) -> Integrators -> ...
		- In this example, The parser would be strictly forbidden from depending on any Source Buffer data, and the Integrator would also be forbidden from depending on Token data.
	- The idea is that each stage process can work as its own sub-program and take "raw input" from any source, even if that input was not built from the preceding stages of the standard pipeline.