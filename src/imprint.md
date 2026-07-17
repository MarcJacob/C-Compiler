Root folder for the project's source code, meaning implementation (.c) files and local includes (.h) that are not going to be shared to children or siblings in the folder hierarchy. 

The project is made up of any number of files, but must always "collapse" into a single translation unit around the main.c file.

main.c should only contain the "front end" of the program, launching the compiler stage(s) required from user input, and include all the other .c files.

compiler.c contains core compiler orchestration, making use of all stages as appropriate, and implementing the error handling and other globally useable stuff for all stages.

The implementation files of each stage should live in their own folder.