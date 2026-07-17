This folder contains the full implementation of the Tokenizer Process, which from the outside is basically one large function call that turns a buffer of characters into a vector of tokens.

Although the main entry point of the tokenizer is in tokenizer.c, one should not hesitate to split it into multiple .c files, which should then all be included in tokenizer.c directly.

The tokenizer works off of a tree-like approach, attempting to read the most "complex" (meaning "longest") token first and "giving up" into simpler tokens.

No static / global state should be used. Pass around the TokenizerProcess structure around every function as a means of reporting an error and its position, and use the Buffer Reader nesting system to ensure strict reading order and rollback.

Error handling is done by using one of the available error-handling functions which will fill in data inside the Tokenizer process, and in turn be passed to the compiler process.