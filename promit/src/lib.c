#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define _GNU_SOURCE
#include <time.h>

#include "dtoa.h"
#include "lib.h"

#define setStatic(klass, name, value) tableInsert(&klass -> statics, name, ((ValueContainer) { value, true }));
#define setField(instance, name, value) tableInsert(&instance -> fields, name, ((ValueContainer) { value, true }));
#define setMethod(klass, name, value) tableInsert(&klass -> methods, name, ((ValueContainer) { value, true }));

#define getField(instance, name, value) tableGet(&instance -> fields, name, &value);

#define NATIVE_R_ERR(format, ...) RUNTIME_ERROR(format, ##__VA_ARGS__);\
	pack.hadError = true;\
	return pack;

#define initNativePack NativePack pack;\
	pack.hadError = false;\
	pack.value = NULL_VAL;

#define defineMethod(klass, name, fnName) \
	method = newNative(vm, fnName);\
	setMethod(klass, name, OBJECT_VAL(method));
	
#define defineStaticMethod(klass, name, fnName) \
	method = newNative(vm, fnName);\
	setStatic(klass, name, OBJECT_VAL(method));

static ObjClass* fileClass;

#define fileInstanceFile if(!IS_INSTANCE(values[0]) || VALUE_INSTANCE(values[0]) -> klass != fileClass) {\
	NATIVE_R_ERR("Provided reciever/instance (this) must be of file instance!");\
}\
	ObjInstance* instance = VALUE_INSTANCE(values[0]);\
	ValueContainer fileContainer;\
	getField(instance, fileField, fileContainer);\
	ObjFile* file = VALUE_FILE(fileContainer.value);

static ObjString* fileField;
static ObjString* modeField;
static ObjString* locationField;
static ObjString* sizeField;
static ObjString* deniedStdin;
static ObjString* deniedStdout;
static ObjString* deniedStderr;

static char* fileModes[] = { "r", "w", "r+", "w+", "a", "a+" };

static size_t fileOpened = 0u;

static NativePack fileInit(VM* vm, int argCount, Value* values) {
	initNativePack;

	if(!IS_INSTANCE(values[0]) || VALUE_INSTANCE(values[0]) -> klass != fileClass) {
		NATIVE_R_ERR("Need a file instance to call File.init(filename, mode)!");
	}
	
	if(argCount < 3) {
		NATIVE_R_ERR("Too few arguments to call File.init(filename, mode)!");
	}
	
	ObjFile* file = newFile(vm);
	
	ObjInstance* instance = VALUE_INSTANCE(values[0]);
	
	setField(instance, fileField, OBJECT_VAL(file));
	
	if(!IS_STRING(values[1])) {
		NATIVE_R_ERR("Expected parameter 1 (location) to be a string in File.init(filename, mode)!");
	}
	
	if(!IS_NUMBER(values[2])) {
		NATIVE_R_ERR("Expected parameter 2 to be a valid mode in file.init(filename, mode)!");
	}
	
	setField(instance, locationField, values[1]);
	setField(instance, modeField, values[2]);
	setField(instance, sizeField, NUMBER_VAL(-1));

	int modeType = (int) VALUE_NUMBER(values[2]);
	
	if(modeType > 11 || modeType < 0) {
		NATIVE_R_ERR("Unrecognized file mode!");
	}
	
	char* mode = fileModes[modeType];
	
	if(fileOpened + 1u > FOPEN_MAX) {
		NATIVE_R_ERR("Maximum simultaneous opened file limit reached in File.init(filename, mode)!");
	}
	
	if((file -> file = fopen(VALUE_CSTRING(values[1]), mode)) != NULL) 
		fileOpened++;
	
	pack.value = values[0];
	
	return pack;
}

static NativePack fileWrite(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call function File.write(line)");
	}
	
	fileInstanceFile;
	
	double wrote;

	// If the current file pointer is NULL, segmentation fault may occur.

	if(file -> file == NULL) {
		pack.value = NUMBER_VAL(EOF);

		return pack;
	}
	
	if(IS_STRING(values[1])) {
		ObjString* string = VALUE_STRING(values[1]);
		
		wrote = (double) fprintf(file -> file, string -> buffer);
	}
	else if(IS_BYTELIST(values[1])) {
		ObjByteList* byteList = VALUE_BYTELIST(values[1]);
		
		wrote = (double) fwrite(byteList -> bytes, 1u, byteList -> size, file -> file);
	} else {
		NATIVE_R_ERR("Expected a string or bytelist as the first argument!");
	}
	
	pack.value = NUMBER_VAL(wrote);
	
	return pack;
}

static NativePack fileWriteLine(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		NATIVE_R_ERR("Too few parameters to call function File.write_line(line)");
	}
	
	if(!IS_STRING(values[1])) {
		NATIVE_R_ERR("Expected a string at first argument!");
	}
	
	fileInstanceFile;

	// Prevent segmentation fault.

	if(file -> file == NULL) {
		pack.value = NUMBER_VAL(EOF);

		return pack;
	}
	
	ObjString* string = VALUE_STRING(values[1]);
	
	double wrote = fprintf(file -> file, "%s\n", string -> buffer);
	
	pack.value = NUMBER_VAL(wrote);
	
	return pack;
}

static NativePack fileIsOpened(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	pack.value = BOOL_VAL(file -> file != NULL);
	
	return pack;
}

static NativePack fileClose(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	if(file -> file != NULL) {
		ValueContainer location;
		
		getField(instance, locationField, location);
		
		ObjString* loc = VALUE_STRING(location.value);
		
		if(loc == deniedStdin || loc == deniedStdout || loc == deniedStderr) {
			pack.value = BOOL_VAL(true);
			
			file -> file = NULL;
			
			fileOpened--;
		} else {
			bool closed = fclose(file -> file) != EOF;
			
			if(closed) {
				file -> file = NULL;
				
				fileOpened--;
			}
				
			pack.value = BOOL_VAL(closed);
		}
	} else pack.value = BOOL_VAL(false);
		
	return pack;
}

static NativePack fileFlush(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	if(file -> file != NULL) {
		int value = fflush(file -> file);
	
		pack.value = BOOL_VAL(value == 0);
	} else pack.value = BOOL_VAL(false);
	
	return pack;
}

static NativePack fileReadLine(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	bool ignoreNewline = false;
	
	if(argCount >= 2) {
		if(!IS_BOOL(values[1])) {
			NATIVE_R_ERR("Expected the first parameter to be a boolean!");
		}
		
		ignoreNewline = VALUE_BOOL(values[1]);
	}

	// To handle segmentation fault in POSIX OS's.

	if(file -> file == NULL) {
		pack.value = NUMBER_VAL(EOF);

		return pack;
	}
	
	char* line = NULL;
	size_t len = 0;
	
	int status = getline(&line, &len, file -> file);
	
	if(status != EOF) {
		if(ignoreNewline && status) {
			line[status - 1u] = 0;
			
			status--;
		}
		
		ObjString* string = TAKE_STRING(line, status, true);
		
		pack.value = OBJECT_VAL(string);
	} else pack.value = NUMBER_VAL((double) status);
	
	return pack;
}

static NativePack fileOpen(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	ValueContainer valueContainer;
	
	getField(instance, locationField, valueContainer);
	
	ObjString* location = VALUE_STRING(valueContainer.value);
	
	if(argCount < 3) {
		NATIVE_R_ERR("Too few arguments to call open(location, mode)!");
	}
	
	if(!IS_STRING(values[1])) {
		NATIVE_R_ERR("Expected parameter 1 (location) to be a string!");
	}
	
	if(!IS_NUMBER(values[2])) {
		NATIVE_R_ERR("Expedted parameter 2 to be a valid mode!");
	}
	
	setField(instance, locationField, values[1]);
	setField(instance, modeField, values[2]);
	setField(instance, sizeField, NUMBER_VAL(-1));

	int modeType = (int) VALUE_NUMBER(values[2]);
	
	if(modeType > 11 || modeType < 0) {
		NATIVE_R_ERR("Unrecognized file mode!");
	}
	
	char* mode = fileModes[modeType];
	
	if(file -> file != NULL && location != deniedStdin && location != deniedStdout
		&& location != deniedStderr) {
		bool closed = fclose(file -> file) != EOF;
		
		if(closed) 
			fileOpened--;
	}
	
	file -> file = fopen(VALUE_CSTRING(values[1]), mode);
	
	bool opened = file -> file != NULL;
	
	if(opened) 
		fileOpened++;
	
	pack.value = BOOL_VAL(opened);
	
	return pack;
}

static NativePack fileReopen(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	ValueContainer location;
	
	getField(instance, locationField, location);
	
	ObjString* loc = VALUE_STRING(location.value);
	
	if(loc == deniedStdin) {
		if(file -> file == NULL) 
			fileOpened++;
		
		file -> file = stdin;
		
		pack.value = BOOL_VAL(true);
		
		return pack;
	}
	else if(loc == deniedStderr) {
		if(file -> file == NULL) 
			fileOpened++;
		
		file -> file = stderr;
		
		pack.value = BOOL_VAL(true);
		
		return pack;
	}
	else if(loc == deniedStdout) {
		if(file -> file == NULL) 
			fileOpened++;
		
		file -> file = stdout;
		
		pack.value = BOOL_VAL(true);
		
		return pack;
	}
	
	char* mode;
	
	if(argCount < 2) {
		ValueContainer modeType;
		
		getField(instance, modeField, modeType);
		
		mode = fileModes[(int) VALUE_NUMBER(modeType.value)];
	} else {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first parameter to be a valid mode!");
		}
		
		int modeType = (int) VALUE_NUMBER(values[1]);
		
		if(modeType < 0 || modeType > 11) {
			NATIVE_R_ERR("Unrecognized file mode!");
		}
		
		mode = fileModes[modeType];
	}
	
	if(file -> file != NULL && !fclose(file -> file)) 
		fileOpened--;
	
	file -> file = fopen(VALUE_CSTRING(location.value), mode);
	
	bool opened = file -> file != NULL;
	
	if(opened) 
		fileOpened++;
	
	pack.value = BOOL_VAL(opened);
	
	return pack;
}

static NativePack fileEOF(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	pack.value = BOOL_VAL(file -> file != NULL ? feof(file -> file) != 0 : false);
	
	return pack;
}

// Work in progress.

static NativePack fileByteList(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	ObjByteList* byteList = newByteList(vm);
	
	if(argCount < 2) {
		pack.value = OBJECT_VAL(byteList);
		
		return pack;
	}
	
	if(!IS_LIST(values[1])) {
		NATIVE_R_ERR("Expected the first parameter to be a list!");
	}
	
	ObjList* list = VALUE_LIST(values[1]);
	
	
	byteList -> size = list -> count;
	
	byteList -> bytes = malloc(list -> count);
	
	if(byteList == NULL) {
		NATIVE_R_ERR("Could not allocate memory for bytelist!");
	}
	
	for(register size_t i = 0; i < list -> count; i++) {
		if(!IS_NUMBER(list -> values[i])) {
			NATIVE_R_ERR("List value other than number cannot be converted to bytearray!");
		}
		
		double number = VALUE_NUMBER(list -> values[i]);
		
		if(number < 0 || number > 255) {
			NATIVE_R_ERR("The number range should be in range from 0 to 255, found %g at index %zu!", number, i);
		}
		
		byteList -> bytes[i] = (uint8_t) number;
	}
	
	pack.value = OBJECT_VAL(byteList);
	
	return pack;
}

static NativePack fileError(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	pack.value = BOOL_VAL(file -> file != NULL ? (bool) ferror(file -> file) : false);
	
	return pack;
}

static NativePack fileClearError(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;

	// Preventing segmentation fault in POSIX OS's.

	if(file -> file != NULL) 
		clearerr(file -> file);
	
	return pack;
}

static NativePack fileReadc(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;

	if(file -> file == NULL) {
		pack.value = NUMBER_VAL(EOF);

		return pack;
	}
	
	char* buffer = ALLOCATE(char, 2);
	
	int result = fgetc(file -> file);
	
	if(result == EOF) {
		pack.value = NUMBER_VAL(EOF);
		
		return pack;
	}
	
	buffer[0] = (char) result;
	
	buffer[1] = 0;
	
	pack.value = OBJECT_VAL(TAKE_STRING(buffer, 1u, true));
	
	return pack;
}

static NativePack fileRead(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call File.read(bytes)!");
	}
	
	if(!IS_NUMBER(values[1])) {
		NATIVE_R_ERR("Expected the first argument to be a number in File.read(bytes)!");
	}
	
	ObjByteList* byteList = newByteList(vm);
	
	double size = VALUE_NUMBER(values[1]);
	
	if(size < 0) {
		NATIVE_R_ERR("Expected a positive number as an argument in File.read(bytes)!");
	}

	if(file -> file == NULL) {
		pack.value = OBJECT_VAL(byteList);

		return pack;
	}
	
	uint8_t* buffer = (uint8_t*) malloc((int) size);
	
	size_t rsize = fread(buffer, 1u, (size_t) size, file -> file);
	
	// byteList -> capacity = (size_t) size;
	
	byteList -> bytes = buffer;
	byteList -> size  = rsize;
	
	pack.value = OBJECT_VAL(byteList);
	
	return pack;
}

static NativePack _fileOpened(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	pack.value = NUMBER_VAL((double) fileOpened);
	
	return pack;
}

static NativePack fileReadRest(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	if(file -> file == NULL) {
		pack.value = NUMBER_VAL(EOF);
		
		return pack;
	}
	
	char ch;
	
	if((ch = fgetc(file -> file)) == EOF) {
		pack.value = NUMBER_VAL(EOF);
		
		return pack;
	} else ungetc(ch, file -> file);
	
	fpos_t pos;
	
	fgetpos(file -> file, &pos);
	
	long previousOffset = ftell(file -> file);
	
	fseek(file -> file, 0L, SEEK_END);
	
	long size = ftell(file -> file) - previousOffset;
	
	fsetpos(file -> file, &pos);		// Back to the previous position.
	
	char* buffer = ALLOCATE(char, size + 1);
	
	fread(buffer, 1u, size, file -> file);
	
	buffer[size] = 0;
	
	pack.value = OBJECT_VAL(TAKE_STRING(buffer, size, true));
	
	return pack;
}

static long calcSize(FILE* file, long pos) {
	bool reset = pos < 0;
	
	if(reset) 
		pos = ftell(file);
	
	fseek(file, 0L, SEEK_END);
	
	long size = ftell(file);
	
	if(reset) 
		fseek(file, pos, SEEK_SET);
	
	return size;
}

static NativePack fileReadAll(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	if(file -> file == NULL) {
		pack.value = NUMBER_VAL(EOF);
		
		return pack;
	}
	
	long pos;
	
	pos = ftell(file -> file);
	
	ValueContainer siz;
	
	getField(instance, sizeField, siz);
	
	long size = (long) VALUE_NUMBER(siz.value);
	
	if(size < 0) {
		size = calcSize(file -> file, pos);
		
		setField(instance, sizeField, NUMBER_VAL((double) size));
	}
	
	char* buffer = ALLOCATE(char, size + 1);
	
	rewind(file -> file);
	
	fread(buffer, 1u, size, file -> file);
	
	buffer[size] = 0;
	
	fseek(file -> file, pos, SEEK_SET);		// Roll back to the previous position.
	
	pack.value = OBJECT_VAL(TAKE_STRING(buffer, size, true));
	
	return pack;
}

static NativePack fileRewind(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	if(file -> file != NULL) 
		rewind(file -> file);
	
	return pack;
}

static NativePack fileTell(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	pack.value = NUMBER_VAL(file -> file != NULL ? (double) ftell(file -> file) : -1);
	
	return pack;
}

static NativePack fileSize(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;

	if(file -> file == NULL) {
		pack.value = NUMBER_VAL(0);

		return pack;
	}
	
	ValueContainer valueContainer;
	
	getField(instance, sizeField, valueContainer);
	
	double size = VALUE_NUMBER(valueContainer.value);
	
	if(size < 0) {
		if(file -> file == NULL) 
			pack.value = NUMBER_VAL(-1);
		else {
			size = (double) calcSize(file -> file, -1);
			
			Value r = NUMBER_VAL(size);
			
			setField(instance, sizeField, r);
			
			pack.value = r;
		}
		
		return pack;
	} pack.value = NUMBER_VAL(size);
	
	return pack;
}

static NativePack fileSeek(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	fileInstanceFile;
	
	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call File.seek()!");
	}
	
	long offset;
	
	if(!IS_NUMBER(values[1])) {
		NATIVE_R_ERR("Expected the first argument to be a number in File.seek()!");
	}
	
	offset = (long) VALUE_NUMBER(values[1]);
	
	int origin = SEEK_CUR;
	
	if(argCount > 2) {
		if(!IS_NUMBER(values[2])) {
			NATIVE_R_ERR("Expected the second argument to be a number in File.seek()!");
		}
		
		origin = (int) VALUE_NUMBER(values[2]);
	}

	if(file -> file == NULL) {
		pack.value = BOOL_VAL(false);

		return pack;
	}
	
	ValueContainer siz;
	
	getField(instance, sizeField, siz);
	
	long size = (long) VALUE_NUMBER(siz.value);
	
	if(size < 0) {
		size = calcSize(file -> file, -1);
		
		setField(instance, sizeField, NUMBER_VAL((double) size));
	}
	
	long totalOffset;
	
	switch(origin) {
		case SEEK_SET: 
			totalOffset = offset;
			break;
		
		case SEEK_CUR: 
			totalOffset = ftell(file -> file) + offset;
			break;
		
		case SEEK_END: 
			totalOffset = size;
			break;
	}
	
	if(totalOffset > size) 
		pack.value = BOOL_VAL(false);
	else pack.value = BOOL_VAL(fseek(file -> file, offset, origin) == 0);
	
	return pack;
}

static NativePack _fileReadAll(VM* vm, int argCount, Value* values) {
	initNativePack;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call File::read_all(filename)!");
	}

	if(!IS_STRING(values[1])) {
		NATIVE_R_ERR("Expected the first argument to be a string in File::read_all(filename)!");
	}

	ObjString* string = VALUE_STRING(values[1]);

	FILE* file = fopen(string -> buffer, "r");

	if(file == NULL) {
		pack.value = OBJECT_VAL(TAKE_STRING("", 0u, false));

		return pack;
	}

	fseek(file, 0L, SEEK_END);

	size_t size = ftell(file);

	rewind(file);

	char* buffer = ALLOCATE(char, size + 1);

	if(buffer == NULL) {
		pack.value = OBJECT_VAL(TAKE_STRING("", 0u, false));

		return pack;
	}

	(void) fread(buffer, 1u, size, file);

	ObjString* buff = TAKE_STRING(buffer, size, true);

	pack.value = OBJECT_VAL(buff);

	return pack;
}

static NativePack file__represent__(VM* vm, int argCount, Value* values) {
	initNativePack;

	fileInstanceFile;

	char result[100];

	char* modes[] = { "READ", "WRITE", "HYPER_READ", "HYPER_WRITE", "APPEND", "HYPER_APPEND" };

	ValueContainer mode;

	getField(instance, modeField, mode);

	short midx = (short) VALUE_NUMBER(mode.value);

	if(file -> file != NULL) 
		sprintf(result, "<File with %s (%s) mode at 0x%.16X>", modes[midx], fileModes[midx], file -> file);
	else sprintf(result, "<File closed>");

	size_t length = strlen(result);

	char* buffer = ALLOCATE(char, length + 1u);

	memcpy(buffer, result, length * sizeof(char));

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, length, true));

	return pack;
}

void initFileLib(VM* vm) {
	fileField     = TAKE_STRING("__file__", 8u, false);
	modeField     = TAKE_STRING("__mode__", 8u, false);
	locationField = TAKE_STRING("__location__", 8u, false);
	sizeField     = TAKE_STRING("__size__", 8u, false);
	deniedStdin   = TAKE_STRING("~~DENIED~~../86450D", 19u, false);
	deniedStdout  = TAKE_STRING("~~DENIED~~../76445H", 19u, false);
	deniedStderr  = TAKE_STRING("~~DENIED~~../34713N", 19u, false);
	
	ObjString* name = TAKE_STRING("File", 4u, false);
	
	fileClass = newClass(vm, name);
	
	setStatic(fileClass, TAKE_STRING("MODE_READ", 9u, false), NUMBER_VAL(0));
	setStatic(fileClass, TAKE_STRING("MODE_WRITE", 10u, false), NUMBER_VAL(1));
	setStatic(fileClass, TAKE_STRING("MODE_HYPER_READ", 15u, false), NUMBER_VAL(2));
	setStatic(fileClass, TAKE_STRING("MODE_HYPER_WRITE", 16u, false), NUMBER_VAL(3));
	setStatic(fileClass, TAKE_STRING("MODE_APPEND", 11u, false), NUMBER_VAL(4));
	setStatic(fileClass, TAKE_STRING("MODE_HYPER_APPEND", 17u, false), NUMBER_VAL(5));
	setStatic(fileClass, TAKE_STRING("SEEK_TOP", 8u, false), NUMBER_VAL(SEEK_SET));
	setStatic(fileClass, TAKE_STRING("SEEK_CUR", 8u, false), NUMBER_VAL(SEEK_CUR));
	setStatic(fileClass, TAKE_STRING("SEEK_END", 8u, false), NUMBER_VAL(SEEK_END));
	setStatic(fileClass, TAKE_STRING("MAX_OPENABLE", 12u, false), NUMBER_VAL(FOPEN_MAX));
	setStatic(fileClass, TAKE_STRING("EOF", 3u, false), NUMBER_VAL(EOF));
	
	ObjNative* method;
	
	defineStaticMethod(fileClass, TAKE_STRING("bytelist", 8u, false), fileByteList);
	defineStaticMethod(fileClass, TAKE_STRING("opened", 6u, false), _fileOpened);
	defineStaticMethod(fileClass, TAKE_STRING("read_all", 8u, false), _fileReadAll);
	
	defineMethod(fileClass, TAKE_STRING("init", 4u, false), fileInit);
	defineMethod(fileClass, TAKE_STRING("write", 5u, false), fileWrite);
	defineMethod(fileClass, TAKE_STRING("write_line", 10u, false), fileWriteLine);
	defineMethod(fileClass, TAKE_STRING("close", 5u, false), fileClose);
	defineMethod(fileClass, TAKE_STRING("is_opened", 9u, false), fileIsOpened);
	defineMethod(fileClass, TAKE_STRING("flush", 5u, false), fileFlush);
	defineMethod(fileClass, TAKE_STRING("open", 4u, false), fileOpen);
	defineMethod(fileClass, TAKE_STRING("reopen", 6u, false), fileReopen);
	defineMethod(fileClass, TAKE_STRING("read_line", 9u, false), fileReadLine);
	defineMethod(fileClass, TAKE_STRING("read_rest", 9u, false), fileReadRest);
	defineMethod(fileClass, TAKE_STRING("read_all", 8u, false), fileReadAll);
	defineMethod(fileClass, TAKE_STRING("readc", 5u, false), fileReadc);
	defineMethod(fileClass, TAKE_STRING("read", 4u, false), fileRead);
	defineMethod(fileClass, TAKE_STRING("eof", 3u, false), fileEOF);
	defineMethod(fileClass, TAKE_STRING("error", 5u, false), fileError);
	defineMethod(fileClass, TAKE_STRING("clear_error", 11u, false), fileClearError);
	defineMethod(fileClass, TAKE_STRING("tell", 4u, false), fileTell);
	defineMethod(fileClass, TAKE_STRING("size", 4u, false), fileSize);
	defineMethod(fileClass, TAKE_STRING("rewind", 6u, false), fileRewind);
	defineMethod(fileClass, TAKE_STRING("seek", 4u, false), fileSeek);
	defineMethod(fileClass, TAKE_STRING("__represent__", 13u, false), file__represent__);
	
	tableInsert(&vm -> globals, name, ((ValueContainer) { OBJECT_VAL(fileClass), true }));
}

static NativePack systemExit(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount == 1) {
		// Clean everyting up at exit.
		
		freeVM(vm);
		
		exit(EXIT_SUCCESS);
	}
	else if(argCount == 2) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the status to be a number in System::exit(status)!");
		}
		
		int status = (int) VALUE_NUMBER(values[1]);
		
		freeVM(vm);
		
		exit(status);
	}
	
	// No need to write rest of the code, as the process will terminate.
}

static NativePack systemPause(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount > 1) {
		printValue(values + 1);
	}
	
	getch();
	
	if(argCount > 1) puts("");
	
	return pack;
}

static NativePack systemRename(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 3) {
		NATIVE_R_ERR("Too few arguments to call System::rename()!");
	}
	
	if(!IS_STRING(values[1]) || !IS_STRING(values[2])) {
		NATIVE_R_ERR("Expected the arguments to be string in System::rename()!");
	}
	
	int result = rename(VALUE_CSTRING(values[1]), VALUE_CSTRING(values[2]));
	
	pack.value = BOOL_VAL(result == 0);
	
	return pack;
}

static NativePack systemRemove(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call System::rename(filename)!");
	}
	
	if(!IS_STRING(values[1])) {
		NATIVE_R_ERR("Expected the first arguments to be a string in System::rename(filename)!");
	}
	
	int result = remove(VALUE_CSTRING(values[1]));
	
	pack.value = BOOL_VAL(result == 0);
	
	return pack;
}

static NativePack systemPrintError(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount > 1) {
		if(!IS_STRING(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a string!");
		}
		
		perror(VALUE_CSTRING(values[1]));
	} else perror("");
	
	return pack;
}

static NativePack systemGC(VM* vm, int argCount, Value* values) {
	initNativePack;

	garbageCollector();

	return pack;
}

extern ObjFile* vm_stdin;
extern ObjFile* vm_stdout;

void initSystemLib(VM* vm) {
	ObjString* name = TAKE_STRING("System", 6u, false);
	
	ObjClass* systemClass = newClass(vm, name);
	
	ObjNative* method;
	
	defineStaticMethod(systemClass, TAKE_STRING("exit", 4u, false), systemExit);
	defineStaticMethod(systemClass, TAKE_STRING("rename", 6u, false), systemRename);
	defineStaticMethod(systemClass, TAKE_STRING("remove", 6u, false), systemRemove);
	defineStaticMethod(systemClass, TAKE_STRING("print_error", 11u, false), systemPrintError);
	defineStaticMethod(systemClass, TAKE_STRING("gc", 2u, false), systemGC);
	defineStaticMethod(systemClass, TAKE_STRING("pause", 5u, false), systemPause);
	
	ObjInstance* systemStdin  = newInstance(vm, fileClass);
	ObjFile* fileStdin = newFile(vm);
	
	fileStdin -> file = stdin;
	
	// Set the vm_stdin global.
	
	vm_stdin = fileStdin;
	
	setField(systemStdin, fileField, OBJECT_VAL(fileStdin));
	setField(systemStdin, modeField, NUMBER_VAL(0));
	setField(systemStdin, locationField, OBJECT_VAL(deniedStdin));
	
	ObjInstance* systemStdout = newInstance(vm, fileClass);
	ObjFile* fileStdout = newFile(vm);
	
	fileStdout -> file = stdout;
	
	vm_stdout = fileStdout;
	
	setField(systemStdout, fileField, OBJECT_VAL(fileStdout));
	setField(systemStdout, modeField, NUMBER_VAL(1));
	setField(systemStdout, locationField, OBJECT_VAL(deniedStdout));
	
	ObjInstance* systemStderr = newInstance(vm, fileClass);
	ObjFile* fileStderr = newFile(vm);
	
	fileStderr -> file = stderr;
	
	setField(systemStderr, fileField, OBJECT_VAL(fileStderr));
	setField(systemStderr, modeField, NUMBER_VAL(1));
	setField(systemStderr, locationField, OBJECT_VAL(deniedStderr));
	
	fileOpened += 3u;
	
	setStatic(systemClass, TAKE_STRING("stdin", 5u, false), OBJECT_VAL(systemStdin));
	setStatic(systemClass, TAKE_STRING("stdout", 6u, false), OBJECT_VAL(systemStdout));
	setStatic(systemClass, TAKE_STRING("stderr", 6u, false), OBJECT_VAL(systemStderr));
	setStatic(systemClass, TAKE_STRING("EXIT_SUCCESS", 12u, false), NUMBER_VAL(EXIT_SUCCESS));
	setStatic(systemClass, TAKE_STRING("EXIT_FAILURE", 12u, false), NUMBER_VAL(EXIT_FAILURE));
	
	tableInsert(&vm -> globals, name, ((ValueContainer) { OBJECT_VAL(systemClass), true }));
}

// Trigonometric.

static NativePack mathCos(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::cos(radian)!");
		}
	}
	
	pack.value = NUMBER_VAL(cos(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathSin(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::sin(radian)!");
		}
	}
	
	pack.value = NUMBER_VAL(sin(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathTan(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::tan(radian)!");
		}
	}
	
	pack.value = NUMBER_VAL(tan(VALUE_NUMBER(values[1])));
	
	return pack;
}

// arc-cos.

static NativePack mathAcos(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::acos(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(acos(VALUE_NUMBER(values[1])));
	
	return pack;
}

// arc-sin.

static NativePack mathAsin(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::asin(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(asin(VALUE_NUMBER(values[1])));
	
	return pack;
}

// arc-tan

static NativePack mathAtan(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::atan(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(atan(VALUE_NUMBER(values[1])));
	
	return pack;
}

// arc-tan2 (y, x) -> tan^-1 (y/x) in plane 2D cartesian co-ordinate system.

static NativePack mathAtan2(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount < 3) {
		NATIVE_R_ERR("Too few arguments to call Math::atan2(y, x)!");
	}
	
	if(!IS_NUMBER(values[1]) || !IS_NUMBER(values[2])) {
		NATIVE_R_ERR("Expected the first 2 arguments to be a number in Math::atan2(y, x)!");
	}
	
	pack.value = NUMBER_VAL(atan2(VALUE_NUMBER(values[1]), VALUE_NUMBER(values[2])));
	
	return pack;
}

// Hyperbolic trigonometry.

static NativePack mathCosh(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::cosh(radian)!");
		}
	}
	
	pack.value = NUMBER_VAL(cosh(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathSinh(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::sin(radian)!");
		}
	}
	
	pack.value = NUMBER_VAL(sinh(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathTanh(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::tanh(radian)!");
		}
	}
	
	pack.value = NUMBER_VAL(tanh(VALUE_NUMBER(values[1])));
	
	return pack;
}

// Arc.

static NativePack mathAcosh(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::acosh(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(acosh(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathAsinh(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::asinh(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(asinh(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathAtanh(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::atanh(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(atanh(VALUE_NUMBER(values[1])));
	
	return pack;
}

// Exponentials.

static NativePack mathExp(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::exp(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(exp(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathExp2(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::exp2(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(exp2(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathExpm1(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::expm1(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(expm1(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathFrexp(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	ObjDictionary* dict = newDictionary(vm);
	
	tableInsert(&dict -> fields, TAKE_STRING("sig", 3u, false), (ValueContainer) { NUMBER_VAL(NAN), false });
	tableInsert(&dict -> fields, TAKE_STRING("exp", 3u, false), (ValueContainer) { NUMBER_VAL(NAN), false });
	
	pack.value = OBJECT_VAL(dict);
	
	if(argCount < 2) 
		return pack;
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::frexp(number)!");
		}
	}
	
	int exp;
	
	double result = frexp(VALUE_NUMBER(values[1]), &exp);
	
	tableInsert(&dict -> fields, TAKE_STRING("sig", 3u, false), (ValueContainer) { NUMBER_VAL(result), false });
	tableInsert(&dict -> fields, TAKE_STRING("exp", 3u, false), (ValueContainer) { NUMBER_VAL((double) exp), false });
	
	return pack;
}

static NativePack mathLdexp(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount < 3) {
		NATIVE_R_ERR("Too few arguments to call Math::ldexp(significand, exponent)!");
	}
	
	if(!IS_NUMBER(values[1]) || !IS_NUMBER(values[2])) {
		NATIVE_R_ERR("Expected the first 2 arguments to be a number in Math::ldexp(significand, exponent)!");
	}
	
	pack.value = NUMBER_VAL(ldexp(VALUE_NUMBER(values[1]), VALUE_NUMBER(values[2])));
	
	return pack;
}

static NativePack mathLog(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::log(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(log(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathLog10(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::log10(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(log10(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathLog1p(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::log1p(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(log1p(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathLog2(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::log2(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(log2(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathBlog(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount < 3) {
		NATIVE_R_ERR("Too few arguments to call Math::blog(base, x)!");
	}
	
	if(!IS_NUMBER(values[1]) || !IS_NUMBER(values[2])) {
		NATIVE_R_ERR("Expected the first 2 arguments to be a number in Math::blog(base, x)!");
	}
	
	pack.value = NUMBER_VAL(log(VALUE_NUMBER(values[2])) / log(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathModf(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	ObjDictionary* dict = newDictionary(vm);
	
	tableInsert(&dict -> fields, TAKE_STRING("wnum", 4u, false), (ValueContainer) { NUMBER_VAL(NAN), false });
	tableInsert(&dict -> fields, TAKE_STRING("frac", 4u, false), (ValueContainer) { NUMBER_VAL(NAN), false });
	
	pack.value = OBJECT_VAL(dict);
	
	if(argCount < 2) 
		return pack;
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::modf(number)!");
		}
	}
	
	double wnum;
	
	double frac = modf(VALUE_NUMBER(values[1]), &wnum);
	
	tableInsert(&dict -> fields, TAKE_STRING("wnum", 4u, false), (ValueContainer) { NUMBER_VAL(wnum), false });
	tableInsert(&dict -> fields, TAKE_STRING("frac", 4u, false), (ValueContainer) { NUMBER_VAL(frac), false });
	
	return pack;
}

// Power functions.

static NativePack mathPow(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount < 3) {
		NATIVE_R_ERR("Too few arguments to call Math::pow(base, exponent)!");
	}
	
	if(!IS_NUMBER(values[1]) || !IS_NUMBER(values[2])) {
		NATIVE_R_ERR("Expected the first 2 arguments to be a number in Math::pow(base, exponent)!");
	}
	
	pack.value = NUMBER_VAL(pow(VALUE_NUMBER(values[1]), VALUE_NUMBER(values[2])));
	
	return pack;
}

static NativePack mathSqrt(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::sqrt(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(sqrt(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathCbrt(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::cbrt(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(cbrt(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathHypot(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount < 3) {
		NATIVE_R_ERR("Too few arguments to call Math::hypot(x, y)!");
	}
	
	if(!IS_NUMBER(values[1]) || !IS_NUMBER(values[2])) {
		NATIVE_R_ERR("Expected the first 2 arguments to be a number in Math::hypot(x, y)!");
	}
	
	pack.value = NUMBER_VAL(hypot(VALUE_NUMBER(values[1]), VALUE_NUMBER(values[2])));
	
	return pack;
}

// Error and Gamma functions.

static NativePack mathErf(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::erf(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(erf(VALUE_NUMBER(values[1])));
	
	return pack;
}

// Complementary error function.

static NativePack mathErfc(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::erfc(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(erfc(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathTgamma(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::tgamma(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(tgamma(VALUE_NUMBER(values[1])));
	
	return pack;
}

// Calculates natural log of the result of gamma function.

static NativePack mathLgamma(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::lgamma(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(lgamma(VALUE_NUMBER(values[1])));
	
	return pack;
}

// Rounding and Remainders.

static NativePack mathCeil(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::ceil(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(ceil(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathFloor(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::floor(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(floor(VALUE_NUMBER(values[1])));
	
	return pack;
}

// Truncates the value.
// 4.53452546 -> 4 (0.53452546 is truncated).

static NativePack mathTrunc(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::trunc(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(trunc(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathRound(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::round(number)!");
		}
	}
	
	pack.value = NUMBER_VAL(round(VALUE_NUMBER(values[1])));
	
	return pack;
}

// Returns floating-point remainder of numer | denom (rounded to nearest).
// remainder = numer - (numer | denom) * denom.

static NativePack mathRemainder(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount < 3) {
		NATIVE_R_ERR("Too few arguments to call Math::remainder(numerator, denominator)!");
	}
	
	if(!IS_NUMBER(values[1]) || !IS_NUMBER(values[2])) {
		NATIVE_R_ERR("Expected the first 2 arguments to be a number in Math::remainder(numerator, denominator)!");
	}
	
	pack.value = NUMBER_VAL(remainder(VALUE_NUMBER(values[1]), VALUE_NUMBER(values[2])));
	
	return pack;
}

// Minimum, Maximum and Difference.
// Returns x - y, if x > y otherwise 0.

static NativePack mathDim(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount < 3) {
		NATIVE_R_ERR("Too few arguments to call Math::dim(x, y)!");
	}
	
	if(!IS_NUMBER(values[1]) || !IS_NUMBER(values[2])) {
		NATIVE_R_ERR("Expected the first 2 arguments to be a number in Math::remainder(x, y)!");
	}
	
	pack.value = NUMBER_VAL(fdim(VALUE_NUMBER(values[1]), VALUE_NUMBER(values[2])));
	
	return pack;
}

static NativePack mathMin(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(INFINITY);
		return pack;
	}
	
	double min = VALUE_NUMBER(values[1]);
	
	double temp;
	
	for(register int i = 2; i < argCount; i++) {
		if((temp = VALUE_NUMBER(values[i])) < min) 
			min = temp;
	}
	
	pack.value = NUMBER_VAL(min);
	
	return pack;
}

static NativePack mathMax(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(-INFINITY);
		return pack;
	}
	
	double max = VALUE_NUMBER(values[1]);
	
	double temp;
	
	for(register int i = 2; i < argCount; i++) {
		if((temp = VALUE_NUMBER(values[i])) > max) 
			max = temp;
	}
	
	pack.value = NUMBER_VAL(max);
	
	return pack;
}

// Returns absolute value of x.

static NativePack mathAbs(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	if(argCount < 2) {
		pack.value = NUMBER_VAL(NAN);
		return pack;
	}
	else if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Math::abs(x)!");
		}
	}
	
	pack.value = NUMBER_VAL(fabs(VALUE_NUMBER(values[1])));
	
	return pack;
}

static NativePack mathRandom(VM* vm, int argCount, Value* values) {
	initNativePack;

	pack.value = NUMBER_VAL((double) rand() / (double) RAND_MAX);

	return pack;
}

void initMathLib(VM* vm) {
	ObjString* name = TAKE_STRING("Math", 4u, false);
	
	ObjClass* mathClass = newClass(vm, name);
	
	ObjNative* method;
	
	defineStaticMethod(mathClass, TAKE_STRING("cos", 3u, false), mathCos);
	defineStaticMethod(mathClass, TAKE_STRING("sin", 3u, false), mathSin);
	defineStaticMethod(mathClass, TAKE_STRING("tan", 3u, false), mathTan);
	defineStaticMethod(mathClass, TAKE_STRING("acos", 4u, false), mathAcos);
	defineStaticMethod(mathClass, TAKE_STRING("asin", 4u, false), mathAsin);
	defineStaticMethod(mathClass, TAKE_STRING("atan", 4u, false), mathAtan);
	defineStaticMethod(mathClass, TAKE_STRING("atan2", 5u, false), mathAtan2);
	
	defineStaticMethod(mathClass, TAKE_STRING("cosh", 4u, false), mathCosh);
	defineStaticMethod(mathClass, TAKE_STRING("sinh", 4u, false), mathSinh);
	defineStaticMethod(mathClass, TAKE_STRING("tanh", 4u, false), mathTanh);
	defineStaticMethod(mathClass, TAKE_STRING("acosh", 5u, false), mathAcosh);
	defineStaticMethod(mathClass, TAKE_STRING("asinh", 5u, false), mathAsinh);
	defineStaticMethod(mathClass, TAKE_STRING("atanh", 5u, false), mathAtanh);
	
	defineStaticMethod(mathClass, TAKE_STRING("exp", 3u, false), mathExp);
	defineStaticMethod(mathClass, TAKE_STRING("exp2", 4u, false), mathExp2);
	defineStaticMethod(mathClass, TAKE_STRING("expm1", 5u, false), mathExpm1);
	defineStaticMethod(mathClass, TAKE_STRING("frexp", 5u, false), mathFrexp);
	defineStaticMethod(mathClass, TAKE_STRING("ldexp", 5u, false), mathLdexp);
	defineStaticMethod(mathClass, TAKE_STRING("log", 3u, false), mathLog);
	defineStaticMethod(mathClass, TAKE_STRING("log10", 5u, false), mathLog10);
	defineStaticMethod(mathClass, TAKE_STRING("log2", 4u, false), mathLog2);
	defineStaticMethod(mathClass, TAKE_STRING("log1p", 5u, false), mathLog1p);
	defineStaticMethod(mathClass, TAKE_STRING("blog", 4u, false), mathBlog);
	defineStaticMethod(mathClass, TAKE_STRING("modf", 4u, false), mathModf);
	
	defineStaticMethod(mathClass, TAKE_STRING("pow", 3u, false), mathPow);
	defineStaticMethod(mathClass, TAKE_STRING("sqrt", 4u, false), mathSqrt);
	defineStaticMethod(mathClass, TAKE_STRING("cbrt", 4u, false), mathCbrt);
	defineStaticMethod(mathClass, TAKE_STRING("hypot", 5u, false), mathHypot);
	
	defineStaticMethod(mathClass, TAKE_STRING("erf", 3u, false), mathErf);
	defineStaticMethod(mathClass, TAKE_STRING("erfc", 4u, false), mathErfc);
	defineStaticMethod(mathClass, TAKE_STRING("tgamma", 6u, false), mathTgamma);
	defineStaticMethod(mathClass, TAKE_STRING("lgamma", 6u, false), mathLgamma);
	
	defineStaticMethod(mathClass, TAKE_STRING("ceil", 4u, false), mathCeil);
	defineStaticMethod(mathClass, TAKE_STRING("floor", 5u, false), mathFloor);
	defineStaticMethod(mathClass, TAKE_STRING("trunc", 5u, false), mathTrunc);
	defineStaticMethod(mathClass, TAKE_STRING("round", 5u, false), mathRound);
	defineStaticMethod(mathClass, TAKE_STRING("remainder", 9u, false), mathRemainder);
	
	defineStaticMethod(mathClass, TAKE_STRING("dim", 3u, false), mathDim);
	defineStaticMethod(mathClass, TAKE_STRING("min", 3u, false), mathMin);
	defineStaticMethod(mathClass, TAKE_STRING("max", 3u, false), mathMax);
	
	defineStaticMethod(mathClass, TAKE_STRING("abs", 3u, false), mathAbs);

	defineStaticMethod(mathClass, TAKE_STRING("random", 6u, false), mathRandom);
	
	setStatic(mathClass, TAKE_STRING("PI", 2u, false), NUMBER_VAL(3.1415926535897931));
	setStatic(mathClass, TAKE_STRING("E", 1u, false), NUMBER_VAL(2.7182818284590451));
	setStatic(mathClass, TAKE_STRING("LN2", 2u, false), NUMBER_VAL(0.69314718055994529));
	setStatic(mathClass, TAKE_STRING("LN10", 2u, false), NUMBER_VAL(2.3025850929940459));
	
	tableInsert(&vm -> globals, name, ((ValueContainer) { OBJECT_VAL(mathClass), true }));
}

#define timeGet(instance, name, value) tableGet(&instance -> fields, name, &value)
#define timeSet(instance, name, value) tableInsert(&instance -> fields, name, (ValueContainer) { NUMBER_VAL((value)), false });

static ObjString* secField;
static ObjString* usecField;
static ObjString* tzoneField;

static ObjClass* timeClass;

#define timeInstanceTime if(!IS_INSTANCE(values[0]) || VALUE_INSTANCE(values[0]) -> klass != timeClass) {\
		NATIVE_R_ERR("Provided reciever/instance (this) must be of file instance!");\
	}\
	ObjInstance* instance = VALUE_INSTANCE(values[0]);\
	ValueContainer timeContainer;\
	getField(instance, secField, timeContainer);\
	double sec = VALUE_NUMBER(timeContainer.value);\
	getField(instance, usecField, timeContainer);\
	double usec = VALUE_NUMBER(timeContainer.value);\
	getField(instance, tzoneField, timeContainer);\
	double tzone = VALUE_NUMBER(timeContainer.value);

int gettzoffset(void) {
	int offset = 0;

#ifdef _WIN32
	TIME_ZONE_INFORMATION tzinfo;

	GetTimeZoneInformation(&tzinfo);

	offset = -(tzinfo.Bias * 60);
#else
	time_t rawtime = time(NULL);
	struct tm* bdt = localtime(&rawtime);

	offset = bdt -> __tm_gmtoff;
#endif // _WIN32

	return offset;
}

static bool isLeap(int year) {
	if(!(year % 400) || !(year % 4) && (year % 100)) return true;
	
	return false;
}

static NativePack timeInit(VM* vm, int argCount, Value* values) {
	initNativePack;

	if(!IS_INSTANCE(values[0]) || VALUE_INSTANCE(values[0]) -> klass != timeClass) {
		NATIVE_R_ERR("Need a time instance to call Time.init(value...)!");
	}

	ObjInstance* instance = VALUE_INSTANCE(values[0]);
	
	if(argCount < 2) {
		// Create a new time instance of current time.

		struct timeval tv;

		gettimeofday(&tv, NULL);

		setField(instance, secField, NUMBER_VAL((double) tv.tv_sec));
		setField(instance, usecField, NUMBER_VAL((double) tv.tv_usec));
		setField(instance, tzoneField, NUMBER_VAL((double) gettzoffset()));
	}
	
	pack.value = values[0];
	
	return pack;
}

static int timeUnits[] = { 1, 1e3, 1e6, 1, 60, 3600, 86400, 
	604800, 2629746, 31556926 };

static NativePack timeGetYear(VM* vm, int argCount, Value* values) {
	initNativePack;

	timeInstanceTime;

	pack.value = NUMBER_VAL(((long long) (sec + tzone) / timeUnits[9]) + 1970);

	return pack;
}

static NativePack timeGetUTCYear(VM* vm, int argCount, Value* values) {
	initNativePack;

	timeInstanceTime;

	pack.value = NUMBER_VAL(((long long) sec / timeUnits[9]) + 1970);

	return pack;
}

static  NativePack timeSleep(VM* vm, int argCount, Value* values) {
}

// Gets the current CPU time.

static NativePack timeNow(VM* vm, int argCount, Value* values) {
	NativePack pack;
	
	pack.hadError = false;
	pack.value    = NUMBER_VAL((double) clock() / CLOCKS_PER_SEC);
	
	return pack;
}

static NativePack timeElapsed(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	struct timeval tv;
	
	gettimeofday(&tv, NULL);

	ObjDictionary* dict = newDictionary(vm);

	tableInsert(&dict -> fields, TAKE_STRING("sec", 3u, false), (ValueContainer) { NUMBER_VAL((double) tv.tv_sec), true });
	tableInsert(&dict -> fields, TAKE_STRING("usec", 4u, false), (ValueContainer) { NUMBER_VAL((double) tv.tv_usec), true });
	
	pack.value = OBJECT_VAL(dict);
	
	return pack;
}

static NativePack timeStringifyISO(VM* vm, int argCount, Value* values) {
	initNativePack;

	return pack;
}

static NativePack timeSetYear(VM* vm, int argCount, Value* values) {
	initNativePack;

	if(argCount < 2) {
		pack.value = NUMBER_VAL(0);

		return pack;
	}

	timeInstanceTime;

	double year = trunc(VALUE_NUMBER(values[1]));

	if(year < -1e5 || year > 1e5) {
		NATIVE_R_ERR("Invalid year provided in Time.set_year(year)! How far you intend to go?");
	}

	if(year >= 0 && year < 100) 
		year += 1900;
	
	int rest = (long long) sec % timeUnits[9];

	double ysec = (year - 1970) * timeUnits[9];

	pack.value = NUMBER_VAL((ysec - sec) * 1000 + ((long long) usec / 1000));

	setField(instance, secField, NUMBER_VAL(ysec + rest));

	return pack;
}

static NativePack timeSetUTCYear(VM* vm, int argCount, Value* values) {
	initNativePack;

	if(argCount < 2) {
		pack.value = NUMBER_VAL(0);

		return pack;
	}

	timeInstanceTime;

	double year = trunc(VALUE_NUMBER(values[1]));

	if(year < -1e5 || year > 1e5) {
		NATIVE_R_ERR("Invalid year provided in Time.set_utc_year(year)! How far you intend to go?");
	}

	if(year >= 0 && year < 100) 
		year += 1900;
	
	int rest = (long long) sec % timeUnits[9];

	double ysec = (year - 1970) * timeUnits[9];

	pack.value = NUMBER_VAL((ysec - sec) * 1000 + ((long long) usec / 1000));

	setField(instance, secField, NUMBER_VAL(ysec + rest));

	return pack;
}

static NativePack timeGetDate(VM* vm, int argCount, Value* values) {
	initNativePack;

	timeInstanceTime;
	
	int year = ((long long) (sec + tzone) / timeUnits[9]) + 1970;

	int rest = ((year - 1) / 400 - 1970 / 400) +
		((year - 1) / 100 - 1970 / 100);
	
	uint16_t days[] = { 31, 28 + isLeap(year), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	
	for(uint8_t i = 0u; i < 12u && rest > days[i]; i++, rest -= days[i]);

	pack.value = NUMBER_VAL(rest);

	return pack;
}

static NativePack timeCount(VM* vm, int argCount, Value* values) {
	initNativePack;

	timeInstanceTime;

	ObjDictionary* dict = newDictionary(vm);

	tableInsert(&dict -> fields, TAKE_STRING("sec", 3u, false), (ValueContainer) { NUMBER_VAL(sec), true });
	tableInsert(&dict -> fields, TAKE_STRING("usec", 4u, false), (ValueContainer) { NUMBER_VAL(usec), true });

	pack.value = OBJECT_VAL(dict);

	return pack;
}

void initTimeLib(VM* vm) {
	usecField   = TAKE_STRING("_p_usec__", 9u, false);
	secField    = TAKE_STRING("_p_sec__", 8u, false);
	tzoneField  = TAKE_STRING("_p_tzone__", 10u, false);
	
	ObjString* name = TAKE_STRING("Time", 4u, false);
	
	timeClass = newClass(vm, name);
	
	ObjNative* method;
	
	defineMethod(timeClass, vm -> initString, timeInit);

	defineMethod(timeClass, TAKE_STRING("get_year", 8u, false), timeGetYear);
	defineMethod(timeClass, TAKE_STRING("get_utc_year", 12u, false), timeGetUTCYear);
	defineMethod(timeClass, TAKE_STRING("set_year", 8u, false), timeSetYear);
	defineMethod(timeClass, TAKE_STRING("set_utc_year", 12u, false), timeSetYear);
	defineMethod(timeClass, TAKE_STRING("get_date", 8u, false), timeGetDate);
	defineMethod(timeClass, TAKE_STRING("count", 5u, false), timeCount);
	
	defineStaticMethod(timeClass, TAKE_STRING("sleep", 5u, false), timeSleep);
	defineStaticMethod(timeClass, TAKE_STRING("now", 3u, false), timeNow);
	defineStaticMethod(timeClass, TAKE_STRING("elapsed", 7u, false), timeElapsed);
	
	setStatic(timeClass, TAKE_STRING("NANOSECONDS", 11u, false), NUMBER_VAL(0));
	setStatic(timeClass, TAKE_STRING("MICROSECONDS", 12u, false), NUMBER_VAL(1));
	setStatic(timeClass, TAKE_STRING("MILLISECONDS", 12u, false), NUMBER_VAL(2));
	setStatic(timeClass, TAKE_STRING("SECONDS", 7u, false), NUMBER_VAL(3));
	setStatic(timeClass, TAKE_STRING("MINUTES", 7u, false), NUMBER_VAL(4));
	setStatic(timeClass, TAKE_STRING("HOURS", 5u, false), NUMBER_VAL(5));
	setStatic(timeClass, TAKE_STRING("DAYS", 4u, false), NUMBER_VAL(6));
	setStatic(timeClass, TAKE_STRING("WEEKS", 5u, false), NUMBER_VAL(7));
	setStatic(timeClass, TAKE_STRING("MONTHS", 6u, false), NUMBER_VAL(8));
	setStatic(timeClass, TAKE_STRING("YEARS", 5u, false), NUMBER_VAL(9));
	
	tableInsert(&vm -> globals, name, (ValueContainer) { OBJECT_VAL(timeClass), true });
}

static ObjString* numberField;

extern ObjClass* vmNumberClass;

#define numberInstanceNumber if(!IS_NUMBER(values[0]) && (!IS_INSTANCE(values[0]) || VALUE_INSTANCE(values[0]) -> klass != vmNumberClass)) {\
	NATIVE_R_ERR("Provided reciever/instance (this) must be number or instance of Number(number)!");\
}\
	double number;\
	if(IS_NUMBER(values[0])) number = VALUE_NUMBER(values[0]);\
	else {\
		ObjInstance* instance = VALUE_INSTANCE(values[0]);\
		ValueContainer numberContainer;\
		getField(instance, numberField, numberContainer);\
		number = VALUE_NUMBER(numberContainer.value);\
	}

static double toNumber(Value* value) {
	switch(value -> type) {
		case VAL_NUMBER:  return value -> as.number;
		case VAL_BOOLEAN: return (double) value -> as.boolean;
		case VAL_NULL:    return 0;
		case VAL_OBJECT: {
			switch(OBJ_TYPE(*value)) {
				case OBJ_STRING: return pstrtod(VALUE_STRING(*value) -> buffer);
			}

			return NAN;
		}
	}
}

static NativePack numberNumberify(VM* vm, int argCount, Value* values) {
	initNativePack;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call Number::numberify(value)!");
	}

	pack.value = NUMBER_VAL(toNumber(values + 1));

	return pack;
}

static NativePack numberInit(VM* vm, int argCount, Value* values) {
	initNativePack;

	double number = 0;

	if(argCount > 1) 
		number = toNumber(values + 1);
	
	ObjInstance* instance = VALUE_INSTANCE(values[0]);

	setField(instance, numberField, NUMBER_VAL(number));

	pack.value = values[0];

	return pack;
}

static NativePack numberToLocale(VM* vm, int argCount, Value* values) {
	initNativePack;

	numberInstanceNumber;

	// Check for NaN and Infinity.

	if(isinf(number)) {
		bool isneg = number < 0;

		pack.value = OBJECT_VAL(TAKE_STRING(isneg ? "-∞" : "∞", 1u + !isneg, false));

		return pack;
	}
	else if(isnan(number)) {
		pack.value = OBJECT_VAL(TAKE_STRING("NaN", 3u, false));

		return pack;
	}

	int precision = 3;

	if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number Number.to_locale(frac_precision)!");
		}

		precision = (int) VALUE_NUMBER(values[1]);

		if(precision < 0 || precision > 20) {
			NATIVE_R_ERR("Invalid precision provided in Number.to_locale(frac_precision)! Range should be [0 - 20]");
		}
	}

	double wnum;

	double frac = modf(number, &wnum);

	register unsigned long ifrac = (unsigned long) round(fabs(frac) * pow(10, precision));
	long long inum = (long long) fabs(wnum);

	// First identify the length of the wnum (Left side of the radix point).

	int wnum_length = inum != 0 ? ceil(log10(inum + 1)) : 1,
	    frac_length = ceil(log10(ifrac + 1));
	
	// The size of wnum and frac should be 20.
	// I don't want to loose fraction precision in locale string.

	if((wnum_length + frac_length) > 20) {
		ifrac /= (long long) pow(10, frac_length + wnum_length - 20);

		frac_length = 20 - wnum_length;
	}

	bool isfrac = ifrac > 0;

	// Then add the length of the fraction number as well as the number of commas we are gonna have.

	short separators = (wnum_length / 3) + (!(wnum_length % 3) ? -1 : 0);

	int length = wnum_length + frac_length + (isfrac ? 1 : 0) + separators + (number < 0);

	char* buffer = ALLOCATE(char, length + 1);

	if(buffer == NULL) {
		pack.value = OBJECT_VAL(TAKE_STRING("", 0u, false));

		return pack;
	}

	register short temp = length - 1;

	while(ifrac) {
		buffer[temp--] = (ifrac % 10) + '0';
		ifrac /= 10;
	}

	if(isfrac) 
		buffer[temp--] = '.';

	if(inum == 0) 
		buffer[temp--] = '0';
	else {
		register short count = 0;

		while(inum) {
			buffer[temp--] = (inum % 10) + '0';
			inum /= 10;

			if(count == 2) {
				if(separators) { buffer[temp--] = ','; count = 0; separators--; }
			} else count++;
		}
	}

	if(number < 0) 
		buffer[temp--] = '-';

	buffer[length] = 0;

	ObjString* string = TAKE_STRING(buffer, length, true);

	pack.value = OBJECT_VAL(string);

	return pack;
}

// Converts a number to Bangladeshi locale string.

static NativePack numberToLocaleBD(VM* vm, int argCount, Value* values) {
	initNativePack;

	// Code is almost same as the previous one.

	numberInstanceNumber;

	// Check for NaN and Infinity.

	if(isinf(number)) {
		bool isneg = number < 0;

		pack.value = OBJECT_VAL(TAKE_STRING(isneg ? "-∞" : "∞", 1u + !isneg, false));

		return pack;
	}
	else if(isnan(number)) {
		pack.value = OBJECT_VAL(TAKE_STRING("NaN", 3u, false));

		return pack;
	}

	int precision = 3;

	if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number Number.to_locale_bd(frac_precision)!");
		}

		precision = (int) VALUE_NUMBER(values[1]);

		if(precision < 0 || precision > 20) {
			NATIVE_R_ERR("Invalid precision provided in Number.to_locale_bd(frac_precision)! Range should be [0 - 20]");
		}
	}

	double wnum;

	double frac = modf(number, &wnum);

	register unsigned long ifrac = (unsigned long) round(fabs(frac) * pow(10, precision));
	long long inum = (long long) fabs(wnum);

	// First identify the length of the wnum (Left side of the radix point).

	int wnum_length = inum != 0 ? ceil(log10(inum + 1)) : 1,
	    frac_length = ceil(log10(ifrac + 1));
	
	// The size of wnum and frac should be 20.

	if((wnum_length + frac_length) > 20) {
		ifrac /= (long long) pow(10, frac_length + wnum_length - 20);

		frac_length = 20 - wnum_length;
	}

	bool isfrac = ifrac > 0;

	// Then add the length of the fraction number as well as the number of commas we are gonna have.
	// Number of commas gonna vary as Bangladeshi locale is different.
	// Consider : 1352274543657 to be converted to general locale string and bn locale string.
	// Locale String : 1,352,274,543,657
	// BD Locale String : 13,52,27,45,43,657  -> Commas repeat after 2 number excluding the first 3 numbers.

	short separators = wnum_length > 3 ? ((wnum_length - 3) / 2) + (!((wnum_length - 3) % 2) ? -1 : 0) + 1 : 0;

	int length = wnum_length + frac_length + (isfrac ? 1 : 0) + separators + (number < 0);

	char* buffer = ALLOCATE(char, length + 1);

	if(buffer == NULL) {
		pack.value = OBJECT_VAL(TAKE_STRING("", 0u, false));

		return pack;
	}

	register short temp = length - 1;

	while(ifrac) {
		buffer[temp--] = (ifrac % 10) + '0';
		ifrac /= 10;
	}

	if(isfrac) 
		buffer[temp--] = '.';

	if(inum == 0) 
		buffer[temp--] = '0';
	else {
		register short count = 0;
		register bool change = false;

		while(inum) {
			buffer[temp--] = (inum % 10) + '0';
			inum /= 10;

			if(separators) {
				if(!change && count == 2) { buffer[temp--] = ','; count = 0; separators--; change = true; }
				else if(change && count == 1) { buffer[temp--] = ','; count = 0; separators--; }
				else count++;
			} else count++;
		}
	}

	if(number < 0) 
		buffer[temp--] = '-';

	buffer[length] = 0;

	ObjString* string = TAKE_STRING(buffer, length, true);

	pack.value = OBJECT_VAL(string);

	return pack;
}

#define MAX_SAFE_INTEGER 9007199254740991
#define MIN_SAFE_INTEGER -9007199254740992

#define MAX_VALUE 1.7976931348623157e+308
#define MIN_VALUE 5e-324

static NativePack numberBlooper(VM* vm, int argCount, Value* vaules) {
	initNativePack;

	// You probably know who she is.

	ObjString* string = TAKE_STRING("I used to love Arpi Dey Puja!", 29u, false);

	pack.value = OBJECT_VAL(string);

	return pack;
}

static NativePack numberIsSafeInteger(VM* vm, int argCount, Value* values) {
	initNativePack;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call Number.is_safe_integer(number)!");
	}

	if(!IS_NUMBER(values[1])) {
		NATIVE_R_ERR("Expected the first argument to be a number in Number.is_safe_integer(number)!");
	}

	double number = VALUE_NUMBER(values[1]);

	pack.value = BOOL_VAL(number != NAN && fabs(number) != INFINITY && number >= MIN_SAFE_INTEGER && number <= MAX_SAFE_INTEGER);

	return pack;
}

static char* scientific(double number, short digits) {
	int exponent, sign;

	char* result = dtoa(number, DTOA_SHORTEST, 20, &exponent, &sign, NULL);

	size_t length = strlen(result);

	char* buffer = malloc(120u * sizeof(char));

	if(sign) buffer[0] = '-';

	char csign = exponent <= 0 ? '-' : '+';

	exponent = abs(exponent - 1);

	if(length > 1u) {
		if(digits != -1) {
			if(digits <= length - 1u) {
				if(digits == 0) sprintf(buffer + sign, "%.*sE%c%d", 1u, result, csign, exponent);
				else sprintf(buffer + sign, "%.*s.%.*sE%c%d", 1u, result, digits, result + 1u, csign, exponent);
			} else sprintf(buffer + sign, "%.*s.%s%0.*dE%c%d", 1u, result, result + 1u, digits + 1 - length, 0, csign, exponent);
		} else sprintf(buffer + sign, "%.*s.%sE%c%d", 1u, result, result + 1u, csign, exponent);
	} else {
		if(digits != -1 && digits != 0) sprintf(buffer + sign, "%s.%.*dE%c%d", result, digits, 0, csign, exponent);
		else sprintf(buffer + sign, "%sE%c%d", result, csign, exponent);
	}

	length = strlen(buffer);

	freedtoa(result);

	return buffer;
}

static NativePack numberScientific(VM* vm, int argCount, Value* values) {
	initNativePack;

	short digits = -1;

	if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Number.scientific(digits)!");
		}

		double value = VALUE_NUMBER(values[1]);

		if(value < 0 || value > 100) {
			NATIVE_R_ERR("Argument must be in range of [1,100] in Number.scientific(digits)!");
		}

		digits = (short) value;
	}

	numberInstanceNumber;

	char* result = scientific(number, digits);

	size_t length = strlen(result);

	char* buffer = ALLOCATE(char, length + 1u);

	memcpy(buffer, result, (length + 1u) * sizeof(char));

	free(result);

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, length, true));

	return pack;
}

// Returns the next greater double.
// Only to use in numberStringify. So cases for Infinity
// is omitted.

static double nextGreaterDouble(double number) {
	uint64_t d64;

	memcpy(&d64, &number, sizeof(double));
	
	bool sign = number < 0;

	// Case -0.0
	if(sign && (d64 & 0xFFFFFFFFFFFFF) == 0) 
		return +0.0;
	
	if(sign) d64 = d64 - 1u;
	else d64 = d64 + 1u;

	memcpy(&number, &d64, sizeof(double));

	return number;
}

// Applying the v8 JS engine solution. They did a great job.
// Read out the their version of 'stringify' at : 
//     https://github.com/v8/v8/blob/lkgr/src/numbers/conversions.cc#L1314

static NativePack numberStringify(VM* vm, int argCount, Value* values) {
	initNativePack;

	numberInstanceNumber;

	bool sign = number < 0;

	// Check for Infinity and NaN.

	if(isinf(number)) {
		pack.value = OBJECT_VAL(TAKE_STRING(sign ? "-Infinity" : "Infinity", 8u + sign, false));

		return pack;
	}
	else if(isnan(number)) {
		pack.value = OBJECT_VAL(TAKE_STRING("NaN", 3u, false));

		return pack;
	}

	// Defualt base is 10.

	uint8_t base = 10u;

	if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in Number.stringify(base)!");
		}

		double value = VALUE_NUMBER(values[1]);

		if(value < 2 || value > 36) {
			NATIVE_R_ERR("Expected the base range [2, 36], %lf found in Number.stringify(base)!", value);
		}

		base = (uint8_t) value;
	}

	// Special case for base 10.

	if(base == 10 && (number > MAX_SAFE_INTEGER || number < MIN_SAFE_INTEGER)) {
		// Then do scientific representation.
		// Simply call numberScientific.

		return numberScientific(vm, argCount, values);
	}

	// Characters to put in the buffer (respecive to their decimal number as index).

	char* chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	// Use absolute value.

	if(sign) 
		number = -number;

	// Buffer to store the number.

	char buffer[2048u];

	register double intp = trunc(number),
	                decp = number - intp;

	// Starting the very middle index. All integer part will continue to the left and
	// decimal part to the right. Assuming we have enough space left in the buffer.

	register short intpos = 1024, decpos = intpos + 1;

	// Minimum number decp should be greater than 'delta' to qualify the next digit.
	// Trust me, I don't know (yet) how it's generated.
	// Understand it as a rounding factor.

	register double delta = fmax(0.5 * (nextGreaterDouble(number) - number), nextGreaterDouble(0.0));

	if(decp >= delta) {
		// Start generating the fraction point.

		buffer[decpos++] = '.';

		do {
			// Shift up according to base.

			decp  *= base;
			delta *= base;

			short digit = (short) trunc(decp);

			// Insert the digit.

			buffer[decpos++] = chars[digit];

			// Count remainder (Enqueue the next digit).

			decp -= digit;

			// Check whether the decimal part needs rounding.

			// If the decimal part is greater than 0.5 let's say, 0.65...
			// do necessary rounding to shorten the decimal part, preserving
			// upmost precision. In short keep the decimal part from creating
			// gigantic strings.

			// But, theres a catch. Say, if I'm to store 0.35 into a double,
			// it will mess up the precision, because of the '3' before 5.
			// It will be as follows -> 0.349999999.... bla bla bla, also happens
			// with all odd numbers (They are odd after all). So, if generated
			// digit is odd (like 3 in our case) and the decp is 0.5, DO IMMEDIATE
			// ROUNDING UP.

			if(decp > 0.5 || (decp == 0.5 && (digit & 1))) {
				// If the very last digit almost crosses 1 (respective to our rounding
				// factor delta), roll back to previous digit and increase it by 1.

				if(decp + delta > 1) {
					// For cases like 0.349999999, roll back, increase and do it again.

					while(true) {
						decpos--;

						// If it hits the radix point, increse the integer part.

						if(decpos == 1024) {
							intp += 1;

							break;
						}

						// Or else go with the previous digit.

						// Get the current previous character and coresponding digit.

						char c = buffer[decpos];

						digit = c > '9' ? (c + 10 - 'a') : (c - '0');

						// If it's within the range of base, increase it by 1.

						if(digit + 1 < base) {
							buffer[decpos++] = chars[digit + 1];

							break;
						}
					}

					break;
				}
			}
		} while(decp >= delta);
	}

	register double remainder = 0;

	// Now process the integer part.

	do {
		remainder = fmod(intp, base);

		buffer[intpos--] = chars[(int) trunc(remainder)];

		intp = (intp - remainder) / base;
	} while(intp);

	// Add a '-' sign if the number is negative.

	if(sign) buffer[intpos--] = '-';

	// Calculate the length.

	size_t length = decpos - intpos - 1;

	// Actual string which we are gonna return.

	char* numstr = ALLOCATE(char, length + 1u);

	// Copy the contents.

	memcpy(numstr, buffer + intpos + 1, length * sizeof(char));

	// Terminate the buffer.

	numstr[length] = 0;

	pack.value = OBJECT_VAL(TAKE_STRING(numstr, length, true));

	return pack;
}

static NativePack numberValue(VM* vm, int argCount, Value* values) {
	initNativePack;

	numberInstanceNumber;

	pack.value = NUMBER_VAL(number);

	return pack;
}

static NativePack numberIsNaN(VM* vm, int argCount, Value* values) {
	initNativePack;

	if(argCount < 2) {
		pack.value = BOOL_VAL(true);

		return pack;
	}

	double number = 69;

	if(IS_NUMBER(values[1])) 
		number = VALUE_NUMBER(values[1]);
	else if(IS_INSTANCE(values[1])) {
		ObjInstance* instance = VALUE_INSTANCE(values[1]);

		if(!strcmp(instance -> klass -> name -> buffer, "Number")) {
			ValueContainer valueContainer;

			getField(instance, numberField, valueContainer);

			number = VALUE_NUMBER(valueContainer.value);
		} else number = NAN;
	} else number = NAN;

	pack.value = BOOL_VAL(isnan(number));

	return pack;
}

static NativePack number__represent__(VM* vm, int argCount, Value* values) {
	return numberValue(vm, argCount, values);
}

void initNumberLib(VM* vm) {
	numberField = TAKE_STRING("_p_number__", 11u, false);

	ObjString* name = TAKE_STRING("Number", 6u, false);

	ObjClass* numberClass = newClass(vm, name);

	vmNumberClass = numberClass;

	ObjNative* method;

	defineMethod(numberClass, TAKE_STRING("init", 4u, false), numberInit);
	defineMethod(numberClass, TAKE_STRING("__blooper__", 11u, false), numberBlooper);
	defineMethod(numberClass, TAKE_STRING("to_locale", 9u, false), numberToLocale);
	defineMethod(numberClass, TAKE_STRING("to_locale_bd", 12u, false), numberToLocaleBD);
	defineMethod(numberClass, TAKE_STRING("scientific", 10u, false), numberScientific);
	defineMethod(numberClass, TAKE_STRING("stringify", 9u, false), numberStringify);
	defineMethod(numberClass, TAKE_STRING("value", 5u, false), numberValue);
	defineMethod(numberClass, TAKE_STRING("__represent__", 13u, false), number__represent__);
	
	defineStaticMethod(numberClass, TAKE_STRING("isNaN", 5u, false), numberIsNaN);

	
	defineStaticMethod(numberClass, TAKE_STRING("is_safe_integer", 15u, false), numberIsSafeInteger);
	defineStaticMethod(numberClass, TAKE_STRING("numberify", 9u, false), numberNumberify);

	setStatic(numberClass, TAKE_STRING("MAX_SAFE_INTEGER", 16u, false), NUMBER_VAL(MAX_SAFE_INTEGER));
	setStatic(numberClass, TAKE_STRING("MIN_SAFE_INTEGER", 16u, false), NUMBER_VAL(MIN_SAFE_INTEGER));
	setStatic(numberClass, TAKE_STRING("MAX_VALUE", 9u, false), NUMBER_VAL(MAX_VALUE));
	setStatic(numberClass, TAKE_STRING("MIN_VALUE", 9u, false), NUMBER_VAL(MIN_VALUE));
	setStatic(numberClass, TAKE_STRING("EPSILON", 7u, false), NUMBER_VAL(nextGreaterDouble(1.0) - 1.0));

	tableInsert(&vm -> globals, name, (ValueContainer) { OBJECT_VAL(numberClass), true });
}

extern ObjClass* vmListClass;

#define listInstanceList if(!IS_LIST(values[0]) && (!IS_INSTANCE(values[0]) || VALUE_INSTANCE(values[0]) -> klass != vmListClass)) {\
	NATIVE_R_ERR("Provided reciever/instance (this) must be list or instance of List.init(length | list | elems...)!");\
}\
	ObjList* list;\
	if(IS_LIST(values[0])) list = VALUE_LIST(values[0]);\
	else {\
		ObjInstance* instance = VALUE_INSTANCE(values[0]);\
		ValueContainer listContainer;\
		getField(instance, listField, listContainer);\
		list = VALUE_LIST(listContainer.value);\
	}

ObjString* listField;

static NativePack listInit(VM* vm, int argCount, Value* values) {
	initNativePack;

	ObjList* list = NULL;

	if(argCount < 3) {
		if(IS_LIST(values[1])) {
			// Create a copy of the provided list.

			ObjList* valis = VALUE_LIST(values[1]);

			list = newList(vm);

			list -> capacity = valis -> capacity;
			list -> count    = valis -> count;
			list -> values   = GROW_ARRAY(Value, list -> values, 0u, list -> capacity);

			memcpy(list -> values, valis -> values, list -> count * sizeof(Value));
		}
		else if(IS_NUMBER(values[1])) {
			double number = VALUE_NUMBER(values[1]);

			if(number < 0) {
				NATIVE_R_ERR("Invalid number passed as an argument in List.init(length | list | elems...)!");
			}

			size_t size = (size_t) number;

			list = newList(vm);

			list -> capacity = size;
			list -> count    = size;
			list -> values   = GROW_ARRAY(Value, list -> values, 0u, size);

			Value set = NULL_VAL;

			if(argCount > 2) 
				set = values[2];
			
			for(register uint64_t i = 0u; i < size; i++) 
				list -> values[i] = set;
		} else {
			list = newList(vm);

			list -> capacity = GROW_CAPACITY(list -> capacity);
			list -> values   = GROW_ARRAY(Value, list -> values, 0u, list -> capacity);

			list -> values[list -> count++] = values[1];
		}
	} else {
		// Create a list with all the arguments.

		list = newList(vm);

		list -> capacity = argCount - 1;
		list -> count    = argCount - 1;
		list -> values   = GROW_ARRAY(Value, list -> values, 0u, argCount - 1);

		memcpy(list -> values, values + 1, (argCount - 1) * sizeof(Value));
	}
	
	ObjInstance* instance = VALUE_INSTANCE(values[0]);

	setField(instance, listField, OBJECT_VAL(list));

	pack.value = values[0];

	return pack;
}

#define getArity(value) IS_FUNCTION(value) ? VALUE_FUNCTION(value) -> arity : VALUE_CLOSURE(value) -> function -> arity

static NativePack listForeach(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call List.foreach(callback)!");
	}

	if(!IS_FUNCTION(values[1]) && !IS_CLOSURE(values[1])) {
		NATIVE_R_ERR("Expected the first argument to be callable in List.foreach(callback)!");
	}

	Value callback = values[1];

	uint32_t arity = getArity(callback);
	arity = arity <= 3u ? arity : 3u;

	for(uint64_t i = 0; i < list -> count; i++) {
		stack_push(vm, callback);

		Value args[] = { list -> values[i], NUMBER_VAL(i), OBJECT_VAL(list) };

		for(short j = 0; j < arity; j++) 
			stack_push(vm, args[j]);

		if(callValue(vm, callback, arity) && run(vm) != INTERPRET_RUNTIME_ERROR) stack_pop(vm);
		else {
			pack.hadError = true;

			return pack;
		}
	}

	return pack;
}

static NativePack listInsert(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	// If the list array is almost full, increase the capacity.

	if(list -> count + argCount - 1 > list -> capacity) {
		size_t oldCapacity = list -> capacity;

		list -> capacity = GROW_CAPACITY(list -> capacity);
		list -> values   = GROW_ARRAY(Value, list -> values, oldCapacity, list -> capacity);
	}

	for(register uint64_t i = 1; i < argCount; i++) 
		list -> values[list -> count++] = values[i];
	
	pack.value = NUMBER_VAL(list -> count);

	return pack;
}

static NativePack listReduce(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call List.reduce(reducer, initial_value)!");
	}

	if(list -> count == 0u) {
		pack.value = argCount > 2 ? values[2] : NULL_VAL;

		return pack;
	}

	if(!IS_FUNCTION(values[1]) && !IS_CLOSURE(values[1])) {
		NATIVE_R_ERR("The first argument should be callable in List.reduce(reducer, initial_value)!");
	}

	register uint64_t i = 0u;

	Value reducer = values[1],
	      initval = argCount > 2 ? values[2] : list -> values[i++];
	
	uint64_t arity = getArity(reducer);
	arity = arity <= 4u ? arity : 4u;
	
	for(; i < list -> count; i++) {
		stack_push(vm, reducer);

		Value args[] = { initval, list -> values[i], NUMBER_VAL(i), OBJECT_VAL(list) };

		for(short j = 0; j < arity; j++) 
			stack_push(vm, args[j]);
		
		if(callValue(vm, reducer, arity) && run(vm) != INTERPRET_RUNTIME_ERROR) {
			initval = stack_pop(vm);
		} else {
			pack.hadError = true;

			return pack;
		}
	}

	pack.value = initval;

	return pack;
}

static NativePack listReduceRight(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call List.reduce_right(reducer, initial_value)!");
	}

	if(list -> count == 0u) {
		pack.value = argCount > 2 ? values[2] : NULL_VAL;

		return pack;
	}

	if(!IS_FUNCTION(values[1]) && !IS_CLOSURE(values[1])) {
		NATIVE_R_ERR("The first argument should be callable in List.reduce_right(reducer, initial_value)!");
	}

	register long i = list -> count - 1;

	Value reducer = values[1],
	      initval = argCount > 2 ? values[2] : list -> values[i--];
	
	uint64_t arity = getArity(reducer);
	arity = arity <= 4u ? arity : 4u;
	
	for(; i >= 0; i--) {
		stack_push(vm, reducer);

		Value args[] = { initval, list -> values[i], NUMBER_VAL(i), OBJECT_VAL(list) };

		for(short j = 0; j < arity; j++) 
			stack_push(vm, args[j]);
		
		if(callValue(vm, reducer, arity) && run(vm) != INTERPRET_RUNTIME_ERROR) {
			initval = stack_pop(vm);
		} else {
			pack.hadError = true;

			return pack;
		}
	}

	pack.value = initval;

	return pack;
}

static NativePack listAppend(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	ObjList* result = newList(vm);

	result -> capacity = list -> count;
	result -> count    = list -> count;
	result -> values   = GROW_ARRAY(Value, result -> values, 0u, list -> count);

	memcpy(result -> values, list -> values, list -> count * sizeof(Value));

	for(register int i = 1; i < argCount; i++) {
		if(!IS_LIST(values[i])) {
			NATIVE_R_ERR("Provided argument no %d is not a list in List.append(lists...)!", i);
		}

		ObjList* tli = VALUE_LIST(values[i]);

		if(result -> count + tli -> count >= result -> capacity) {
			size_t oldCapacity = result -> capacity;

			result -> capacity += tli -> count;
			result -> values   = GROW_ARRAY(Value, result -> values, oldCapacity, result -> capacity);
		}

		memcpy(result -> values + result -> count, tli -> values, tli -> count * sizeof(Value));

		result -> count += tli -> count;
	}

	pack.value = OBJECT_VAL(result);

	return pack;
}

static NativePack listPop(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(list -> count == 0u) 
		return pack;

	pack.value = list -> values[0];

	for(register int i = 0; i < list -> count - 1u; i++) 
		list -> values[i] = list -> values[i + 1];
	
	list -> count -= 1u;

	return pack;
}

static NativePack listInsertFront(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(list -> count + argCount - 1u >= list -> capacity) {
		size_t oldCapacity = list -> capacity;

		list -> capacity += argCount - 1;
		list -> values   = GROW_ARRAY(Value, list -> values, oldCapacity, list -> capacity);
	}

	register int i;

	for(i = list -> count - 1; i >= 0; i--) 
		list -> values[i + argCount - 1u] = list -> values[i];
	
	list -> count += argCount - 1;
	
	memcpy(list -> values, values + 1u, (argCount - 1) * sizeof(Value));
	
	pack.value = NUMBER_VAL(list -> count);

	return pack;
}

static NativePack listMap(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call List.map(callback)!");
	}

	// Check whether the callback is function/closure.

	if(!IS_FUNCTION(values[1]) && !IS_CLOSURE(values[1])) {
		NATIVE_R_ERR("Expected the first argument callable (non-native) in List.map(callback)!");
	}

	Value callback = values[1];

	uint32_t arity = getArity(callback);
	arity = arity <= 3u ? arity : 3u;

	ObjList* result = newList(vm);

	result -> capacity = list -> count;
	result -> values   = GROW_ARRAY(Value, result -> values, 0u, result -> capacity);

	// For GC.

	stack_push(vm, OBJECT_VAL(result));

	for(size_t i = 0u; i < list -> count; i++) {
		stack_push(vm, callback);

		Value args[] = { list -> values[i], NUMBER_VAL(i), OBJECT_VAL(list) };

		for(short j = 0; j < arity; j++) 
			stack_push(vm, args[j]);
		
		if(callValue(vm, callback, arity) && run(vm) != INTERPRET_RUNTIME_ERROR) {
			result -> values[result -> count++] = stack_pop(vm);
		} else {
			pack.hadError = true;

			return pack;
		}
	}

	stack_pop(vm);

	pack.value = OBJECT_VAL(result);

	return pack;
}

static NativePack listSlice(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	ObjList* result = newList(vm);

	int start = 0, end = list -> count;

	int steps = 1u;

	if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected a number as a first argument in List.slice(start, end, steps)!");
		}

		start = (int) VALUE_NUMBER(values[1]);

		if(abs(start) <= list -> count) 
			start = start >= 0 ? start : list -> count + start;
		else start = start >= 0 ? list -> count : 0;
	}

	if(argCount > 2) {
		if(!IS_NUMBER(values[2])) {
			NATIVE_R_ERR("Expected a number as the second argument in List.slice(start, end, steps)!");
		}

		end = (int) VALUE_NUMBER(values[2]);

		if(abs(end) <= list -> count) 
			end = end >= 0 ? end : list -> count + end;
		else end = end >= 0 ? list -> count : 0;
	}

	if(argCount > 3) {
		if(!IS_NUMBER(values[3])) {
			NATIVE_R_ERR("Expected a number as steps in List.slice(start, end, steps)!");
		}

		steps = (int) VALUE_NUMBER(values[3]);
		
		if(steps < 0) {
			NATIVE_R_ERR("Steps cannot be negative in List.slice(start, end, steps)!");
		}

		steps = steps > list -> count ? (end - start) : steps;
	}

	pack.value = OBJECT_VAL(result);

	if(end <= start) 
		return pack;

	result -> capacity = (end - start) / steps;
	result -> values   = GROW_ARRAY(Value, result -> values, 0u, result -> capacity);

	for(; start < end; start += steps) 
		result -> values[result -> count++] = list -> values[start];

	return pack;
}

static NativePack listShift(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(list -> count == 0u) 
		return pack;

	pack.value = list -> values[--list -> count];

	return pack;
}

static NativePack listAt(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(list -> count == 0u) 
		return pack;

	uint64_t index = 0;

	if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in List.at(index)!");
		}

		double value = VALUE_NUMBER(values[1]);

		if(value < LLONG_MIN || value > LLONG_MAX) {
			NATIVE_R_ERR("Invalid index number provided in List.at(index)!");
		}

		if(fabs(value) < list -> count) 
			value = value >= 0 ? value : list -> count + value;
		else value = value > 0 ? list -> count - 1u : 0;

		index = (uint64_t) value;
	}

	pack.value = list -> values[index];

	return pack;
}

static NativePack listValue(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	pack.value = OBJECT_VAL(list);

	return pack;
}

static NativePack list__represent__(VM* vm, int argCount, Value* values) {
	return listValue(vm, argCount, values);
}

static NativePack listReverse(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	ObjList* result = newList(vm);

	result -> capacity = list -> count;
	result -> count    = list -> count;
	result -> values   = GROW_ARRAY(Value, result -> values, 0u, result -> count);

	for(uint64_t i = 0, j = list -> count - 1u; i < list -> count; i++, j--) 
		result -> values[j] = list -> values[i];
	
	pack.value = OBJECT_VAL(result);

	return pack;
}

static NativePack listIndex(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	pack.value = NUMBER_VAL(-1);

	if(argCount < 2) 
		return pack;

	uint8_t occurance = 0;

	if(argCount > 2) {
		if(!IS_NUMBER(values[2])) {
			NATIVE_R_ERR("Expected the second argument to be a number in List.index(value, occurance)!");
		}

		double value = VALUE_NUMBER(values[2]);

		if(value < 0 || value > 1) {
			NATIVE_R_ERR("Invalid occurance value passed in List.index(value, occurance)!");
		}

		occurance = (uint8_t) value;
	}

	if(occurance == 0) {
		for(size_t i = 0; i < list -> count; i++) {
			if(valuesEqual(values[1], list -> values[i])) {
				pack.value = NUMBER_VAL(i);

				return pack;
			}
		}
	} else {
		for(long i = list -> count - 1u; i >= 0u; i--) {
			if(valuesEqual(values[1], list -> values[i])) {
				pack.value = NUMBER_VAL(i);

				return pack;
			}
		}
	}

	return pack;
}

static void simpleAppend(ObjList* list1, ObjList* list2) {
	if(list1 -> count + list2 -> count >= list1 -> capacity) {
		size_t oldCapacity = list1 -> capacity;

		list1 -> capacity += list2 -> count;
		list1 -> values    = GROW_ARRAY(Value, list1 -> values, oldCapacity, list1 -> capacity);
	}

	memcpy(list1 -> values + list1 -> count, list2 -> values, list2 -> count * sizeof(Value));

	list1 -> count += list2 -> count;
}

static ObjList* flat(VM* vm, ObjList* list, int currentDepth, int depth) {
	ObjList* result = newList(vm);

	for(uint64_t i = 0; i < list -> count; i++) {
		Value elem = list -> values[i];

		if(IS_LIST(elem) && (depth < 0 || currentDepth < depth)) 
			simpleAppend(result, flat(vm, VALUE_LIST(elem), currentDepth + 1, depth));
		else {
			if(result -> count + 1u >= result -> capacity) {
				size_t oldCapacity = result -> capacity;

				result -> capacity = GROW_CAPACITY(oldCapacity);
				result -> values   = GROW_ARRAY(Value, result -> values, oldCapacity, result -> capacity);
			}

			result -> values[result -> count++] = elem;
		}
	}

	return result;
}

static NativePack listFlat(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	int depth = -1;

	if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in List.flat(depth)!");
		}

		double value = VALUE_NUMBER(values[1]);

		if(value < 0) {
			NATIVE_R_ERR("The provided depth should be a positive value in List.flat(depth)!");
		}

		depth = (int) value;
	}

	pack.value = OBJECT_VAL(flat(vm, list, 0, depth));

	return pack;
}

static NativePack listContains(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	pack.value = BOOL_VAL(false);

	if(argCount < 2) 
		return pack;

	Value val = values[1];

	int from = 0;

	if(argCount > 2) {
		if(!IS_NUMBER(values[2])) {
			NATIVE_R_ERR("Expected start_index to be a number in List.contains(value, start_index)!");
		}

		from = (int) VALUE_NUMBER(values[2]);

		if(abs(from) < list -> count) 
			from = from >= 0 ? from : list -> count + from;
		else from = from > 0 ? list -> count - 1 : 0;
	}

	for(; from < list -> count; from++) {
		if(valuesEqual(list -> values[from], val)) {
			pack.value = BOOL_VAL(true);

			return pack;
		}
	}

	return pack;
}

static NativePack listFill(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	Value value = NULL_VAL;

	if(argCount > 1) 
		value = values[1];

	int start = 0, end = list -> count;

	if(argCount > 2) {
		if(!IS_NUMBER(values[2])) {
			NATIVE_R_ERR("Expected start to be a number in List.fill(value, start, end)!");
		}

		start = (int) VALUE_NUMBER(values[2]);

		if(abs(start) <= list -> count) 
			start = start >= 0 ? start : list -> count + start;
		else start = start > 0 ? list -> count - 1u : 0;
	}

	if(argCount > 3) {
		if(!IS_NUMBER(values[3])) {
			NATIVE_R_ERR("Expected end to be a number in List.fill(value, start, end)!");
		}

		end = (int) VALUE_NUMBER(values[3]);

		if(abs(end) <= list -> count) 
			end = end >= 0 ? end : list -> count + end;
		else end = end > 0 ? list -> count : 0;
	}

	while(start < end) 
		list -> values[start++] = value;
	
	pack.value = OBJECT_VAL(list);

	return pack;
}

static NativePack listSearch(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call List.search(predicate, occurance)!");
	}

	if(!IS_FUNCTION(values[1]) && !IS_CLOSURE(values[1])) {
		NATIVE_R_ERR("Expected the first argument to be callable in List.search(predicate, occurance)!");
	}

	Value predicate = values[1];

	uint8_t occurance = 0;

	if(argCount > 2) {
		if(!IS_NUMBER(values[2])) {
			NATIVE_R_ERR("Expected the second argument to be a number in List.search(predicate, occurance)!");
		}

		double value = VALUE_NUMBER(values[2]);

		if(value < 0 || value > 1) {
			NATIVE_R_ERR("Invalid occurance value passed in List.search(predicate, occurance)!");
		}

		occurance = (uint8_t) value;
	}

	uint64_t arity = getArity(predicate);
	arity = arity <= 3u ? arity : 3u;

	if(occurance) {
		for(long i = list -> count - 1; i >= 0; i--) {
			Value value = list -> values[i];

			stack_push(vm, predicate);

			Value args[] = { value, NUMBER_VAL(i), OBJECT_VAL(list) };

			for(short j = 0; j < arity; j++) 
				stack_push(vm, args[j]);
			
			if(callValue(vm, predicate, arity) && run(vm) != INTERPRET_RUNTIME_ERROR) {
				Value fnres = stack_pop(vm);

				if(toBoolean(vm, &fnres)) {
					pack.value = value;

					return pack;
				}
			} else {
				pack.hadError = true;

				return pack;
			}
		}
	} else {
		for(long i = 0; i < list -> count; i++) {
			Value value = list -> values[i];

			stack_push(vm, predicate);

			Value args[] = { value, NUMBER_VAL(i), OBJECT_VAL(list) };

			for(short j = 0; j < arity; j++) 
				stack_push(vm, args[j]);
			
			if(callValue(vm, predicate, arity) && run(vm) != INTERPRET_RUNTIME_ERROR) {
				Value fnres = stack_pop(vm);

				if(toBoolean(vm, &fnres)) {
					pack.value = value;

					return pack;
				}
			} else {
				pack.hadError = true;

				return pack;
			}
		}
	}

	return pack;
}

static NativePack listSearchIndex(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call List.search(predicate, occurance)!");
	}

	if(!IS_FUNCTION(values[1]) && !IS_CLOSURE(values[1])) {
		NATIVE_R_ERR("Expected the first argument to be callable in List.search(predicate, occurance)!");
	}

	Value predicate = values[1];

	uint8_t occurance = 0;

	if(argCount > 2) {
		if(!IS_NUMBER(values[2])) {
			NATIVE_R_ERR("Expected the second argument to be a number in List.search(predicate, occurance)!");
		}

		double value = VALUE_NUMBER(values[2]);

		if(value < 0 || value > 1) {
			NATIVE_R_ERR("Invalid occurance value passed in List.search(predicate, occurance)!");
		}

		occurance = (uint8_t) value;
	}

	uint64_t arity = getArity(predicate);
	arity = arity <= 3u ? arity : 3u;

	if(occurance) {
		for(long i = list -> count - 1; i >= 0; i--) {
			Value value = list -> values[i];

			stack_push(vm, predicate);

			Value args[] = { value, NUMBER_VAL(i), OBJECT_VAL(list) };

			for(short j = 0; j < arity; j++) 
				stack_push(vm, args[j]);
			
			if(callValue(vm, predicate, arity) && run(vm) != INTERPRET_RUNTIME_ERROR) {
				Value fnres = stack_pop(vm);

				if(toBoolean(vm, &fnres)) {
					pack.value = NUMBER_VAL(i);

					return pack;
				}
			} else {
				pack.hadError = true;

				return pack;
			}
		}
	} else {
		for(long i = 0; i < list -> count; i++) {
			Value value = list -> values[i];

			stack_push(vm, predicate);

			Value args[] = { value, NUMBER_VAL(i), OBJECT_VAL(list) };

			for(short j = 0; j < arity; j++) 
				stack_push(vm, args[j]);
			
			if(callValue(vm, predicate, arity) && run(vm) != INTERPRET_RUNTIME_ERROR) {
				Value fnres = stack_pop(vm);

				if(toBoolean(vm, &fnres)) {
					pack.value = NUMBER_VAL(i);

					return pack;
				}
			} else {
				pack.hadError = true;

				return pack;
			}
		}
	}

	pack.value = NUMBER_VAL(-1);

	return pack;
}

static NativePack listFilter(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call List.filter(predicate)!");
	}

	if(!IS_FUNCTION(values[1]) && !IS_CLOSURE(values[1])) {
		NATIVE_R_ERR("The provided predicate is not callable in List.filter(predicate)!");
	}

	Value predicate = values[1];

	ObjList* result = newList(vm);

	uint64_t arity = getArity(predicate);
	arity = arity <= 3u ? arity : 3u;

	// For GC.

	stack_push(vm, OBJECT_VAL(result));

	for(uint64_t i = 0; i < list -> count; i++) {
		Value value = list -> values[i];

		stack_push(vm, predicate);

		Value args[] = { value, NUMBER_VAL(i), OBJECT_VAL(list) };

		for(short j = 0; j < arity; j++) 
			stack_push(vm, args[j]);
		
		if(callValue(vm, predicate, arity) && run(vm) != INTERPRET_RUNTIME_ERROR) {
			Value fnres = stack_pop(vm);

			if(toBoolean(vm, &fnres)) {
				if(result -> count + 1u >= result -> capacity) {
					size_t oldCapacity = result -> capacity;

					result -> capacity = GROW_CAPACITY(oldCapacity);
					result -> values   = GROW_ARRAY(Value, result -> values, oldCapacity, result -> capacity);
				}

				result -> values[result -> count++] = value;
			}
		} else {
			pack.hadError = true;

			return pack;
		}
	}

	pack.value = stack_pop(vm);

	return pack;
}

static NativePack listRemove(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	ObjList* result = newList(vm);

	pack.value = OBJECT_VAL(result);

	if(argCount < 2) 
		return pack;
	
	if(!IS_NUMBER(values[1])) {
		NATIVE_R_ERR("Expected the first argument to be a number in List.remove(index, range)!");
	}

	long long start = (long long) VALUE_NUMBER(values[1]);

	if(llabs(start) < list -> count) 
		start = start >= 0 ? start : list -> count + start;
	else start = start > 0 ? list -> count - 1u : 0;

	long long range = list -> count;

	if(argCount > 2) {
		if(!IS_NUMBER(values[2])) {
			NATIVE_R_ERR("Expected the range to be a number in List.remove(index, range)!");
		}

		range = (long long) VALUE_NUMBER(values[2]);

		if(range < 0) {
			NATIVE_R_ERR("Range must be a positive value in List.remove(index, range)!");
		}
	}

	if(range + start > list -> count) 
		range = list -> count - start;
	else if(range == 0) 
		return pack;
	
	result -> capacity = range;
	result -> count    = range;
	result -> values   = GROW_ARRAY(Value, result -> values, 0u, result -> capacity);

	memcpy(result -> values, list -> values + start, range * sizeof(Value));

	list -> count -= range;

	for(; start < list -> count; start++) 
		list -> values[start] = list -> values[start + range];

	return pack;
}

static NativePack listInject(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call List.inject(index, values...)!");
	}

	long long index = (long long) VALUE_NUMBER(values[1]);

	if(llabs(index) < list -> count) 
		index = index >= 0 ? index : list -> count + index;
	else index = index > 0 ? list -> count - 1u : 0;

	int extlen = argCount - 2;

	if(list -> count + extlen >= list -> capacity) {
		size_t oldCapacity = list -> capacity;

		list -> capacity += extlen;
		list -> values    = GROW_ARRAY(Value, list -> values, oldCapacity, list -> capacity);
	}

	list -> count += extlen;
	
	for(long long i = list -> count - 1u; i >= index; i--) 
		list -> values[i] = list -> values[i - extlen];

	memcpy(list -> values + index, values + 2u, extlen * sizeof(Value));

	pack.value = NUMBER_VAL(list -> count);

	return pack;
}

static NativePack listCheck(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call List.check(predicate, type)!");
	}

	if(!IS_FUNCTION(values[1]) && !IS_CLOSURE(values[1])) {
		NATIVE_R_ERR("The predicate must be callable in List.check(predicate, type)!");
	}

	Value predicate = values[1];

	bool type = 0;

	if(argCount > 2) {
		if(!IS_NUMBER(values[2])) {
			NATIVE_R_ERR("The prvided second argument is not a number in List.check(predicate, type)!");
		}

		double value = VALUE_NUMBER(values[2]);

		if(value < 0 || value > 1) {
			NATIVE_R_ERR("Invalid type passed in List.check(predicate, type)!");
		}

		type = (bool) value;
	}

	uint64_t arity = getArity(predicate);
	arity = arity <= 3u ? arity : 3u;

	for(uint64_t i = 0; i < list -> count; i++) {
		Value value = list -> values[i];

		stack_push(vm, predicate);

		Value args[] = { value, NUMBER_VAL(i), OBJECT_VAL(list) };

		for(short j = 0; j < arity; j++) 
			stack_push(vm, args[j]);

		if(callValue(vm, predicate, arity) && run(vm) != INTERPRET_RUNTIME_ERROR) {
			Value fnres = stack_pop(vm);

			bool ret = toBoolean(vm, &fnres);

			if(type ^ ret) {
				pack.value = BOOL_VAL(ret & true);

				return pack;
			}
		} else {
			pack.hadError = true;

			return pack;
		}
	}

	pack.value = BOOL_VAL(type);

	return pack;
}

static NativePack listJoin(VM* vm, int argCount, Value* values) {
	initNativePack;

	listInstanceList;

	if(list -> count == 0u) {
		char* buffer = ALLOCATE(char, 1u);

		buffer[0] = 0;

		pack.value = OBJECT_VAL(TAKE_STRING(buffer, 0u, true));

		return pack;
	}

	char* seperator = ",";
	size_t seplen   = 1u;

	if(argCount > 1) {
		seperator = toString(vm, values + 1u);

		if(seperator == NULL) {
			pack.hadError;

			return pack;
		}

		seplen = strlen(seperator);
	}

	size_t bufidx = 0, length = 1100;
	char* buffer  = ALLOCATE(char, length);

	for(uint64_t i = 0; i < list -> count; i++) {
		Value value = list -> values[i];
		bool atEnd  = i == list -> count - 1u;

		char* result = toString(vm, &value);

		if(result == NULL) {
			pack.hadError = true;

			return pack;
		}

		size_t reslen = strlen(result);

		size_t newlen = reslen + bufidx + (atEnd ? 0 : seplen) + 1u;

		if(newlen > length) {
			buffer = (char*) reallocate((void*) buffer, length * sizeof(char), newlen * sizeof(char));

			length = newlen;
		}

		bufidx += sprintf(buffer + bufidx, result);

		if(!atEnd)
			bufidx += sprintf(buffer + bufidx, seperator);

		free(result);
	}

	if(argCount > 1) 
		free(seperator);
	
	buffer = (char*) reallocate((void*) buffer, length * sizeof(char), (bufidx + 1u) * sizeof(char));

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, bufidx, true));

	return pack;
}

char* toStringRaw(VM* vm, Value* const value) {
	switch(value -> type) {
		case VAL_NUMBER: {
			double number = VALUE_NUMBER(*value);
			
			bool sign = number < 0;

			if(isinf(number)) {
				char* buffer = malloc(10u * sizeof(char));

				if(buffer == NULL) {
					RUNTIME_ERROR("Failed to allocate memory while converting number to string!");

					return NULL;
				}

				sprintf(buffer, sign ? "-Infinity" : "Infinity");

				return buffer;
			}
			else if(isnan(number)) {
				char* buffer = malloc(4u * sizeof(char));

				if(buffer == NULL) {
					RUNTIME_ERROR("Failed to allocate memory while converting number to string!");

					return NULL;
				}

				sprintf(buffer, "NaN");

				return buffer;
			}

			if(number > MAX_SAFE_INTEGER || number < MIN_SAFE_INTEGER) 
				return scientific(number, -1);
			
			// This code is from numberStringify(VM*, int, Value*).
			// Just little bit modified.
			
			char* chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

			if(sign) 
				number = -number;

			char buffer[2048u];

			register double intp = trunc(number),
							decp = number - intp;

			register short intpos = 1024, decpos = intpos + 1;

			register double delta = fmax(0.5 * (nextGreaterDouble(number) - number), nextGreaterDouble(0.0));

			if(decp >= delta) {
				// Start generating the fraction point.

				buffer[decpos++] = '.';

				do {
					decp  *= 10u;
					delta *= 10u;

					short digit = (short) trunc(decp);

					buffer[decpos++] = chars[digit];

					decp -= digit;

					if(decp > 0.5 || (decp == 0.5 && (digit & 1))) {
						if(decp + delta > 1) {
							while(true) {
								decpos--;

								if(decpos == 1024) {
									intp += 1;

									break;
								}

								char c = buffer[decpos];

								digit = c > '9' ? (c + 10 - 'a') : (c - '0');

								if(digit + 1 < 10u) {
									buffer[decpos++] = chars[digit + 1];

									break;
								}
							}

							break;
						}
					}
				} while(decp >= delta);
			}

			register double remainder = 0;

			do {
				remainder = fmod(intp, 10u);

				buffer[intpos--] = chars[(int) trunc(remainder)];

				intp = (intp - remainder) / 10u;
			} while(intp);

			if(sign) buffer[intpos--] = '-';

			size_t length = decpos - intpos - 1;

			char* numstr = malloc((length + 1u) * sizeof(char));

			if(numstr == NULL) {
				RUNTIME_ERROR("Failed to allocate memory while converting number to string!");

				return NULL;
			}

			memcpy(numstr, buffer + intpos + 1, length * sizeof(char));

			numstr[length] = 0;

			return numstr;
		}

		case VAL_NULL: {
			char* buffer = (char*) malloc(5u * sizeof(char));

			if(buffer == NULL) {
				RUNTIME_ERROR("Failed to allocate memory while converting null to string!");

				return NULL;
			}

			sprintf(buffer, "null");

			return buffer;
		}

		case VAL_BOOLEAN: {
			bool bl = VALUE_BOOL(*value);

			char* buffer = malloc(6u * sizeof(char));

			if(buffer == NULL) {
				RUNTIME_ERROR("Failed to allocate memory while converting boolean to string!");

				return NULL;
			}

			strcpy(buffer, bl ? "true" : "false");

			return buffer;
		}

		case VAL_OBJECT: {
			switch(OBJ_TYPE(*value)) {
				case OBJ_STRING: {
					ObjString* string = VALUE_STRING(*value);

					char* buffer = malloc((string -> length + 1u) * sizeof(char));

					if(buffer == NULL) {
						RUNTIME_ERROR("Failed to allocate memory while allocating string!");

						return NULL;
					}

					strcpy(buffer, string -> buffer);

					return buffer;
				}

				case OBJ_FUNCTION: {
					ObjFunction* function = VALUE_FUNCTION(*value);

					char* buffer = malloc(((function -> name == NULL ? 9u : function -> name -> length) + 30u) * sizeof(char));

					if(buffer == NULL) {
						RUNTIME_ERROR("Failed to allocate memory while converting function to string!");

						return NULL;
					}

					sprintf(buffer, "<fn '%s' at 0x%.16X>", function -> name == NULL ? "anonymous" :
						function -> name -> buffer, function);

					return buffer;
				}

				case OBJ_CLOSURE: {
					ObjClosure* closure = VALUE_CLOSURE(*value);

					char* buffer = malloc(((closure -> function -> name == NULL ? 9u : closure -> function -> name -> length) + 70u) * sizeof(char));

					if(buffer == NULL) {
						RUNTIME_ERROR("Failed to allocate memory while converting closure to string!");

						return NULL;
					}

					sprintf(buffer, "<closure '%s' at 0x%.16X with %d upvalues>", closure -> function -> name != NULL ? 
						closure -> function -> name -> buffer : 
						"anonymous", closure -> function, closure -> upvalueCount);

					return buffer;
				}

				case OBJ_NATIVE: {
					char* buffer = malloc(35u * sizeof(char));

					if(buffer == NULL) {
						RUNTIME_ERROR("Failed to allocate memory while converting native function to string!");

						return NULL;
					}

					sprintf(buffer, "<fn native at 0x%.16X>", VALUE_NATIVE(*value) -> function);

					return buffer;
				}

				case OBJ_UPVALUE: {
					char* buffer = malloc(10u * sizeof(char));

					if(buffer == NULL) {
						RUNTIME_ERROR("Failed to allocate memory while converting upvalue to string!");

						return NULL;
					}

					strcpy(buffer, "<upvalue>");

					return buffer;
				}

				case OBJ_CLASS: {
					ObjClass* klass = VALUE_CLASS(*value);

					char* buffer = malloc((klass -> name -> length + 12u) * sizeof(char));

					if(buffer == NULL) {
						RUNTIME_ERROR("Failed to allocate memory while converting class to string!");

						return NULL;
					}

					sprintf(buffer, "<class '%s'>", klass -> name -> buffer);

					return buffer;
				}

				case OBJ_INSTANCE: {
					ObjInstance* instance = VALUE_INSTANCE(*value);

					char* buffer = malloc((instance -> klass -> name -> length + 25u) * sizeof(char));

					if(buffer == NULL) {
						RUNTIME_ERROR("Failed to allocate memory while converting instance to string!");

						return NULL;
					}

					sprintf(buffer, "<instance of '%s' class>", VALUE_INSTANCE(*value) -> klass -> name -> buffer);

					return buffer;
				}

				case OBJ_BOUND_METHOD: {
					ObjBoundMethod* method = VALUE_BOUND_METHOD(*value);

					char* buffer;
					size_t length, index = 0u;
					
					switch(method -> function -> type) {
						case OBJ_CLOSURE: {
							ObjClosure* closure = (ObjClosure*) method -> function;

							length = ((closure -> function -> name == NULL ? 9u : closure -> function -> name -> length) + 90u) * sizeof(char);

							buffer = malloc(length);

							if(buffer == NULL) {
								RUNTIME_ERROR("Failed to allocate memory while converting bound method to string!");

								return NULL;
							}

							index = sprintf(buffer, "<closure '%s' at 0x%.16X with %d upvalues", 
								closure -> function -> name != NULL ? closure -> function -> name -> buffer : "anonymous", closure -> function, closure -> upvalueCount);
							
							break;
						}
						
						case OBJ_NATIVE: {
							length = 55u;

							buffer = malloc(length * sizeof(char));

							if(buffer == NULL) {
								RUNTIME_ERROR("Failed to allocate memory while converting bound method to string!");

								return NULL;
							}

							index = sprintf(buffer, "<fn native at 0x%.16X", ((ObjNative*) method -> function) -> function);
							
							break;
						}
						
						case OBJ_FUNCTION: {
							ObjFunction* function = (ObjFunction*) method -> function;

							length = ((function -> name == NULL ? 9u : function -> name -> length) + 50u) * sizeof(char);

							buffer = malloc(length * sizeof(char));

							if(buffer == NULL) {
								RUNTIME_ERROR("Failed to allocate memory while converting bound method to string!");

								return NULL;
							}
							
							index = sprintf(buffer, "<fn '%s' at 0x%.16X", function -> name -> buffer, function);
							
							break;
						}
					}
					
					index += sprintf(buffer + index, " of instance ");
					
					switch(method -> reciever.type) {
						case VAL_OBJECT: {
							switch(OBJ_TYPE(method -> reciever)) {
								case OBJ_INSTANCE: {
									ObjClass* klass = VALUE_INSTANCE(method -> reciever) -> klass;

									length += klass -> name -> length * sizeof(char);

									buffer = realloc(buffer, length);

									if(buffer == NULL) {
										RUNTIME_ERROR("Failed to allocate memory while converting bound method to string!");

										return NULL;
									}

									index += sprintf(buffer + index, "'%s'>", klass -> name -> buffer);
								
									break;
								}

								// Objects which has wrapper classes.

								case OBJ_LIST: {
									length += 4u * sizeof(char);

									buffer = realloc(buffer, length);

									if(buffer == NULL) {
										RUNTIME_ERROR("Failed to allocate memory while converting bound method to string!");

										return NULL;
									}

									index += sprintf(buffer + index, "'%s'>", "List");

									break;
								}

								case OBJ_STRING: {
									length += 6u * sizeof(char);

									buffer = realloc(buffer, length);

									if(buffer == NULL) {
										RUNTIME_ERROR("Failed to allocate memory while converting bound method to string!");

										return NULL;
									}

									index += sprintf(buffer + index, "'%s'>", "String");

									break;
								}

								case OBJ_DICTIONARY: {
									length += 10u * sizeof(char);

									buffer = realloc(buffer, length);

									if(buffer == NULL) {
										RUNTIME_ERROR("Failed to allocate memory while converting bound method to string!");

										return NULL;
									}

									index += sprintf(buffer + index, "'%s'>", "Dictionary");

									break;
								}
							}

							break;
						}
						
						// Refers to a wrapper class.

						case VAL_NUMBER: {
							length += 6u * sizeof(char);

							buffer = realloc(buffer, length);

							if(buffer == NULL) {
								RUNTIME_ERROR("Failed to allocate memory while converting bound method to string!");

								return NULL;
							}

							index += sprintf(buffer + index, "'%s'>", "Number"); break;
						}
					}

					return buffer;
				}

				case OBJ_DICTIONARY: {
					Table* fields = &VALUE_DICTIONARY(*value) -> fields;

					ObjString* stringify = TAKE_STRING("stringify", 9u, false);
					ObjString* represent = TAKE_STRING("__represent__", 13u, false);

					ValueContainer valueContainer;

					bool found;

					if((found = tableGet(fields, stringify, &valueContainer)));
					else if((found = tableGet(fields, represent, &valueContainer)));

					if(found) {
						Value callable = valueContainer.value;

						if(IS_FUNCTION(callable) || IS_CLOSURE(callable)) {
							stack_push(vm, callable);

							if(callValue(vm, callable, 0u) && run(vm) != INTERPRET_RUNTIME_ERROR) {
								// Reuse callable.

								callable = stack_pop(vm);

								return toStringRaw(vm, &callable);
							} else return NULL;
						}
						else if(IS_NATIVE(callable)) {
							NativeFn native = VALUE_NATIVE(callable) -> function;

							NativePack pack = native(vm, 1, (Value*) value);

							if(!pack.hadError) 
								return toStringRaw(vm, &pack.value);
							
							return NULL;
						}
					}

					char* buffer = malloc(16u * sizeof(char));

					if(buffer == NULL) {
						RUNTIME_ERROR("Failed to allocate memory while converting dictionary to string!");

						return NULL;
					}

					sprintf(buffer, "{ Dictionary }");

					return buffer;
				}

				case OBJ_LIST: {
					NativePack pack = listJoin(vm, 1u, (Value*) value);

					if(!pack.hadError) {
						ObjString* string = VALUE_STRING(pack.value);

						char* buffer = malloc((string -> length + 1u) * sizeof(char));

						strcpy(buffer, string -> buffer);

						return buffer;
					}

					
					return NULL;
				}

				case OBJ_BYTELIST: {
					ObjByteList* byteList = VALUE_BYTELIST(*value);

					char* buffer = malloc(((byteList -> size * 5u) + 15u) * sizeof(char));

					if(buffer == NULL) {
						RUNTIME_ERROR("Failed to allocate memory while converting bytelist to string!");

						return NULL;
					}

					size_t index = sprintf(buffer, "<bytelist b'");

					for(size_t i = 0u; i < byteList -> size; i++) 
						index += sprintf(buffer + index, "\\x%03hhu", byteList -> bytes[i]);
					
					sprintf(buffer + index, "'>");

					return buffer;
				}
			}

			break;
		}
	}
}

char* toString(VM* vm, Value* const value) {
	if(IS_INSTANCE(*value)) {
		ObjInstance* instance = VALUE_INSTANCE(*value);

		ValueContainer valueContainer;

		ObjString* stringify = TAKE_STRING("stringify", 9u, false);
		ObjString* represent = TAKE_STRING("__represent__", 13u, false);

		bool found = false;

		if((found = tableGet(&instance -> fields, stringify, &valueContainer)));
		else if((found = tableGet(&instance -> klass -> methods, stringify, &valueContainer)));
		else if((found = tableGet(&instance -> fields, represent, &valueContainer)));
		else if((found = tableGet(&instance -> klass -> methods, represent, &valueContainer)));

		if(found) {
			Value callable = valueContainer.value;

			if(IS_FUNCTION(callable) || IS_CLOSURE(callable)) {
				stack_push(vm, callable);

				if(callValue(vm, callable, 0u)) {
					vm -> stack[vm -> stackTop - 1u] = *value;

					if(run(vm) != INTERPRET_RUNTIME_ERROR) {
						callable = stack_pop(vm);

						return toStringRaw(vm, &callable);
					}

					return NULL;
				}

				return NULL;
			}
			else if(IS_NATIVE(callable)) {
				NativeFn native = VALUE_NATIVE(callable) -> function;

				NativePack pack = native(vm, 1, (Value*) value);

				if(!pack.hadError) 
					return toStringRaw(vm, &pack.value);
				
				return NULL;
			}
		}
	}

	return toStringRaw(vm, value);
}

static NativePack listStringify(VM* vm, int argCount, Value* values) {
	return listJoin(vm, 1u, values);
}

static NativePack listMake(VM* vm, int argCount, Value* values) {
	initNativePack;

	ObjList* result = newList(vm);

	result -> capacity += argCount - 1;
	result -> values    = GROW_ARRAY(Value, result -> values, 0u, result -> capacity);

	for(int i = 1; i < argCount; i++) 
		result -> values[result -> count++] = values[i];
	
	pack.value = OBJECT_VAL(result);

	return pack;
}

void initListLib(VM* vm) {
	listField = TAKE_STRING("_p_list__", 9u, false);

	ObjString* name = TAKE_STRING("List", 4u, false);

	ObjClass* listClass = newClass(vm, name);

	vmListClass = listClass;

	ObjNative* method;

	defineMethod(listClass, vm -> initString, listInit);

	defineMethod(listClass, TAKE_STRING("insert", 6u, false), listInsert);
	defineMethod(listClass, TAKE_STRING("insert_front", 12u, false), listInsertFront);
	defineMethod(listClass, TAKE_STRING("reduce", 6u, false), listReduce);
	defineMethod(listClass, TAKE_STRING("reduce_right", 12u, false), listReduceRight);
	defineMethod(listClass, TAKE_STRING("map", 3u, false), listMap);
	defineMethod(listClass, TAKE_STRING("slice", 5u, false), listSlice);
	defineMethod(listClass, TAKE_STRING("append", 6u, false), listAppend);
	defineMethod(listClass, TAKE_STRING("pop", 3u, false), listPop);
	defineMethod(listClass, TAKE_STRING("shift", 5u, false), listShift);
	defineMethod(listClass, TAKE_STRING("at", 2u, false), listAt);
	defineMethod(listClass, TAKE_STRING("value", 5u, false), listValue);
	defineMethod(listClass, TAKE_STRING("foreach", 7u, false), listForeach);
	defineMethod(listClass, TAKE_STRING("reverse", 7u, false), listReverse);
	defineMethod(listClass, TAKE_STRING("index", 5u, false), listIndex);
	defineMethod(listClass, TAKE_STRING("flat", 4u, false), listFlat);
	defineMethod(listClass, TAKE_STRING("contains", 8u, false), listContains);
	defineMethod(listClass, TAKE_STRING("fill", 4u, false), listFill);
	defineMethod(listClass, TAKE_STRING("search", 6u, false), listSearch);
	defineMethod(listClass, TAKE_STRING("search_index", 12u, false), listSearchIndex);
	defineMethod(listClass, TAKE_STRING("filter", 6u, false), listFilter);
	defineMethod(listClass, TAKE_STRING("remove", 6u, false), listRemove);
	defineMethod(listClass, TAKE_STRING("inject", 6u, false), listInject);
	defineMethod(listClass, TAKE_STRING("check", 5u, false), listCheck);
	defineMethod(listClass, TAKE_STRING("join", 4u, false), listJoin);
	defineMethod(listClass, TAKE_STRING("stringify", 9u, false), listStringify);
	defineMethod(listClass, TAKE_STRING("__represent__", 13u, false), list__represent__);

	defineStaticMethod(listClass, TAKE_STRING("make", 4u, false), listMake);

	// For List.index(value, occurance) and List.search(predicate, occurance)

	setStatic(listClass, TAKE_STRING("FIRST_OCCURANCE", 15u, false), NUMBER_VAL(0));
	setStatic(listClass, TAKE_STRING("LAST_OCCURANCE", 14u, false), NUMBER_VAL(1));

	// For List.check(predicate, type).

	setStatic(listClass, TAKE_STRING("CHECK_SINGLE", 12u, false), NUMBER_VAL(0));
	setStatic(listClass, TAKE_STRING("CHECK_ALL", 9u, false), NUMBER_VAL(1));

	tableInsert(&vm -> globals, name, (ValueContainer) { OBJECT_VAL(listClass), true });
}

ObjString* stringField;

extern ObjClass* vmStringClass;

#define stringInstanceString if(!IS_STRING(values[0]) && (!IS_INSTANCE(values[0]) || VALUE_INSTANCE(values[0]) -> klass != vmStringClass)) {\
	NATIVE_R_ERR("Provided reciever/instance (this) must be string or instance of String.init(string)!");\
}\
	ObjString* string;\
	if(IS_STRING(values[0])) string = VALUE_STRING(values[0]);\
	else {\
		ObjInstance* instance = VALUE_INSTANCE(values[0]);\
		ValueContainer stringContainer;\
		getField(instance, stringField, stringContainer);\
		string = VALUE_STRING(stringContainer.value);\
	}

static NativePack stringInit(VM* vm, int argCount, Value* values) {
	initNativePack;

	ObjString* string;

	if(argCount > 1) {
		char* result = toString(vm, (Value* const) (values + 1u));

		size_t length = strlen(result);

		string = TAKE_STRING(result, length, true);
	} else string = TAKE_STRING("", 0u, false);

	setField(VALUE_INSTANCE(values[0]), stringField, OBJECT_VAL(string));

	pack.value = values[0];

	return pack;
}

static NativePack string__represent__(VM* vm, int argCount, Value* values) {
	NativePack pack;

	stringInstanceString;

	pack.hadError = false;
	pack.value    = OBJECT_VAL(string);

	return pack;
}

static NativePack stringAt(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	if(string -> length == 0u) 
		return pack;

	long long index = 0;

	if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in String.at(index)");
		}

		double value = VALUE_NUMBER(values[1]);

		if(value < LLONG_MIN || value > LLONG_MAX) {
			NATIVE_R_ERR("Invalid index number provided in String.at(index)!");
		}

		index = (long long) value;

		if(llabs(index) < string -> length) 
			index = index >= 0 ? index : string -> length + index;
		else index = index > 0 ? string -> length - 1u : 0;
	}

	char* buffer = ALLOCATE(char, 2u);

	buffer[0] = string -> buffer[index];
	buffer[1] = 0;

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, 1u, true));

	return pack;
}

static NativePack stringAppend(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	size_t size = string -> length,
	      index = 0u;

	for(int i = 1; i < argCount; i++) {
		Value value = values[i];

		if(!IS_STRING(value)) {
			NATIVE_R_ERR("Provided argument no %d is not a list in String.append(strings...)!", i);
		}

		size += VALUE_STRING(value) -> length;
	}

	char* buffer = ALLOCATE(char, size + 1u);

	memcpy(buffer, string -> buffer, string -> length * sizeof(char));

	index += string -> length;

	for(int i = 1; i < argCount; i++) {
		ObjString* value = VALUE_STRING(values[i]);

		memcpy(buffer + index, value -> buffer, value -> length * sizeof(char));

		index += value -> length;
	}

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, size, true));

	return pack;
}

static NativePack stringValue(VM* vm, int argCount, Value* values) {
	NativePack pack;

	stringInstanceString;

	pack.hadError = false;
	pack.value    = OBJECT_VAL(string);

	return pack;
}

static NativePack stringStringify(VM* vm, int argCount, Value* values) {
	return stringValue(vm, argCount, values);
}

static NativePack stringUppercase(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	char* buffer = ALLOCATE(char, string -> length + 1u);

	for(size_t i = 0; i < string -> length; i++) {
		char ch = string -> buffer[i];

		buffer[i] = ch >= 97 && ch <= 123 ? ch & 0xDF : ch;
	}

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, string -> length, true));

	return pack;
}

static NativePack stringLowercase(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	char* buffer = ALLOCATE(char, string -> length + 1u);

	for(size_t i = 0; i < string -> length; i++) {
		char ch = string -> buffer[i];

		buffer[i] = ch >= 65 && ch <= 91 ? ch | 0x20 : ch;
	}

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, string -> length, true));

	return pack;
}

static NativePack _stringStringify(VM* vm, int argCount, Value* values) {
	initNativePack;
	
	char* buffer = toString(vm, (Value* const) (values + 1u));

	size_t length = strlen(buffer);

	vm -> bytesAllocated += (length + 1u) * sizeof(char);

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, length, true));

	return pack;
}

static NativePack stringSplit(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	ObjList* result = newList(vm);

	pack.value = OBJECT_VAL(result);

	if(argCount < 2) {
		result -> capacity = 1u;
		result -> values   = GROW_ARRAY(Value, result -> values, 0u, 1u);
		result -> count    = 1u;

		result -> values[0] = OBJECT_VAL(string);

		return pack;
	}

	char* seperator = toString(vm, (Value* const) values + 1u);

	register uint64_t i = 0u;
	
	size_t length = strlen(seperator);

	// Special case.
	// Achievable also without the special case scenerio.
	// But that will slow down the normal process.

	if(length == 0u) {
		result -> capacity = string -> length;
		result -> values   = GROW_ARRAY(Value, result -> values, 0u, result -> capacity);

		char* buffer;

		for(uint64_t i = 0u; i < string -> length; i++) {
			buffer = ALLOCATE(char, 2u);

			buffer[0] = string -> buffer[i];
			buffer[1] = 0;

			result -> values[result -> count++] = OBJECT_VAL(TAKE_STRING(buffer, 1u, true));
		}

		free(seperator);

		return pack;
	}

	size_t prev = 0u;

	bool splitted = false;
			
	while(i < string -> length) {
		if(string -> buffer[i] == seperator[0] && 
		!memcmp(string -> buffer + i, seperator, length * sizeof(char))) {
			size_t size = i - prev;

			char* buffer = ALLOCATE(char, size + 1u);

			memcpy(buffer, string -> buffer + prev, size * sizeof(char));

			buffer[size] = 0;

			Value value = OBJECT_VAL(TAKE_STRING(buffer, size, true));

			if(result -> count + 1u >= result -> capacity) {
				size_t oldCapacity = result -> capacity;

				result -> capacity = GROW_CAPACITY(oldCapacity);
				result -> values   = GROW_ARRAY(Value, result -> values, oldCapacity, result -> capacity);
			}

			result -> values[result -> count++] = value;

			prev = i = i + length;

			if(!splitted) splitted = true;
		} else i++;
	}

	// Add the final splitted string if the string got splitted.

	if(splitted) {
		length = i - prev;

		char* buffer = ALLOCATE(char, length + 1u);

		memcpy(buffer, string -> buffer + prev, length * sizeof(char));

		buffer[length] = 0;

		result -> values[result -> count++] = OBJECT_VAL(TAKE_STRING(buffer, length, true));
	} else {
		// If the string didn't got split the string is practically stays the same.

		result -> capacity = 1u;
		result -> values   = GROW_ARRAY(Value, result -> values, 0u, 1u);
		result -> count    = 1u;

		result -> values[0] = OBJECT_VAL(string);
	}

	free(seperator);

	return pack;
}

static bool trimable(char ch) {
	switch(ch) {
		case '\n':
		case '\f':
		case '\r':
		case '\b':
		case '\v':
		case '\t':
		case ' ':
			return true;
	}

	return false;
}

static NativePack stringStripRight(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	uint64_t count = 0u;

	for(long long i = string -> length - 1; i >= 0; i--) {
		if(trimable(string -> buffer[i])) 
			count++;
		else break;
	}

	size_t length = string -> length - count;

	char* buffer = ALLOCATE(char, length + 1u);

	memcpy(buffer, string -> buffer, length * sizeof(char));

	buffer[length] = 0;

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, length, true));

	return pack;
}

static NativePack stringStripLeft(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	uint64_t count = 0u;

	for(long long i = 0; i < string -> length; i++) {
		if(trimable(string -> buffer[i])) 
			count++;
		else break;
	}

	size_t length = string -> length - count;

	char* buffer = ALLOCATE(char, length + 1u);

	memcpy(buffer, string -> buffer + count, length * sizeof(char));

	buffer[length] = 0;

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, length, true));

	return pack;
}

static NativePack stringStrip(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	uint64_t countLeft, countRight;

	countLeft = countRight = 0u;

	for(uint64_t i = 0u; i < string -> length; i++) {
		if(trimable(string -> buffer[i])) 
			countLeft++;
		else break;
	}

	for(long long i = string -> length - 1; i >= 0; i--) {
		if(trimable(string -> buffer[i])) 
			countRight++;
		else break;
	}

	size_t length = string -> length - (countLeft + countRight);

	char* buffer = ALLOCATE(char, length + 1u);

	memcpy(buffer, string -> buffer + countLeft, length * sizeof(char));

	buffer[length] = 0;

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, length, true));

	return pack;
}

static NativePack stringSubstr(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	if(argCount < 2) {
		pack.value = OBJECT_VAL(string);
 
		return pack;
	}

	int start = 0, end = string -> length;

	if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in String.substr(start, end)!");
		}

		start = (int) VALUE_NUMBER(values[1]);

		if(abs(start) <= string -> length) 
			start = start >= 0 ? start : string -> length + start;
		else start = start > 0 ? string -> length - 1 : 0;
	}

	if(argCount > 2) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the second argument to be a number in String.substr(start, end)!");
		}

		end = (int) VALUE_NUMBER(values[2]);

		if(abs(end) <= string -> length) 
			end = end >= 0 ? end : string -> length + end;
		else end = end > 0 ? string -> length : 0;
	}

	size_t length = end - start;

	char* buffer = ALLOCATE(char, length + 1u);

	memcpy(buffer, string -> buffer + start, length * sizeof(char));

	buffer[length] = 0;

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, length, true));

	return pack;
}

static NativePack stringPadLeft(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	if(argCount < 2) {
		pack.value = OBJECT_VAL(string);

		return pack;
	}

	if(!IS_NUMBER(values[1])) {
		NATIVE_R_ERR("Expected the first argument to be a number in String.pad_left(length, string)!");
	}

	double value = VALUE_NUMBER(values[1]);

	if(value < 0 || value > LLONG_MAX) {
		NATIVE_R_ERR("Invalid length provided in String.pad_left(length, string)!");
	}

	char* pad = " ";

	size_t padlen = 1u;

	if(argCount > 2) {
		pad = toString(vm, (Value* const) values + 2u);

		padlen = strlen(pad);
	}

	size_t length = (size_t) value, 
	       buflen = length + string -> length;
	
	char* buffer = ALLOCATE(char, buflen + 1u);
	
	if(length < padlen) 
		memcpy(buffer, pad, length * sizeof(char));
	else {
		size_t repeat = length / padlen;

		// Repeat the pad.

		uint64_t i = 0u;

		for(; i < repeat; i++) {
			memcpy(buffer + i * padlen, pad, padlen * sizeof(char));
		}

		memcpy(buffer + i * padlen, pad, (length - repeat * padlen) * sizeof(char));
	}

	memcpy(buffer + length, string -> buffer, string -> length * sizeof(char));

	buffer[buflen] = 0;

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, buflen, true));

	// 'pad' came from toString, thus allocated in heap.

	if(argCount > 2) 
		free(pad);

	return pack;
}

static NativePack stringPadRight(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	if(argCount < 2) {
		pack.value = OBJECT_VAL(string);

		return pack;
	}

	if(!IS_NUMBER(values[1])) {
		NATIVE_R_ERR("Expected the first argument to be a number in String.pad_right(length, string)!");
	}

	double value = VALUE_NUMBER(values[1]);

	if(value < 0 || value > LLONG_MAX) {
		NATIVE_R_ERR("Invalid length provided in String.pad_right(length, string)!");
	}

	char* pad = " ";

	size_t padlen = 1u;

	if(argCount > 2) {
		pad = toString(vm, (Value* const) values + 2u);

		padlen = strlen(pad);
	}

	size_t length = (size_t) value, 
	       buflen = length + string -> length;
	
	char* buffer = ALLOCATE(char, buflen + 1u);

	memcpy(buffer, string -> buffer, string -> length * sizeof(char));
	
	if(length < padlen) 
		memcpy(buffer + string -> length, pad, length * sizeof(char));
	else {
		size_t repeat = length / padlen;

		// Repeat the pad.

		uint64_t i = 0u;

		for(; i < repeat; i++) {
			memcpy(buffer + i * padlen + string -> length, pad, padlen * sizeof(char));
		}

		memcpy(buffer + i * padlen + string -> length, pad, (length - repeat * padlen) * sizeof(char));
	}

	buffer[buflen] = 0;

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, buflen, true));

	// 'pad' came from toString, thus allocated in heap.

	if(argCount > 2) 
		free(pad);

	return pack;
}

static NativePack stringRepeat(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	if(argCount < 2) {
		pack.value = OBJECT_VAL(TAKE_STRING("", 0u, false));

		return pack;
	}

	if(!IS_NUMBER(values[1])) {
		NATIVE_R_ERR("Expected the first argument to be a number in String.repeat(times)!");
	}

	long long times = (long long) VALUE_NUMBER(values[1]);

	if(times < 0) {
		NATIVE_R_ERR("Invalid times value provided in String.repeat(times)!");
	}

	size_t length = string -> length * times;

	char* buffer = ALLOCATE(char, length + 1u);

	for(register long long i = 0; i < times; i++) 
		memcpy(buffer + i * string -> length, string -> buffer, string -> length * sizeof(char));
	
	buffer[length] = 0;

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, length, true));

	return pack;
}

static NativePack stringContains(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call String.contains(string)!");
	}

	char* includes = toString(vm, (Value* const) values + 1u);

	size_t length = strlen(includes);

	for(register uint64_t i = 0u; i < string -> length; i++) {
		if(i < (string -> length + 1u - length) && string -> buffer[i] == includes[0] &&
		!memcmp(string -> buffer + i, includes, length * sizeof(char))) {
			pack.value = BOOL_VAL(true);

			return pack;
		}
	}

	pack.value = BOOL_VAL(false);

	return pack;
}

static NativePack stringIndex(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	pack.value = NUMBER_VAL(-1);

	if(argCount < 2) 
		return pack;
	
	uint8_t occurance = 0;

	if(argCount > 2) {
		if(!IS_NUMBER(values[2])) {
			NATIVE_R_ERR("Expected the second argument to be a number in String:index(string, occurance)!");
		}

		double value = VALUE_NUMBER(values[2]);

		if(value < 0 || value > 1) {
			NATIVE_R_ERR("Invalid occurance value passed in String.index(string, occurance)!");
		}

		occurance = (uint8_t) value;
	}

	char* includes = toString(vm, (Value* const) values + 1u);

	size_t length = strlen(includes);

	if(occurance == 0) {
		for(register uint64_t i = 0u; i < string -> length; i++) {
			if(i < (string -> length + 1u - length) && string -> buffer[i] == includes[0] &&
			!memcmp(string -> buffer + i, includes, length * sizeof(char))) {
				pack.value = NUMBER_VAL(i);

				return pack;
			}
		}
	} else {
		for(register long long i = string -> length - 1; i >= 0; i--) {
			if(i <= (string -> length - length) && string -> buffer[i] == includes[0] &&
			!memcmp(string -> buffer + i, includes, length * sizeof(char))) {
				pack.value = NUMBER_VAL(i);

				return pack;
			}
		}
	}

	return pack;
}

static NativePack stringReplace(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	if(argCount < 2) {
		pack.value = OBJECT_VAL(string);

		return pack;
	}

	char* rep = toString(vm, (Value* const) values + 1u);

	size_t rsiz = strlen(rep);

	char* repwith = "null";

	size_t rwsiz = 4u;

	if(argCount > 2) {
		repwith = toString(vm, (Value* const) values + 2u);

		rwsiz = strlen(repwith);
	}

	uint8_t occurance = 0;

	if(argCount > 3) {
		if(!IS_NUMBER(values[3])) {
			NATIVE_R_ERR("Expected the third argument to be a number in String:replace(str, repstr, occurance)!");
		}

		double value = VALUE_NUMBER(values[3]);

		if(value < 0 || value > 1) {
			NATIVE_R_ERR("Invalid occurance value passed in String.replace(str, repstr, occurance)!");
		} 

		occurance = (uint8_t) value;
	}

	char* result = NULL;

	uint64_t length, capacity;

	capacity = length = 0u;

	long long i;

	bool replaced = false;

	if(occurance == 0) {
		for(i = 0; i < string -> length; i++) {
			if(i <= (string -> length - rsiz) && string -> buffer[i] == rep[0] &&
			!memcmp(string -> buffer + i, rep, rsiz * sizeof(char))) {
				if(result == NULL) {
					capacity = i + rwsiz + 1u;

					result = GROW_ARRAY(char, result, 0u, capacity);
				}

				memcpy(result, string -> buffer, i * sizeof(char));
				memcpy(result + i, repwith, rwsiz * sizeof(char));

				length += i + rwsiz;

				replaced = true;

				break;
			}
		}		
	} else {
		for(i = string -> length - 1; i >= 0; i--) {
			if(i <= (string -> length - rsiz) && string -> buffer[i] == rep[0] &&
			!memcmp(string -> buffer + i, rep, rsiz * sizeof(char))) {
				if(result == NULL) {
					capacity = i + rwsiz + 1u;

					result = GROW_ARRAY(char, result, 0u, capacity);
				}

				memcpy(result, string -> buffer, i * sizeof(char));
				memcpy(result + i, repwith, rwsiz * sizeof(char));

				length += i + rwsiz;

				replaced = true;

				break;
			}
		}
	}

	// Add the remaining string if the string got replaced.

	if(replaced) {
		size_t remainsiz = string -> length - (i + rsiz);

		if(remainsiz + length >= capacity) {
			size_t oldCapacity = capacity;

			capacity = remainsiz + length + 1u;

			result = GROW_ARRAY(char, result, oldCapacity, capacity);
		}

		memcpy(result + length, string -> buffer + i + rsiz, remainsiz * sizeof(char));

		length += remainsiz;

		result[length] = 0;

		pack.value = OBJECT_VAL(TAKE_STRING(result, length, true));
	} else pack.value = OBJECT_VAL(string);

	if(argCount > 2) 
		free(repwith);
	
	free(rep);

	return pack;
}

static NativePack stringReplaceAll(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	if(argCount < 2) {
		pack.value = OBJECT_VAL(string);

		return pack;
	}

	char* rep = toString(vm, (Value* const) values + 1u);

	size_t rsiz = strlen(rep);

	char* repwith = "null";

	size_t rwsiz = 4u;

	if(argCount > 2) {
		repwith = toString(vm, (Value* const) values + 2u);

		rwsiz = strlen(repwith);
	}

	size_t prev, i, length, capacity;

	i = prev = capacity = length = 0u;

	char* buffer = NULL;

	while(i < string -> length) {
		if(i <= (string -> length - rsiz) && string -> buffer[i] == rep[0] &&
		!memcmp(string -> buffer + i, rep, rsiz * sizeof(char))) {
			size_t size = i - prev;

			if(buffer == NULL) {
				capacity = size + rwsiz + 1u;

				buffer = GROW_ARRAY(char, buffer, 0u, capacity);
			}
			else if(size + rwsiz + length >= capacity) {
				size_t oldCapacity = capacity;

				capacity += size + rwsiz + 1u;

				buffer = GROW_ARRAY(char, buffer, oldCapacity, capacity);
			}

			memcpy(buffer + length, string -> buffer + prev, size * sizeof(char));
			memcpy(buffer + length + size, repwith, rwsiz * sizeof(char));

			i = prev = i + rsiz;

			length += size + rwsiz;
		} else i++;
	}

	// Add the final portion of the string if at least one part of the string is replaced.

	if(buffer != NULL) {
		size_t size = i - prev;

		if(size + length >= capacity) {
			size_t oldCapacity = capacity;

			capacity += size + 1u;

			buffer = GROW_ARRAY(char, buffer, oldCapacity, capacity);
		}

		memcpy(buffer + length, string -> buffer + prev, size * sizeof(char));

		length += size;

		buffer[length] = 0;

		buffer = GROW_ARRAY(char, buffer, capacity, length + 1u);

		pack.value = OBJECT_VAL(TAKE_STRING(buffer, length, true));
	} else pack.value = OBJECT_VAL(string);

	free(rep);

	if(argCount > 2) 
		free(repwith);

	return pack;
}

static NativePack stringCodeAt(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	if(string -> length == 0u) {
		pack.value = NUMBER_VAL(NAN);

		return pack;
	}

	long long index = 0;

	if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected the first argument to be a number in String.code_at(index)");
		}

		double value = VALUE_NUMBER(values[1]);

		if(value < LLONG_MIN || value > LLONG_MAX) {
			NATIVE_R_ERR("Invalid index number provided in String.code_at(index)!");
		}

		index = (long long) value;

		if(llabs(index) < string -> length) 
			index = index >= 0 ? index : string -> length + index;
		else index = index > 0 ? string -> length - 1u : 0;
	}

	pack.value = NUMBER_VAL((double) string -> buffer[index]);

	return pack;
}

static NativePack stringFromCode(VM* vm, int argCount, Value* values) {
	initNativePack;

	if(argCount < 2) {
		pack.value = OBJECT_VAL(TAKE_STRING("", 0u, false));

		return pack;
	}

	size_t size = argCount - 1;

	char* buffer = ALLOCATE(char, size + 1);

	for(int i = 1; i < argCount; i++) {
		double value = VALUE_NUMBER(values[i]);

		if(value < CHAR_MIN || value > CHAR_MAX) {
			NATIVE_R_ERR("Invalid code point provided in String::from_code(codes...)!");
		}

		buffer[i - 1] = (char) value;
	}

	buffer[size] = 0;

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, size, true));

	return pack;
}

static NativePack stringStartsWith(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call String.starts_with(string, index)!");
	}

	char* match = toString(vm, (Value* const) values + 1u);

	size_t length = strlen(match);

	uint64_t index = 0u;

	if(argCount > 2) {
		if(!IS_NUMBER(values[2])) {
			NATIVE_R_ERR("Expected the index to be a number in String.starts_with(string, index)!");
		}

		double value = VALUE_NUMBER(values[2]);

		if(fabs(value) < string -> length) 
			value = value >= 0 ? value : string -> length + value;
		else value = value > 0 ? string -> length : 0;

		index = (uint64_t) value;
	}

	if(string -> length - index >= length && !memcmp(string -> buffer + index, match, length * sizeof(char))) {
		pack.value = BOOL_VAL(true);

		free(match);

		return pack;
	}

	pack.value = BOOL_VAL(false);

	free(match);

	return pack;
}

static NativePack stringEndsWith(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call String.ends_with(string, length)!");
	}

	char* match = toString(vm, (Value* const) values + 1u);

	size_t size = strlen(match);

	uint64_t length = string -> length;

	if(argCount > 2) {
		if(!IS_NUMBER(values[2])) {
			NATIVE_R_ERR("Expected the index to be a number in String.ends_with(string, length)!");
		}

		double value = VALUE_NUMBER(values[2]);

		if(value < 0) {
			NATIVE_R_ERR("The provided length cannot be negative in String.ends_with(string, length)!");
		}

		length = (uint64_t) (value > string -> length ? string -> length : value);
	}

	pack.value = BOOL_VAL(false);

	if(size > length) {
		free(match);

		return pack;
	}

	if(!memcmp(string -> buffer + length - size, match, size * sizeof(char))) {
		free(match);

		pack.value = BOOL_VAL(true);

		return pack;
	}

	free(match);

	return pack;
}

static NativePack stringReverse(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	char* buffer = ALLOCATE(char, string -> length + 1u);

	for(uint64_t i = 0u; i < string -> length; i++) 
		buffer[string -> length - (i + 1u)] = string -> buffer[i];
	
	buffer[string -> length] = 0;

	pack.value = OBJECT_VAL(TAKE_STRING(buffer, string -> length, true));

	return pack;
}

static NativePack stringSlice(VM* vm, int argCount, Value* values) {
	initNativePack;

	stringInstanceString;

	int start = 0, end = string -> length;

	int steps = 1u;

	if(argCount > 1) {
		if(!IS_NUMBER(values[1])) {
			NATIVE_R_ERR("Expected a number as a first argument in List.slice(start, end, steps)!");
		}

		start = (int) VALUE_NUMBER(values[1]);

		if(abs(start) <= string -> length) 
			start = start >= 0 ? start : string -> length + start;
		else start = start >= 0 ? string -> length : 0;
	}

	if(argCount > 2) {
		if(!IS_NUMBER(values[2])) {
			NATIVE_R_ERR("Expected a number as the second argument in List.slice(start, end, steps)!");
		}

		end = (int) VALUE_NUMBER(values[2]);

		if(abs(end) <= string -> length) 
			end = end >= 0 ? end : string -> length + end;
		else end = end >= 0 ? string -> length : 0;
	}

	if(argCount > 3) {
		if(!IS_NUMBER(values[3])) {
			NATIVE_R_ERR("Expected a number as steps in List.slice(start, end, steps)!");
		}

		steps = (int) VALUE_NUMBER(values[3]);
		
		if(steps < 0) {
			NATIVE_R_ERR("Steps cannot be negative in List.slice(start, end, steps)!");
		}

		steps = steps > string -> length ? (end - start) : steps;
	}

	if(end <= start) {
		pack.value = OBJECT_VAL(TAKE_STRING("", 0u, false));

		return pack;
	}

	size_t size = (end - start) / steps;

	char* buffer = ALLOCATE(char, size + 1u);

	for(uint64_t i = 0u; start < end; start += steps, i++) 
		buffer[i] = string -> buffer[start];
	
	pack.value = OBJECT_VAL(TAKE_STRING(buffer, size, true));

	return pack;
}

void initStringLib(VM* vm) {
	stringField = TAKE_STRING("_p_string__", 11u, false);

	ObjString* name = TAKE_STRING("String", 6u, false);

	ObjClass* stringClass = newClass(vm, name);

	vmStringClass = stringClass;

	ObjNative* method;

	defineMethod(stringClass, vm -> initString, stringInit);

	defineMethod(stringClass, TAKE_STRING("at", 2u, false), stringAt);
	defineMethod(stringClass, TAKE_STRING("append", 6u, false), stringAppend);
	defineMethod(stringClass, TAKE_STRING("value", 5u, false), stringValue);
	defineMethod(stringClass, TAKE_STRING("stringify", 9u, false), stringStringify);
	defineMethod(stringClass, TAKE_STRING("uppercase", 9u, false), stringUppercase);
	defineMethod(stringClass, TAKE_STRING("lowercase", 9u, false), stringLowercase);
	defineMethod(stringClass, TAKE_STRING("split", 5u, false), stringSplit);
	defineMethod(stringClass, TAKE_STRING("strip_right", 11u, false), stringStripRight);
	defineMethod(stringClass, TAKE_STRING("strip_left", 10u, false), stringStripLeft);
	defineMethod(stringClass, TAKE_STRING("strip", 5u, false), stringStrip);
	defineMethod(stringClass, TAKE_STRING("substr", 6u, false), stringSubstr);
	defineMethod(stringClass, TAKE_STRING("pad_left", 8u, false), stringPadLeft);
	defineMethod(stringClass, TAKE_STRING("pad_right", 9u, false), stringPadRight);
	defineMethod(stringClass, TAKE_STRING("repeat", 6u, false), stringRepeat);
	defineMethod(stringClass, TAKE_STRING("contains", 8u, false), stringContains);
	defineMethod(stringClass, TAKE_STRING("index", 5u, false), stringIndex);
	defineMethod(stringClass, TAKE_STRING("replace", 7u, false), stringReplace);
	defineMethod(stringClass, TAKE_STRING("replace_all", 11u, false), stringReplaceAll);
	defineMethod(stringClass, TAKE_STRING("code_at", 7u, false), stringCodeAt);
	defineMethod(stringClass, TAKE_STRING("reverse", 7u, false), stringReverse);
	defineMethod(stringClass, TAKE_STRING("starts_with", 11u, false), stringStartsWith);
	defineMethod(stringClass, TAKE_STRING("ends_with", 9u, false), stringEndsWith);
	defineMethod(stringClass, TAKE_STRING("slice", 5u, false), stringSlice);
	defineMethod(stringClass, TAKE_STRING("__represent__", 13u, false), string__represent__);

	defineStaticMethod(stringClass, TAKE_STRING("stringify", 9u, false), _stringStringify);
	defineStaticMethod(stringClass, TAKE_STRING("from_code", 9u, false), stringFromCode);

	// For String.index(string, occurance).

	setStatic(stringClass, TAKE_STRING("FIRST_OCCURANCE", 15u, false), NUMBER_VAL(0));
	setStatic(stringClass, TAKE_STRING("LAST_OCCURANCE", 14u, false), NUMBER_VAL(1));

	tableInsert(&vm -> globals, name, (ValueContainer) { OBJECT_VAL(stringClass), true });
}

ObjString* dictionaryField;

extern ObjClass* vmDictionaryClass;

#define dictInstanceDict if(!IS_DICTIONARY(values[0]) && (!IS_INSTANCE(values[0]) || VALUE_INSTANCE(values[0]) -> klass != vmDictionaryClass)) {\
	NATIVE_R_ERR("Provided reciever/instance (this) must be dictionary or instance of Dictionary(dict)!");\
}\
	ObjDictionary* dict;\
	if(IS_DICTIONARY(values[0])) dict = VALUE_DICTIONARY(values[0]);\
	else {\
		ObjInstance* instance = VALUE_INSTANCE(values[0]);\
		ValueContainer dictContainer;\
		getField(instance, dictionaryField, dictContainer);\
		dict = VALUE_DICTIONARY(dictContainer.value);\
	}

static NativePack dictInit(VM* vm, int argCount, Value* values) {
	initNativePack;

	pack.value = values[0];

	ObjDictionary* result;

	if(argCount < 2) {
		result = newDictionary(vm);

		setField(VALUE_INSTANCE(values[0]), dictionaryField, OBJECT_VAL(result));

		return pack;
	}

	if(!IS_DICTIONARY(values[1])) {
		NATIVE_R_ERR("The provided value is not a dictionary in Dictionary.init(dict)!");
	}

	result = newDictionary(vm);

	tableInsertAll(&VALUE_DICTIONARY(values[1]) -> fields, &result -> fields);

	setField(VALUE_INSTANCE(values[0]), dictionaryField, OBJECT_VAL(result));

	return pack;
}

static NativePack dictKeys(VM* vm, int argCount, Value* values) {
	initNativePack;

	dictInstanceDict;

	ObjList* result = newList(vm);

	result -> capacity = dict -> fields.count;
	result -> values   = GROW_ARRAY(Value, result -> values, 0u, result -> capacity);

	for(uint64_t i = 0u; i < dict -> fields.capacity; i++) {
		ObjString* key = dict -> fields.entries[i].key;

		if(key != NULL) 
			result -> values[result -> count++] = OBJECT_VAL(key);
	}

	pack.value = OBJECT_VAL(result);

	return pack;
}

static NativePack dictHasKey(VM* vm, int argCount, Value* values) {
	initNativePack;

	dictInstanceDict;

	pack.value = BOOL_VAL(false);

	if(argCount < 2) 
		return pack;

	char* result = toString(vm, (Value* const) values + 1u);
	size_t size  = strlen(result);

	vm -> bytesAllocated += size + 1u;

	ObjString* property = TAKE_STRING(result, size, true);

	for(register uint64_t i = 0u; i < dict -> fields.capacity; i++) {
		if(property == dict -> fields.entries[i].key) {
			pack.value = BOOL_VAL(true);

			return pack;
		}
	}

	return pack;
}

static NativePack dictValues(VM* vm, int argCount, Value* values) {
	initNativePack;

	dictInstanceDict;

	ObjList* result = newList(vm);

	result -> capacity = dict -> fields.count;
	result -> values   = GROW_ARRAY(Value, result -> values, 0u, result -> capacity);

	for(register uint64_t i = 0u; i < dict -> fields.capacity; i++) {
		Entry* entry = dict -> fields.entries + i;

		if(entry -> key != NULL) 
			result -> values[result -> count++] = entry -> valueContainer.value;
	}

	pack.value = OBJECT_VAL(result);

	return pack;
}

static NativePack dictValue(VM* vm, int argCount, Value* values) {
	NativePack pack;

	dictInstanceDict;

	pack.hadError = false;
	pack.value    = OBJECT_VAL(dict);

	return pack;
}

static NativePack dictStringify(VM* vm, int argCount, Value* values) {
	initNativePack;

	pack.value = OBJECT_VAL(TAKE_STRING("{ Dictionary }", 14u, false));

	return pack;
}

static NativePack dict__represent__(VM* vm, int argCount, Value* values) {
	return dictValue(vm, argCount, values);
}

void initDictionaryLib(VM* vm) {
	dictionaryField = TAKE_STRING("_p_dictionary__", 15u, false);

	ObjString* name = TAKE_STRING("Dictionary", 10u, false);

	ObjClass* dictionaryClass = newClass(vm, name);

	vmDictionaryClass = dictionaryClass;

	ObjNative* method;

	defineMethod(dictionaryClass, vm -> initString, dictInit);

	defineMethod(dictionaryClass, TAKE_STRING("keys", 4u, false), dictKeys);
	defineMethod(dictionaryClass, TAKE_STRING("has_key", 7u, false), dictHasKey);
	defineMethod(dictionaryClass, TAKE_STRING("values", 6u, false), dictValues);
	defineMethod(dictionaryClass, TAKE_STRING("value", 5u, false), dictValue);
	defineMethod(dictionaryClass, TAKE_STRING("stringify", 9u, false), dictStringify);
	defineMethod(dictionaryClass, TAKE_STRING("__represent__", 13u, false), dict__represent__);

	tableInsert(&vm -> globals, name, (ValueContainer) { OBJECT_VAL(dictionaryClass), true });
}

static NativePack funcBind(VM* vm, int argCount, Value* values) {
	initNativePack;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call Function::bind(method, reciever)!");
	}

	if(!IS_BOUND_MEHTOD(values[1])) {
		NATIVE_R_ERR("The provided value must be a method/bound-method in Function::bind(method, reciever)!");
	}

	ObjBoundMethod* method = VALUE_BOUND_METHOD(values[1]);

	Value reciever = method -> reciever;

	if(argCount > 2) 
		reciever = values[2];

	pack.value = OBJECT_VAL(newBoundMethod(vm, reciever, method -> function));

	return pack;
}

static NativePack funcCall(VM* vm, int argCount, Value* values) {
	initNativePack;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call Function::call(method, reciever, args...)!");
	}

	if(!IS_BOUND_MEHTOD(values[1])) {
		NATIVE_R_ERR("The provided value must be a method/bound-method in Function::call(method, reciever, args...)!");
	}

	ObjBoundMethod* method = VALUE_BOUND_METHOD(values[1]);

	Value reciever = method -> reciever;

	if(argCount > 2) 
		reciever = values[2];

	stack_push(vm, reciever);

	for(int i = 3; i < argCount; i++) 
		stack_push(vm, values[i]);
	
	if(callValue(vm, OBJECT_VAL(method -> function), argCount - 3) && run(vm) != INTERPRET_RUNTIME_ERROR) 
		pack.value = stack_pop(vm);
	else pack.hadError = true;

	return pack;
}

static NativePack funcPass(VM* vm, int argCount, Value* values) {
	initNativePack;

	if(argCount < 2) {
		NATIVE_R_ERR("Too few arguments to call Function::pass(method, reciever, args_list)!");
	}

	if(!IS_BOUND_MEHTOD(values[1])) {
		NATIVE_R_ERR("The provided value must be a method/bound-method in Function::pass(method, reciever, args_list)!");
	}

	ObjBoundMethod* method = VALUE_BOUND_METHOD(values[1]);

	Value reciever = method -> reciever;

	if(argCount > 2) 
		reciever = values[2];

	stack_push(vm, reciever);

	if(argCount > 3) {
		if(!IS_LIST(values[3])) {
			NATIVE_R_ERR("Expected the third argument to be a list in Function::pass(method, reciever, args_list)!");
		}
	}

	ObjList* args = VALUE_LIST(values[3]);

	for(uint64_t i = 0u; i < args -> count; i++) 
		stack_push(vm, args -> values[i]);
	
	if(callValue(vm, OBJECT_VAL(method -> function), args -> count) && run(vm) != INTERPRET_RUNTIME_ERROR) 
		pack.value = stack_pop(vm);
	else pack.hadError = true;

	return pack;

	return pack;
}

extern ObjClass* vmFunctionClass;

void initFunctionLib(VM* vm) {
	ObjString* name = TAKE_STRING("Function", 8u, false);

	ObjClass* functionClass = newClass(vm, name);

	vmFunctionClass = functionClass;

	ObjNative* method;

	defineStaticMethod(functionClass, TAKE_STRING("bind", 4u, false), funcBind);
	defineStaticMethod(functionClass, TAKE_STRING("call", 4u, false), funcCall);
	defineStaticMethod(functionClass, TAKE_STRING("pass", 4u, false), funcPass);

	tableInsert(&vm -> globals, name, (ValueContainer) { OBJECT_VAL(functionClass), true });
}

/** Values are ignored in GC. */

void gcLibIgnore() {
	// File library.

	markObject((Obj*) fileField);
	markObject((Obj*) modeField);
	markObject((Obj*) locationField);
	markObject((Obj*) sizeField);
	markObject((Obj*) deniedStdin);
	markObject((Obj*) deniedStdout);
	markObject((Obj*) deniedStderr);

	// Time library.

	markObject((Obj*) secField);
	markObject((Obj*) usecField);
	markObject((Obj*) tzoneField);

	// Number library.

	markObject((Obj*) numberField);

	// List library.

	markObject((Obj*) listField);

	// String library.

	markObject((Obj*) stringField);

	// Dictionary library.

	markObject((Obj*) dictionaryField);
}
