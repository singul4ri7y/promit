#include <string.h>
#include <stdlib.h>

#include "vm.h"
#include "object.h"

void repl(VM* vm) {
	char line[2048];

	puts("Welcome to Promit v0.5.0 (beta)\nType '.help' for more information.\n");

	while(true) {
		printf("[promit] => ");

		if(!fgets(line, sizeof(line), stdin)) {	
			puts("");

			break;
		}

		if(!strcmp(line, ".help\n")) {
			puts("\nHow to use the REPL:\n\n"
			     "Try to input statements/expressions in a single line.\n"
			     "For multiline input, type '.editor' to go editor mode.\n"
			     "Type '.why' to know about the project.\n"
			     "Type '.clear' to clear the REPL console.\n");
		}
		else if(!strcmp(line, ".why\n")) {
			puts("\nAsif: The Project Promit is developed by SD Asif Hossein, in order to keep his promise once he made to his friend 'Meraj Hossain Promit'.\n"
				 "If you like it, be sure to use it ;).\n");
		}
		else if(!strcmp(line, ".editor\n")) {
			bool editorMode = true;
			
			// No inREPL while in editor mode.
			
			vm -> inREPL = false;

			puts("\nYou are now in editor mode! Type '.end' in a separate line at the end to finish, '.del' to cancel.\n");

			int top = 0, capacity = 2048;
			char* buffer = (char*) malloc(capacity);
			
			buffer[0] = 0;    // Termination character '\0'.

			int l = 1, len;

			while(editorMode) {
				printf("[editor] %d => ", l++);

				if(fgets(line, sizeof(line), stdin)) {
					if(!strcmp(line, ".del\n")) {
						editorMode = false;

						puts("");
						
						vm -> inREPL = true;

						continue;
					}
					else if(!strcmp(line, ".end\n")) {
						puts("\nResults:\n");

						editorMode = false;

						interpret(vm, buffer);
						
						puts("");
						
						// Back to REPL.
						
						vm -> inREPL = true;

						continue;
					}

					len = strlen(line);

					if(top + len >= capacity) {
						capacity *= 2;

						buffer = (char*) realloc(buffer, capacity);
					}

					strcpy(buffer + top, line);

					top += len;
				} else interpret(vm, buffer);
			}
			
			free(buffer);
		}
		else if(!strcmp(line, ".clear\n")) {
			system("clear");
		}
		else if(!strcmp(line, ".exit\n")) {
			freeVM(vm);
			
			exit(EXIT_SUCCESS);
		}
		else if(strlen(line) > 1) interpret(vm, line);
	}
}

void runFile(VM* vm, const char* path) {
	FILE* file = fopen(path, "rb");

	if(file == NULL) {
		fprintf(stderr, "[Error][VM]: Could not open file '%s'!\n", path);
		freeVM(vm);
		exit(EXIT_FAILURE);
	}

	fseek(file, 0L, SEEK_END);
	
	size_t fileSize = ftell(file);

	rewind(file);

	char* buffer = (char*) malloc((fileSize + 1u) * sizeof(char));

	if(buffer == NULL) {
		fprintf(stderr, "[Error][VM]: Not enough memory to load file '%s'!\n", path);
		freeVM(vm);
		exit(EXIT_FAILURE);
	}

	fileSize = fread(buffer, sizeof(char), fileSize, file);

	buffer[fileSize] = 0;

	InterpretResult result = interpret(vm, buffer);

	free(buffer);
	fclose(file);

	if(result == INTERPRET_RUNTIME_ERROR) { freeVM(vm); exit(70); }
	else if(result == INTERPRET_COMPILATION_ERROR) { freeVM(vm); exit(65); }
}

// Set environment variables.

void setArguments(int argc, char** argv, VM* vm) {
	argc++;

	ObjList* list = newList(vm);

	list -> capacity = argc;
	list -> count    = argc;
	list -> values   = GROW_ARRAY(Value, list -> values, 0u, argc);

	for(int i = 0; i < argc; i++) 
		list -> values[i] = OBJECT_VAL(TAKE_STRING(argv[i], strlen(argv[i]), false));
	
	tableInsert(&vm -> globals, TAKE_STRING("$_ARGS", 6u, false), (ValueContainer) { OBJECT_VAL(list), true });
}

void usage() {
	printf("promit [options] ... [arg] | [file]\n"
	       "\t-v    --version     Get version data\n"
	       "\t-c    --command     Run a promit statement/expression\n"
	       "\t-h    --help        Show help (this message)\n");
}

int main(int argc, char** argv) {
	VM vm;

	initVM(&vm);

	// If number of arguments is 1, then start read-eval-print loop.
	
	if(argc == 1) {
		setArguments(argc, argv, &vm);

		vm.inREPL = true;

		repl(&vm);
	}
	else if(argc > 1) {
		if(!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v")) 
			printf("Promit v0.5.0 (beta 3)\n");
		else if(!strcmp(argv[1], "-c") || !strcmp(argv[1], "--command")) {
			if(argc < 3) {
				fprintf(stderr, "Promit : No command string specified");

				freeVM(&vm);

				return EXIT_FAILURE;
			}

			setArguments(argc, argv, &vm);

			InterpretResult result = interpret(&vm, argv[2]);

			if(result == INTERPRET_COMPILATION_ERROR) { freeVM(&vm); exit(65); }
			else if(result == INTERPRET_RUNTIME_ERROR) { freeVM(&vm); exit(70); }
		}
		else if(!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) usage();
		else {
			setArguments(argc, argv, &vm);

			runFile(&vm, argv[1]);
		}
	}

	freeVM(&vm);

	return EXIT_SUCCESS;
}
