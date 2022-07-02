#include <string.h>

#include "compiler.h"
#include "scanner.h"
#include "object.h"

#define UINT24_MAX 0xFFFFFF
#define UINT24_COUNT (UINT24_MAX + 1u)

#define MAX_CASES 512u

typedef struct {
	Token previous;
	Token current;
	bool hadError;
	bool panicMode;
} Parser;

typedef struct {
	bool isConst;
	bool isCaptured;
	int depth;
	Token name;
} Local;

typedef struct {
	uint32_t codePoint;
	uint32_t stride;
	int depth;
} PointData;

typedef enum {
	TYPE_FUNCTION,
	TYPE_METHOD,
	TYPE_INITIALIZER,
	TYPE_STATIC,
	TYPE_PROGRAM
} FunctionType;

typedef struct {
	bool isLocal;
	bool isConst;
	uint32_t index:24;
} Upvalue;

typedef struct Compiler {
	struct Compiler* enclosing;
	
	ObjFunction* function;
	FunctionType type;

	Local* locals;
	uint32_t localCount;
	uint32_t localCapacity;
	int scopeDepth;

	Upvalue upvalues[512u];

	struct {
		PointData* data;
		size_t count;
		size_t capacity;
	} requests;

	int loopDepth;
} Compiler;

typedef struct ClassCompiler {
	struct ClassCompiler* enclosing;
	bool hasSuperClass;
	bool inStatic;
} ClassCompiler;

static Parser parser;

static Compiler* current = NULL;
static ClassCompiler* currentClass = NULL;

static char* relativePath = NULL;
static size_t relativePathSize = 0u;

typedef enum {
	PREC_NONE, 
	PREC_ASSIGNMENT,    // =
	PREC_OR,            // ||, |
	PREC_AND,           // &&, &
	PREC_EQUALITY,      // ==, !=
	PREC_COMPARISON,    // <, >, <=, >=
	PREC_TERM,          // +, -
	PREC_FACTOR,        // /, *
	PREC_UNARY,         // ~, !, -
	PREC_CALL,          // ., ()
	PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	ParseFn mixfix;
	Precedence precedence;
} ParseRule;

static Scanner* globalScanner = NULL;
static VM*      globalVM      = NULL;

static Chunk* currentChunk() {
	return &current -> function -> chunk;
}

static Scanner* getScanner() {
	return globalScanner;
}

static VM* getVM() {
	return globalVM;
}

static void pushRequest(uint32_t codePoint) {
	PointData pointData;

	pointData.codePoint = codePoint;
	pointData.depth     = current -> loopDepth;
	pointData.stride    = 0u;
	
	if(current -> localCount > 0u) {
		for(int i = current -> localCount - 1u; i >= 0; i--) {
			if(current -> locals[i].depth == current -> scopeDepth) 
				pointData.stride++;
		}
	}

	if(current -> requests.count + 1u > current -> requests.capacity) {
		size_t oldCapacity = current -> requests.capacity;

		current -> requests.capacity = GROW_CAPACITY(current -> requests.capacity);
		current -> requests.data     = GROW_ARRAY(PointData, current -> requests.data, oldCapacity, current -> requests.capacity);
	}

	current -> requests.data[current -> requests.count++] = pointData;
}

// Forward declaration of addLocal.

static void advance();
static void addLocal(const Token*, bool);

static void initCompiler(Compiler* compiler, FunctionType type, bool inExpr) {
	compiler -> enclosing = current;
	
	compiler -> function = NULL;
	compiler -> type     = type;

	compiler -> localCount    = 0u;
	compiler -> localCapacity = 0u;
	compiler -> scopeDepth    = 0;
	compiler -> locals        = NULL;

	compiler -> requests.data     = NULL;
	compiler -> requests.count    = 0u;
	compiler -> requests.capacity = 0u;

	compiler -> loopDepth = 0;

	compiler -> function = newFunction(getVM());
	
	compiler -> function -> upvalueCount = 0u;

	current = compiler;

	Token token;

	token.start  = NULL;
	token.length = 0u;

	addLocal(&token, false);
	
	if(!inExpr) {
		if(type != TYPE_PROGRAM) 
			compiler -> function -> name = copyString(getVM(), parser.previous.start, parser.previous.length);
		else compiler -> function -> name = takeString(getVM(), "main", 4, false);
	}
	else if(parser.current.type == TOKEN_IDENTIFIER) {
		advance();

		compiler -> function -> name = copyString(getVM(), parser.previous.start, parser.previous.length);
	}
	
	Local* local = &current -> locals[current -> localCount - 1u];
	
	local -> isCaptured = false;
	local -> depth      = 0;
	
	if(type != TYPE_FUNCTION && type != TYPE_STATIC) {
		local -> name.start  = "this";
		local -> name.length = 4;
	} else {
		local -> name.start  = "";
		local -> name.length = 0;
	}
}

static void freeCompiler(Compiler* compiler) {
	FREE_ARRAY(Local, compiler -> locals, compiler -> localCapacity);
	FREE_ARRAY(PointData, compiler -> requests.data, compiler -> requests.capacity);
}

static void errorAt(Token* token, const char* message) {
	if(parser.panicMode) return;

	parser.panicMode = true;

	fprintf(stderr, "[Error][Compilation][Line %d]", token -> line);

	if(token -> type == TOKEN_EOF) 
		fprintf(stderr, " in the end");
	else if(token -> type != TOKEN_ERROR) 
		fprintf(stderr, " at '%.*s'", token -> length, token -> start);

	fprintf(stderr, ": %s\n", message);

	parser.hadError = true;
}

static void errorAtCurrent(const char* message) {
	errorAt(&parser.current, message);
}

static void error(const char* message) {
	errorAt(&parser.previous, message);
}

static void advance() {
	parser.previous = parser.current;

	while(true) {
		parser.current = scanToken(getScanner());

		if(parser.current.type != TOKEN_ERROR) break;

		errorAtCurrent(parser.current.start);
	}
}

static bool check(TokenType type) {
	return parser.current.type == type;
}

static bool match(TokenType type) {
	if(!check(type)) 
		return false;

	advance();

	return true;
}

static Token syntheticToken(const char* text) {
	Token token;

	token.start  = text;
	token.length = (int) strlen(text);
	
	return token;
}

static void consume(TokenType tokenType, const char* message) {
	if(parser.current.type == tokenType) {
		advance();

		return;
	}

	errorAtCurrent(message);
}

static void emitByte(uint8_t byte) {
	writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte(byte1);
	emitByte(byte2);
}

static void emitReturn() {
	if(current -> type == TYPE_INITIALIZER) 
		emitBytes(OP_GET_LOCAL, 0u);
	else emitByte(OP_NULL);
	
	emitByte(OP_RETURN);
}

static ObjFunction* endCompiler() {
	emitReturn();

	ObjFunction* function = current -> function;

	freeCompiler(current);

	current = current -> enclosing;

	return function;
}

static void emitConstant(Value value) {
	writeConstant(currentChunk(), value, parser.previous.line);
}

static void number(bool canAssign) {
	double value = strtod(parser.previous.start, NULL);

	emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
	emitConstant(OBJECT_VAL(copyString(getVM(), parser.previous.start, parser.previous.length)));
}

// Forward declarations.

static ParseRule* getRule(const TokenType);
static void binary(bool);
static void and(bool);
static void or(bool);
static void statement(bool);
static void takeDeclaration(bool);
static void declaration(bool);
static void fnExpr(bool);
static void constFnExpr(bool);
static void globalInterpolation(bool);
static void ternary();

static void parseInfix(bool canAssign, Precedence precedence) {
	while(precedence <= getRule(parser.current.type) -> precedence) {
		advance();

		ParseFn infixFn = getRule(parser.previous.type) -> infix;

		infixFn(canAssign);
	}
	
	if(canAssign && match(TOKEN_EQUAL)) 
		error("Invalid left-hand assignment target!");
	
	if(canAssign && match(TOKEN_QUESTION)) 
		ternary();
	
	if(canAssign && match(TOKEN_INCREMENT)) 
		error("Invalid use of post increment operator! Target should be a valid identifier!");
	else if(canAssign && match(TOKEN_DECREMENT)) 
		error("Invalid use of post decrement operator! Target should be a valid identifier!");
}

static void parsePrecedence(Precedence precedence) {
	advance();

	ParseFn prefixFn = getRule(parser.previous.type) -> prefix;

	if(prefixFn == NULL) {
		error("Expected an expression!");
		return;
	}

	bool canAssign = precedence <= PREC_ASSIGNMENT;

	prefixFn(canAssign);

	parseInfix(canAssign, precedence);
}

static void expression() {
	parsePrecedence(PREC_ASSIGNMENT);
}

static void semicolon(const char* message, bool multiple) {
	if(!check(TOKEN_RIGHT_BRACE)) {
		consume(TOKEN_SEMICOLON, message);

		while(multiple && match(TOKEN_SEMICOLON));
	}
}

static void literal(bool canAssign) {
	switch(parser.previous.type) {
		case TOKEN_FALSE:    emitByte(OP_FALSE); break;
		case TOKEN_TRUE:     emitByte(OP_TRUE); break;
		case TOKEN_NULL:     emitByte(OP_NULL); break;
		case TOKEN_INFINITY: emitByte(OP_INFINITY); break;
		case TOKEN_NAN:      emitByte(OP_NAN); break;
		default: return;
	}
}

static void unary(bool canAssign) {
	TokenType operatorType = parser.previous.type;

	parsePrecedence(PREC_UNARY);

	switch(operatorType) {
		case TOKEN_MINUS: emitByte(OP_NEGATE); break;
		case TOKEN_NOT:   emitByte(OP_NOT); break;
		case TOKEN_TILDE: emitByte(OP_BITWISE_NEGATION); break;
		default: return;
	}
}

static void grouping(bool canAssign) {
	expression();

	consume(TOKEN_RIGHT_PAREN, "Expeced a ')' after end of expression!");
}

#define NEEDLE_BYTES(n) \
	int b4 = (int) n;\
	uint8_t c = b4 & 0xFF;\
	b4 >>= 0x8;\
	uint8_t b = b4 & 0xFF;\
	b4 >>= 0x8;\
	uint8_t a = b4 & 0xFF;\
	emitByte(a);\
	emitByte(b);\
	emitByte(c);\

#define SET_N_GET(code, global) \
	if(global > 255u) {\
		emitByte(code##_LONG);\
		NEEDLE_BYTES(global)\
	}\
	emitBytes(code, (uint8_t) global);

#define DEFINE(code, global, cnst) \
	if(global > 255u) {\
		emitByte(code##_LONG);\
		emitByte((uint8_t) cnst);\
		NEEDLE_BYTES(global)\
	}\
	emitByte(code);\
	emitBytes((uint8_t) cnst, (uint8_t) global);
	
static void checkConstantPoolOverflow(const char* errorMessage) {
	Chunk* chunk = currentChunk();

	size_t size = chunk -> constants.count;

	if(size >= (0xFFFFFF + 1u)) {
		fprintf(stderr, "[Error][Fatal][Line %d]: %s", parser.previous.line, errorMessage);
		exit(-1);
	}
}

static void addLocal(const Token* token, bool isConst) {
	if(current -> localCount + 1u > UINT24_COUNT) {
		error("Too many locals in a scope! Please be gentle!");
		return;
	}

	if(current -> localCount + 1u > current -> localCapacity) {
		uint32_t oldCapacity = current -> localCapacity;

		current -> localCapacity = GROW_CAPACITY(oldCapacity);
		current -> locals        = GROW_ARRAY(Local, current -> locals, oldCapacity, current -> localCapacity);
	}

	Local* local = current -> locals + current -> localCount++;

	local -> name       = *token;
	local -> depth      = -1;
	local -> isCaptured = false;
	local -> isConst    = isConst;
}

static bool identifiersEqual(Token* token1, Token* token2) {
	return token1 -> length == token2 -> length && !memcmp(token1 -> start, token2 -> start, token1 -> length);
}

static void declareVariable(bool isConst) {
	if(current -> scopeDepth == 0) 
		return;

	Token* token = &parser.previous;

	for(int i = current -> localCount - 1u; i >= 0; i--) {
		Local* local = current -> locals + i;

		if(local -> depth != -1 && local -> depth < current -> scopeDepth) 
			break;

		if(identifiersEqual(token, &local -> name)) {
			if(current -> scopeDepth == local -> depth) {
				char* buffer = malloc((50u + token -> length) * sizeof(char));

				sprintf(buffer, "Redefinition of local variable '%.*s' in same scope!", token -> length, token -> start);

				error(buffer);

				free(buffer);
			}
		}
	}
	
	addLocal(token, isConst);
}

static size_t parseVariable(const char* errorMessage, bool isConst) {
	Chunk* chunk = currentChunk();
	
	consume(TOKEN_IDENTIFIER, errorMessage);

	declareVariable(isConst);

	if(current -> scopeDepth > 0) 
		return 0;
	
	checkConstantPoolOverflow("Too many global variables!");

	return writeValueArray(&chunk -> constants, OBJECT_VAL(copyString(getVM(), parser.previous.start, parser.previous.length)));
}

static int resolveLocal(Compiler* compiler, Token* token) {
	for(int i = compiler -> localCount - 1u; i >= 0; i--) {
		Local* local = compiler -> locals + i;
		
		if(local -> depth != -1 && local -> depth <= current -> scopeDepth && identifiersEqual(&local -> name, token)) 
			return i;
	}
	
	return -1;
}

#define INC_DC(code, type, arg) \
	emitByte(code);\
	if(arg > 255u) {\
		emitByte((uint8_t) (type + 1));\
		NEEDLE_BYTES(arg);\
	} else emitBytes(type, (uint8_t) arg);


static bool checkBinaryToken() {
	return check(TOKEN_ASTERISK) || 
	       check(TOKEN_PLUS) || 
		   check(TOKEN_MINUS) || 
		   check(TOKEN_SLASH) || 
		   check(TOKEN_BITWISE_AND) || 
		   check(TOKEN_BITWISE_OR) || 
		   check(TOKEN_RIGHT_SHIFT) || 
		   check(TOKEN_LEFT_SHIFT) ||
		   check(TOKEN_MODULUS) ||
		   check(TOKEN_XOR);
}

static void binaryOpcode(TokenType type) {
	switch(type) {
		case TOKEN_EQUAL_EQUAL:    emitByte(OP_EQUAL); break;
		case TOKEN_NOT_EQUAL:      emitByte(OP_NOT_EQUAL); break;
		case TOKEN_GREATER:        emitByte(OP_GREATER); break;
		case TOKEN_GREATER_EQUAL:  emitByte(OP_GREATER_EQUAL); break;
		case TOKEN_LESS:           emitByte(OP_LESS); break;
		case TOKEN_LESS_EQUAL:     emitByte(OP_LESS_EQUAL); break;
		
		case TOKEN_PLUS:           emitByte(OP_ADD); break;
		case TOKEN_MINUS:          emitByte(OP_SUBSTRACT); break;
		case TOKEN_SLASH:          emitByte(OP_DIVIDE); break;
		case TOKEN_ASTERISK:       emitByte(OP_MULTIPLY); break;
		case TOKEN_MODULUS:        emitByte(OP_MODULUS); break;
		
		case TOKEN_BITWISE_AND:    emitByte(OP_BITWISE_AND); break;
		case TOKEN_BITWISE_OR:     emitByte(OP_BITWISE_OR); break;
		case TOKEN_RIGHT_SHIFT:    emitByte(OP_RIGHT_SHIFT); break;
		case TOKEN_LEFT_SHIFT:     emitByte(OP_LEFT_SHIFT); break;
		case TOKEN_XOR:            emitByte(OP_XOR); break;
		case TOKEN_INSTOF:         emitByte(OP_INSTOF); break;
		default: return;
	}
}

static int addUpvalue(Compiler* compiler, int index, bool isLocal, bool isConst) {
	int upvalueCount = compiler -> function -> upvalueCount;

	if(upvalueCount == 512u) {
		error("Too many closing variables in a closure!");

		return 0;
	}

	for(int i = 0; i < upvalueCount; i++) {
		Upvalue* upvalue = compiler -> upvalues + i;

		if(upvalue -> index == index && upvalue -> isLocal == isLocal) 
			return i;
	}

	compiler -> upvalues[upvalueCount].isLocal = isLocal;
	compiler -> upvalues[upvalueCount].index   = index;
	compiler -> upvalues[upvalueCount].isConst = isConst;

	return compiler -> function -> upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
	if(compiler -> enclosing == NULL) 
		return -1;

	int local = resolveLocal(compiler -> enclosing, name);

	if(local != -1) {
		compiler -> enclosing -> locals[local].isCaptured = true;
		
		return addUpvalue(compiler, local, true, compiler -> enclosing -> locals[local].isConst);
	}

	int upvalue = resolveUpvalue(compiler -> enclosing, name);

	if(upvalue != -1) 
		return addUpvalue(compiler, upvalue, false, compiler -> enclosing -> upvalues[upvalue].isConst);
	
	return -1;
}

static size_t identifierConstant(Token* token) {
	return writeValueArray(&currentChunk() -> constants, OBJECT_VAL(copyString(getVM(), token -> start, token -> length)));
}

#define SET_N_GET_OPT(code, arg) \
	if(arg <= 20) \
		emitByte(code + 2 + arg);\
	else {\
		SET_N_GET(code, arg)\
	}

static void namedVariable(Token* name, bool canAssign, bool globalOnly) {
	Chunk* chunk = currentChunk();

	checkConstantPoolOverflow("Constant pool overflow while getting/setting variable!");
	
	uint8_t state = 0u;
	
	int arg = !globalOnly ? resolveLocal(current, name) : -1;
	
	if(arg == -1) {
		if(!globalOnly && (arg = resolveUpvalue(current, name)) != -1) state = 2u;
		else {
			state = 1u;
			arg = (int) identifierConstant(name);
		}
	}
	
	if(canAssign && match(TOKEN_EQUAL)) {
		if(state == 0u) {
			// Check if the local is constant. If it is, throw a compile time error.
			
			if(current -> locals[arg].isConst) {
				error("Attempt to assign a value to a constant local!");
			}
			
			expression();

			SET_N_GET_OPT(OP_SET_LOCAL, arg);
		} 
		else if(state == 2u) {
			// Fix const type.
			
			expression();

			SET_N_GET_OPT(OP_SET_UPVALUE, arg);
		} else {
			expression();
			
			SET_N_GET(OP_SET_GLOBAL, arg);
		}
	}
	else if(canAssign && match(TOKEN_INCREMENT)) {
		if(state == 0u) {
			if(current -> locals[arg].isConst) 
				error("Attempt to increment a constant local variable!");
			
			INC_DC(OP_POST_INCREMENT, 2, arg);
		}
		else if(state == 2u) {
			INC_DC(OP_POST_INCREMENT, 4, arg);
		} else { INC_DC(OP_POST_INCREMENT, 0, arg); }    // Immutability/constant will be checked at runtime.
	}
	else if(canAssign && match(TOKEN_DECREMENT)) {
		if(state == 0u) {    // Local
			if(current -> locals[arg].isConst) 
				error("Attempt to decrement a constant local variable!");
			
			INC_DC(OP_POST_DECREMENT, 2, arg);
		} else if(state == 2u) {    // Upvalue
			INC_DC(OP_POST_DECREMENT, 4, arg);
		} else { /* Global */ INC_DC(OP_POST_DECREMENT, 0, arg); }    // Immutability/constant will be checked at runtime.
	}
	else if(canAssign && checkBinaryToken()) {
		advance();
		
		TokenType type = parser.previous.type;
		
		if(match(TOKEN_EQUAL)) {
			if(state == 0u) {
				// First get the value.
				
				SET_N_GET_OPT(OP_GET_LOCAL, arg);
				
				// Check if the local is constant. If it is, throw a compiler time error.
				
				if(current -> locals[arg].isConst) {
					error("Attempt to assign a value to a constant local!");
				}
				
				expression();
				
				binaryOpcode(type);

				SET_N_GET_OPT(OP_SET_LOCAL, arg);
			}
			else if(state == 2u) {
				SET_N_GET_OPT(OP_GET_UPVALUE, arg);
				
				expression();
				
				binaryOpcode(type);

				SET_N_GET_OPT(OP_SET_UPVALUE, arg);
			} else {
				SET_N_GET(OP_GET_GLOBAL, arg);
				
				expression();
				
				binaryOpcode(type);
				
				SET_N_GET(OP_SET_GLOBAL, arg);
			}
		} else {
			// Perform the regular get operation.
			
			if(state == 0u) {
				SET_N_GET_OPT(OP_GET_LOCAL, arg);
			} 
			else if(state == 2u) {
				SET_N_GET_OPT(OP_GET_UPVALUE, arg);
			} else {
				SET_N_GET(OP_GET_GLOBAL, arg);
			}
			
			binary(false);
		}
	}
	else {
		if(state == 0u) {
			SET_N_GET_OPT(OP_GET_LOCAL, arg);
		} 
		else if(state == 2u) {
			SET_N_GET_OPT(OP_GET_UPVALUE, arg);
		} else {
			SET_N_GET(OP_GET_GLOBAL, arg);
		}
	}
}

static void variable(bool canAssign, bool globalOnly) {
	namedVariable(&parser.previous, canAssign, globalOnly);
}

static void defaultVariable(bool canAssign) {
	variable(canAssign, false);
}

static void globalVariable(bool canAssign) {
	consume(TOKEN_IDENTIFIER, "Expected a global variable name!");
	
	variable(canAssign, true);
}

static void singleInterpolation(bool canAssign) {
	variable(canAssign, false);

	emitByte(OP_ADD);
}

static void globalInterpolation(bool canAssign) {
	variable(canAssign, true);
	
	emitByte(OP_ADD);
}

static void stringContinue(bool canAssign) {
	string(canAssign);

	emitByte(OP_ADD);
}

static void interpolation(bool canAssign) {
	expression();
	
	consume(TOKEN_RIGHT_BRACE, "Expected an end of string interpolation!");
	
	emitByte(OP_ADD);
}


static void inc(bool canAssign) {
	Chunk* chunk = currentChunk();

	checkConstantPoolOverflow("Constant pool overflow while setting variable!");
	
	TokenType type = parser.previous.type;
	
	bool globalOnly = false;
	
	if(match(TOKEN_COLON)) 
		globalOnly = true;
	
	if(!match(TOKEN_IDENTIFIER) && !match(TOKEN_THIS)) {
		error("Expected a vairable after pre increment/decrement operator!");

		return;
	}
	
	bool property = false;
	bool exp = false;
	bool isStatic = false;
	
	if(check(TOKEN_DOT) || check(TOKEN_LEFT_SQUARE) || check(TOKEN_COLON_COLON)) {
		variable(false, globalOnly);
		
		property = true;
	
		
		while(true) {
			if(match(TOKEN_DOT)) {
				consume(TOKEN_IDENTIFIER, "Expected a property name!");
				
				if(check(TOKEN_DOT) || check(TOKEN_LEFT_SQUARE) || check(TOKEN_COLON_COLON)) {
					size_t propertyArg = identifierConstant(&parser.previous);
					
					SET_N_GET(OP_GET_PROPERTY, propertyArg);
				} else break;
			}
			else if(match(TOKEN_LEFT_SQUARE)) {
				expression();
				
				consume(TOKEN_RIGHT_SQUARE, "Expected a ']' after expression!");
				
				if(check(TOKEN_DOT) || check(TOKEN_LEFT_SQUARE) || check(TOKEN_COLON_COLON)) {
					emitByte(OP_DNM_GET_PROPERTY);
				} else {
					exp = true;
					
					break;
				}
			} 
			else if(match(TOKEN_COLON_COLON)) {
				consume(TOKEN_IDENTIFIER, "Expected a property name!");
				
				if(check(TOKEN_DOT) || check(TOKEN_LEFT_SQUARE) || check(TOKEN_COLON_COLON)) {
					size_t propertyArg = identifierConstant(&parser.previous);
					
					SET_N_GET(OP_GET_PROPERTY, propertyArg);
				} else {
					isStatic = true;
					
					break;
				}
			} else break;
		}
	}
	
	Token* name = &parser.previous;
	
	OpCode code = type == TOKEN_INCREMENT ? OP_PRE_INCREMENT : OP_PRE_DECREMENT;
	
	if(!property) {
		int arg = !globalOnly ? resolveLocal(current, name) : -1;
		
		uint8_t state = 0u;
		
		if(arg == -1) {
			if(!globalOnly && (arg = resolveUpvalue(current, name)) != -1) state = 2u;
			else {
				state = 1u;
				arg = writeValueArray(&chunk -> constants, OBJECT_VAL(copyString(getVM(), parser.previous.start, parser.previous.length)));
			}
		}
		
		if(state == 0u) {
			if(current -> locals[arg].isConst) 
				error("Attempt to increment a constant local variable!");
			
			INC_DC(code, 2, arg);
		} else if(state == 2u) {
			INC_DC(code, 4, arg);
		} else { INC_DC(code, 0, arg); }    // Immutability/constant will be checked at runtime.
	} else {
		if(!exp) {
			size_t propertyArg = identifierConstant(name);

			INC_DC(code, (isStatic ? 9 : 6), propertyArg);
		} else emitBytes(code, (uint8_t) 8);
	}
}

static void _typeof(bool canAssign) {
	parsePrecedence(PREC_COMPARISON);
	
	emitByte(OP_TYPEOF);
}

static uint8_t argumentList() {
	uint32_t arg = 0u;
	
	if(!check(TOKEN_RIGHT_PAREN)) {
		do {
			expression();
			arg++;
		} while(match(TOKEN_COMMA));
	}
	
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after argument list!");
	
	if(arg > 255) 
		error("Cannot have more than 255 arguments!");
	
	return arg;
}

static void call(bool canAssign) {
	uint8_t argCount = argumentList();
	
	if(argCount <= 20) {
		emitByte(OP_CALL_0 + argCount);
	} else emitBytes(OP_CALL, argCount);
}

static void square(bool);

static void dot(bool canAssign) {
	consume(TOKEN_IDENTIFIER, "Expected a property name after '.'!");

	size_t field = writeValueArray(&currentChunk() -> constants, OBJECT_VAL(copyString(getVM(), parser.previous.start, parser.previous.length)));

	OpCode code;

	if(canAssign && match(TOKEN_EQUAL)) {
		expression();

		SET_N_GET(OP_SET_PROPERTY, field);
		emitByte(false);
	}
	else if(match(TOKEN_INCREMENT)) {
		INC_DC(OP_POST_INCREMENT, 6, field);
	}
	else if(match(TOKEN_DECREMENT)) {
		INC_DC(OP_POST_DECREMENT, 6, field);
	}
	else if(canAssign && checkBinaryToken()) {
		advance();
		
		TokenType type = parser.previous.type;
		
		if(match(TOKEN_EQUAL)) {
			SET_N_GET(OP_GET_PROPERTY_INST, field);
			
			expression();
			
			binaryOpcode(type);
			
			SET_N_GET(OP_SET_PROPERTY, field);
			emitByte(false);
		} else {
			// Perform the regular get operation.
			
			SET_N_GET(OP_GET_PROPERTY, field);
			
			binary(false);
		}
	}
	else if(match(TOKEN_LEFT_PAREN)) {
		if(match(TOKEN_CONST)) {
			consume(TOKEN_RIGHT_PAREN, "Expected a ')' after const notation!");
			consume(TOKEN_EQUAL, "Only use const notation to define a property!");
			
			expression();

			SET_N_GET(OP_SET_PROPERTY, field);
			emitByte(true);

			return;
		}

		uint8_t arg = argumentList();
		
		SET_N_GET(OP_INVOKE, field);
		emitBytes(0u, arg);
	} else {
		SET_N_GET(OP_GET_PROPERTY, field);
	}
}

static void square(bool canAssign) {
	expression();    // It's dynamic data.
	
	consume(TOKEN_RIGHT_SQUARE, "Expected a ']' after expression!");
	
	if(canAssign && match(TOKEN_EQUAL)) {
		expression();

		emitBytes(OP_DNM_SET_PROPERTY, false);
	}
	else if(match(TOKEN_INCREMENT)) 
		emitBytes(OP_POST_INCREMENT, (uint8_t) 8);
	else if(match(TOKEN_DECREMENT)) 
		emitBytes(OP_POST_DECREMENT, (uint8_t) 8);
	else if(canAssign && checkBinaryToken()) {
		advance();
		
		TokenType type = parser.previous.type;
		
		if(match(TOKEN_EQUAL)) {
			emitByte(OP_DNM_GET_PROPERTY_INST);
			
			expression();
			
			binaryOpcode(type);
			
			emitBytes(OP_DNM_SET_PROPERTY, false);
		} else {
			// Perform the regular get operation.
			
			emitByte(OP_DNM_GET_PROPERTY);
			
			binary(false);
		}
	} 
	else if(match(TOKEN_LEFT_PAREN)) {
		if(match(TOKEN_CONST)) {
			consume(TOKEN_RIGHT_PAREN, "Expected a ')' after const notation!");
			consume(TOKEN_EQUAL, "Only use const notation to define a property!");
			
			expression();

			emitBytes(OP_DNM_SET_PROPERTY, true);

			return;
		}

		emitByte(OP_DNM_INVOKE_START);
		
		uint8_t args = argumentList();
		
		emitBytes(OP_DNM_INVOKE_END, args);
	} else emitByte(OP_DNM_GET_PROPERTY);
}

static void _this(bool canAssign) {
	if(currentClass == NULL) {
		error("Cannot use 'this' outside of a class!");
		
		return;
	}
	else if(currentClass -> inStatic) {
		error("Cannot use 'this' inside of a static method!");
	}
	
	variable(false, false);
}

static void dictionary(bool canAssign) {
	emitByte(OP_DICTIONARY);
	
	size_t key;
	
	while(match(TOKEN_IDENTIFIER) || match(TOKEN_STRING)) {
		Token token = parser.previous;

		key = identifierConstant(&parser.previous);

		bool isConst = false;

		if(match(TOKEN_LEFT_PAREN) && match(TOKEN_CONST)) {
			consume(TOKEN_RIGHT_PAREN, "Expected a ')' after const notation!");

			isConst = true;
		}
 
		if(!match(TOKEN_COLON)) {
			if(token.type == TOKEN_IDENTIFIER && (match(TOKEN_COMMA) || check(TOKEN_RIGHT_BRACE))) {
				namedVariable(&token, false, false);

				SET_N_GET(OP_ADD_DICTIONARY, key);
				emitByte(isConst);

				continue;
			} else {
				error("Expected a ':' after key!");

				break;
			}
		}
		
		expression();
		
		SET_N_GET(OP_ADD_DICTIONARY, key);
		emitByte(isConst);
		
		if(!match(TOKEN_COMMA)) {
			if(!check(TOKEN_RIGHT_BRACE)) 
				error("Expected a ',' after a dictionary key-value pair!");
			
			break;
		}
	}
	
	consume(TOKEN_RIGHT_BRACE, "Expected a '}' at the end of dictionary!");
}

static void _static(bool canAssign) {
	consume(TOKEN_IDENTIFIER, "Expected a property name after '::'!");

	size_t field = writeValueArray(&currentChunk() -> constants, OBJECT_VAL(copyString(getVM(), parser.previous.start, parser.previous.length)));

	OpCode code;

	if(canAssign && match(TOKEN_EQUAL)) {
		expression();

		SET_N_GET(OP_SET_STATIC_PROPERTY, field);
		emitByte(false);
	}
	else if(match(TOKEN_INCREMENT)) {
		INC_DC(OP_POST_INCREMENT, 9, field);
	} else if(match(TOKEN_DECREMENT)) {
		INC_DC(OP_POST_DECREMENT, 9, field);
	}
	else if(canAssign && checkBinaryToken()) {
		advance();
		
		TokenType type = parser.previous.type;
		
		if(match(TOKEN_EQUAL)) {
			SET_N_GET(OP_GET_STATIC_PROPERTY_INST, field);
			
			expression();
			
			binaryOpcode(type);
			
			SET_N_GET(OP_SET_STATIC_PROPERTY, field);
			emitByte(false);
		} else {
			// Perform the regular get operation.
			
			SET_N_GET(OP_GET_STATIC_PROPERTY, field);
			
			binary(false);
		}
	}
	else if(match(TOKEN_LEFT_PAREN)) {
		if(match(TOKEN_CONST)) {
			consume(TOKEN_RIGHT_PAREN, "Expected a ')' after const notation!");
			consume(TOKEN_EQUAL, "Only use const notation to define a property!");
			
			expression();

			SET_N_GET(OP_SET_STATIC_PROPERTY, field);
			emitByte(true);

			return;
		}

		uint8_t arg = argumentList();
		
		SET_N_GET(OP_INVOKE, field);
		emitBytes(1u, arg);
	} else {
		SET_N_GET(OP_GET_STATIC_PROPERTY, field);
	}
}

static void list(bool canAssign) {
	emitByte(OP_LIST);
	
	do {
		if(check(TOKEN_RIGHT_SQUARE)) 
			break;
		
		expression();
		
		emitByte(OP_ADD_LIST);
	} while(match(TOKEN_COMMA));
	
	consume(TOKEN_RIGHT_SQUARE, "Expected a ']' at the end of list!");
}

static void recieve(bool canAssign) {
	uint8_t type = 0u;

	if(match(TOKEN_LEFT_PAREN)) {
		consume(TOKEN_IDENTIFIER, "Expected a reciever type!");

		if(parser.previous.length == 6u && !memcmp("string", parser.previous.start, 6u)) 
			type = 0u;
		else if(parser.previous.length == 3u && !memcmp("num", parser.previous.start, 3u))
			type = 1u;
		else if(parser.previous.length == 4u && !memcmp("bool", parser.previous.start, 4u)) 
			type = 2u;
		else error("Unrecognized reciever type!");

		consume(TOKEN_RIGHT_PAREN, "Expected a ')' after reciever type!");
	}

	emitBytes(OP_RECIEVE, type);
}

static void _super(bool canAssign) {
	if(currentClass == NULL) {
		error("Cannot use 'super' outside of class!");
		return;
	}
	else if(currentClass -> hasSuperClass == false) {
		error("Cannot use 'super' in a class with no superclass!");
		return;
	}
	else if(currentClass -> inStatic == true) {
		error("Cannot use 'super' in a static method!");
		return;
	}

	Token tthis = syntheticToken("this"),
	     ssuper = syntheticToken("super");

	namedVariable(&tthis, false, false);

	bool isDynamic = false;

	if(match(TOKEN_DOT)) {
		consume(TOKEN_IDENTIFIER, "Expected a superclass method name!");
	}
	else if(match(TOKEN_LEFT_SQUARE)) {
		expression();
		consume(TOKEN_RIGHT_SQUARE, "Expected a ']' after dynamic 'super' method name!");

		isDynamic = true;
	} else {
		error("Expected getting a method from 'super'!");
		return;
	}

	int ticket = !isDynamic ? identifierConstant(&parser.previous) : -1;

	if(match(TOKEN_LEFT_PAREN)) {
		uint8_t argCount = argumentList();

		namedVariable(&ssuper, false, false);

		if(isDynamic) emitBytes(OP_DNM_SUPER_INVOKE, argCount);
		else {
			SET_N_GET(OP_SUPER_INVOKE, ticket);
			emitByte(argCount);
		}
	} else {
		namedVariable(&ssuper, false, false);

		if(isDynamic) emitByte(OP_DNM_SUPER);
		else { SET_N_GET(OP_SUPER, ticket); }
	}
}

// Parse rules.

ParseRule parseRules[] = {
	[TOKEN_LEFT_PAREN]               = { grouping, call, NULL, PREC_CALL },
	[TOKEN_RIGHT_PAREN]              = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_LEFT_BRACE]               = { dictionary, NULL, NULL, PREC_NONE },
	[TOKEN_MINUS]                    = { unary, binary, NULL, PREC_TERM },
	[TOKEN_PLUS]                     = { NULL, binary, NULL, PREC_TERM },
	[TOKEN_SLASH]                    = { NULL, binary, NULL, PREC_FACTOR },
	[TOKEN_ASTERISK]                 = { NULL, binary, NULL, PREC_FACTOR },
	[TOKEN_DOT]                      = { NULL, dot, NULL, PREC_CALL },
	[TOKEN_COMMA]                    = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_SEMICOLON]                = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_COLON]                    = { globalVariable, NULL, NULL, PREC_NONE },
	[TOKEN_COLON_COLON]              = { NULL, _static, NULL, PREC_CALL },
	[TOKEN_MODULUS]                  = { NULL, binary, NULL, PREC_FACTOR },
	[TOKEN_QUESTION]                 = { NULL, NULL, NULL, PREC_NONE },
	
	[TOKEN_NOT]                      = { unary, NULL, NULL, PREC_NONE },
	[TOKEN_NOT_EQUAL]                = { NULL, binary, NULL, PREC_EQUALITY },
	[TOKEN_EQUAL]                    = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_EQUAL_EQUAL]              = { NULL, binary, NULL, PREC_EQUALITY },
	[TOKEN_GREATER]                  = { NULL, binary, NULL, PREC_COMPARISON },
	[TOKEN_GREATER_EQUAL]            = { NULL, binary, NULL, PREC_COMPARISON },
	[TOKEN_LESS]                     = { NULL, binary, NULL, PREC_COMPARISON },
	[TOKEN_LESS_EQUAL]               = { NULL, binary, NULL, PREC_COMPARISON },
	[TOKEN_OR]                       = { NULL, or, NULL, PREC_OR },
	[TOKEN_AND]                      = { NULL, and, NULL, PREC_AND },
	[TOKEN_RIGHT_SHIFT]              = { NULL, binary, NULL, PREC_TERM },
	[TOKEN_LEFT_SHIFT]               = { NULL, binary, NULL, PREC_TERM },
	[TOKEN_INCREMENT]                = { inc, NULL, NULL, PREC_NONE },
	[TOKEN_DECREMENT]                = { inc, NULL, NULL, PREC_NONE },
	
	[TOKEN_LEFT_SQUARE]              = { list, square, NULL, PREC_CALL },
	[TOKEN_RIGHT_SQUARE]             = { NULL, NULL, NULL, PREC_NONE },
	
	[TOKEN_BITWISE_AND]              = { NULL, binary, NULL, PREC_TERM },
	[TOKEN_BITWISE_OR]               = { NULL, binary, NULL, PREC_TERM },
	[TOKEN_XOR]                      = { NULL, binary, NULL, PREC_TERM },
	[TOKEN_TILDE]                    = { unary, NULL, NULL, PREC_NONE },
	[TOKEN_NAN]                      = { literal, NULL, NULL, PREC_NONE },
	[TOKEN_INFINITY]                 = { literal, NULL, NULL, PREC_NONE },
	
	[TOKEN_IDENTIFIER]               = { defaultVariable, NULL, NULL, PREC_NONE },
	[TOKEN_NUMBER]                   = { number, NULL, NULL, PREC_NONE },
	[TOKEN_STRING]                   = { string, NULL, NULL, PREC_NONE },
	[TOKEN_STRING_CONTINUE]          = { stringContinue, stringContinue, NULL, PREC_ASSIGNMENT },
	[TOKEN_INTERPOLATION_IDENTIFIER] = { NULL, singleInterpolation, NULL, PREC_ASSIGNMENT },
	[TOKEN_GLOBAL_INTERPOLATION]     = { NULL, globalInterpolation, NULL, PREC_ASSIGNMENT },
	[TOKEN_INTERPOLATION_START]      = { NULL, interpolation, NULL, PREC_ASSIGNMENT },

	[TOKEN_TYPEOF]                   = { _typeof, NULL, NULL, PREC_NONE },
	[TOKEN_CONST]                    = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_CONTINUE]                 = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_BREAK]                    = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_INSTOF]                   = { NULL, binary, NULL, PREC_TERM },
	
	[TOKEN_IF]                       = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_ELSE]                     = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_TRUE]                     = { literal, NULL, NULL, PREC_NONE },
	[TOKEN_FALSE]                    = { literal, NULL, NULL, PREC_NONE },
	[TOKEN_CLASS]                    = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_THIS]                     = { _this, NULL, NULL, PREC_NONE },
	[TOKEN_NULL]                     = { literal, NULL, NULL, PREC_NONE },
	[TOKEN_FUNCTION]                 = { fnExpr, NULL, NULL, PREC_NONE },
	[TOKEN_SHOW]                     = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_RECIEVE]                  = { recieve, NULL, NULL, PREC_NONE },
	[TOKEN_FOR]                      = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_WHILE]                    = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_SUPER]                    = { _super, NULL, NULL, PREC_NONE },
	[TOKEN_TAKE]                     = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_RETURN]                   = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_SHOWL]                    = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_ERROR]                    = { NULL, NULL, NULL, PREC_NONE },
	[TOKEN_EOF]                      = { NULL, NULL, NULL, PREC_NONE },
};

static ParseRule* getRule(const TokenType tokenType) {
	return &parseRules[tokenType];
}

static void binary(bool canAssign) {
	TokenType operatorType = parser.previous.type;
	
	ParseRule* rule = getRule(operatorType);
	
	parsePrecedence((Precedence) rule -> precedence + 1u);
	
	binaryOpcode(operatorType);
}

static void expressionStatement(bool inLoop) {
	expression();

	semicolon("Expected a nice ';' at the end of expression!", true);

	if(parser.current.type == TOKEN_EOF && !inLoop) 
		emitByte(OP_POP);
	else emitByte(OP_SILENT_POP);
}

static void block(bool inLoop) {
	while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration(inLoop);
	}

	consume(TOKEN_RIGHT_BRACE, "Expeced a '}' at end of block!");
}

static uint32_t emitJump(uint8_t jumpInstruction) {
	emitByte(jumpInstruction);
	
	emitByte(0xFF);
	emitBytes(0xFF, 0xFF);
	
	return currentChunk() -> count - 3u;
}

static void patchJump(uint32_t stride) {
	uint32_t jump = currentChunk() -> count - stride - 3u;
	
	if(jump > UINT24_MAX) 
		error("Too many intructions to jump over!");
	
	currentChunk() -> code[stride]      = (jump >> 16) & 0xFF;
	currentChunk() -> code[stride + 1u] = (jump >> 8) & 0XFF;
	currentChunk() -> code[stride + 2u] = jump & 0xFF;
}

static void patchRequestJump(uint32_t distance, uint32_t localStride) {
	uint32_t jump = currentChunk() -> count - distance - 3u - localStride;
	
	if(jump > UINT24_MAX) 
		error("Too many intructions to jump over!");
	
	currentChunk() -> code[distance]      = (jump >> 16) & 0xFF;
	currentChunk() -> code[distance + 1u] = (jump >> 8) & 0XFF;
	currentChunk() -> code[distance + 2u] = jump & 0xFF;
}	

static void startLoop() {
	current -> loopDepth++;
}

static void endLoop() {
	current -> loopDepth--;
}

static void patchRequests() {
	if(!current -> requests.count) 
		return;

	while(current -> requests.count) {
		PointData pointData = current -> requests.data[--current -> requests.count];

		if(pointData.depth == current -> loopDepth) 
			patchRequestJump(pointData.codePoint, pointData.stride);
		else break;
	}
}

static void beginScope() {
	current -> scopeDepth++;
}

static void endScope() {
	current -> scopeDepth--;
	
	while(current -> localCount > 0 && current -> locals[current -> localCount - 1].depth > current -> scopeDepth) {
		if(current -> locals[current -> localCount - 1u].isCaptured) 
			emitByte(OP_CLOSE_UPVALUE);
		else emitByte(OP_SILENT_POP);
		
		current -> localCount--;
	}
}

static void ternary() {
	uint32_t thenJump = emitJump(OP_JUMP_IF_FALSE);
	
	expression();
	
	uint32_t elseJump = emitJump(OP_JUMP);
	
	consume(TOKEN_COLON, "Expected ':' after ternary truth condition!");
	
	patchJump(thenJump);
	
	expression();
	
	patchJump(elseJump);
}

static void ifStatement(bool inLoop) {
	consume(TOKEN_LEFT_PAREN, "Expected '(' after 'if'!");
	
	expression();
	
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after condition!");
	
	uint32_t thenJump = emitJump(OP_JUMP_IF_FALSE);
	
	statement(inLoop);
	
	uint32_t elseJump = -1;
	
	if(match(TOKEN_ELSE)) {
		elseJump = emitJump(OP_JUMP);
		patchJump(thenJump);
		statement(inLoop);
	} else patchJump(thenJump);
	
	if(elseJump != -1) 
		patchJump(elseJump);
}

static void emitLoop(uint32_t loopStart) {
	emitByte(OP_LOOP);

	int offset = currentChunk() -> count - loopStart + 3u;
	
	if (offset > UINT24_MAX) error("Too large loop body!");

	emitByte((offset >> 16) & 0xFF);
	emitByte((offset >> 8) & 0xFF);
	emitByte(offset & 0xFF);
}

static void whileStatement() {
	startLoop();

	uint32_t loopStart = currentChunk() -> count;
	
	consume(TOKEN_LEFT_PAREN, "Expected '(' after 'if'!");
	
	expression();
	
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after condition!");
	
	uint32_t exitJump = emitJump(OP_JUMP_IF_FALSE);
	
	statement(true);
	
	patchRequests();
	
	emitLoop(loopStart);
	
	patchJump(exitJump);

	endLoop();
}

static void doStatement() {
	startLoop();
	
	uint32_t loopStart = currentChunk() -> count;
	
	statement(true);
	
	consume(TOKEN_WHILE, "Expected 'while' after 'do' statement!");
	
	consume(TOKEN_LEFT_PAREN, "Expected a '(' after 'while'!");
	
	expression();
	
	consume(TOKEN_RIGHT_PAREN, "Expected a ')' at end of the loop!");
	
	uint32_t thenJump = emitJump(OP_JUMP_IF_FALSE);
	
	patchRequests();
	
	emitLoop(loopStart);
	
	patchJump(thenJump);
	
	endLoop();
}

static void breakStatement(bool inLoop) {
	semicolon("Expected ';' after break statement!", true);
	
	if(!inLoop) 
		error("Attempt to call break outside of loop or switch!");

	pushRequest(emitJump(OP_BREAK_JUMP));
}

static void continueStatement(bool inLoop) {
	semicolon("Expected ';' after continue statement!", true);
	
	if(!inLoop) 
		error("Attempt to call continue outside of loop or switch!");
	
	pushRequest(emitJump(OP_JUMP));
}

static void switchStatement() {
	consume(TOKEN_LEFT_PAREN, "Expected a '(' after keyword 'switch'!");

	expression();

	consume(TOKEN_RIGHT_PAREN, "Expected a ')' after switch statements expression!");
	consume(TOKEN_LEFT_BRACE, "Expected a '{' at the beginning of switch block!");

	bool foundDefault = false;
	int previousJumpSkip = -1;
	int fallthroughJump = -1;

	int caseEnds[MAX_CASES];
	int caseCount = 0;

	while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		if(match(TOKEN_CASE)) {
			if(foundDefault) 
				error("Attempt to add new case after default case in switch!");

			if(previousJumpSkip != -1) {
				patchJump(previousJumpSkip);
				previousJumpSkip = -1;
			}

			emitByte(OP_DUP);

			expression();

			emitByte(OP_EQUAL);

			previousJumpSkip = emitJump(OP_JUMP_IF_FALSE);
			
			if(fallthroughJump != -1) {
				patchJump(fallthroughJump);
				fallthroughJump = -1;
			}

			consume(TOKEN_COLON, "Expected a ':' after case expression!");

			if(check(TOKEN_LEFT_BRACE)) 
				statement(true);
			else {
				beginScope();

				while(!check(TOKEN_CASE) && !check(TOKEN_DEFAULT) && !check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) 
					declaration(true);

				endScope();
			}

			patchRequests();

			if(caseCount > 512) 
				error("Maximum case number exceeded in switch!");
			else caseEnds[caseCount++] = emitJump(OP_SWITCH_JUMP);
			
			fallthroughJump = emitJump(OP_JUMP);
		}
		else if(match(TOKEN_DEFAULT)) {
			if(foundDefault) 
				error("Attempt to add multiple default statements in switch!");
			else foundDefault = true;

			if(previousJumpSkip != -1) {
				patchJump(previousJumpSkip);
				previousJumpSkip = -1;
			}
			
			if(fallthroughJump != -1) {
				patchJump(fallthroughJump);
				fallthroughJump = -1;
			}

			consume(TOKEN_COLON, "Expected a ':' after default!");

			if(check(TOKEN_LEFT_BRACE)) 
				statement(true);
			else {
				beginScope();

				while(!check(TOKEN_CASE) && !check(TOKEN_DEFAULT) && !check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) 
					declaration(true);

				endScope();
			}

			patchRequests();
		} else {
			advance();
			error("Unexpected statement inside switch!");
		}
	}

	consume(TOKEN_RIGHT_BRACE, "Expected a '}' at the end of switch statement!");
	
	if(previousJumpSkip != -1) 
		patchJump(previousJumpSkip);
	
	if(fallthroughJump != -1) 
		patchJump(fallthroughJump);

	for(int i = 0; i < caseCount; i++) 
		patchJump(caseEnds[i]);

	emitByte(OP_SILENT_POP);
}

static void showStatement(bool nl) {
	bool loopShow = false;

	if(match(TOKEN_LEFT_PAREN)) {
		emitConstant(NUMBER_VAL(0));
		
		grouping(false);

		loopShow = true;
	}

	int loopStart = -1,
	    jump      = -1;

	if(check(TOKEN_SEMICOLON)) {
		if(!loopShow) {
			if(nl) {
				emitConstant(OBJECT_VAL(takeString(getVM(), "", 1u, false)));
			} else error("Expected an expression after 'show'!");
		}
		
		loopShow = false;
	} else {
		// Very simple local variable management, without identifier name ofcourse.
		
		if(loopShow) {
			loopStart = currentChunk() -> count;

			emitByte(OP_DUP);

			SET_N_GET(OP_GET_LOCAL, current -> localCount);
			
			emitByte(OP_GREATER);

			jump = emitJump(OP_JUMP_IF_FALSE);
		}

		expression();

		semicolon("Expected a ';' at end of show/showl statement!", true);
	}

	emitByte(nl ? OP_SHOWL : (loopShow ? OP_RAW_SHOW : OP_SHOW));

	if(loopShow) {
		INC_DC(OP_POST_INCREMENT, 2, current -> localCount);
		
		emitByte(OP_SILENT_POP);
		
		emitLoop(loopStart);

		patchJump(jump);

		emitBytes(OP_SILENT_POP, OP_SILENT_POP);
	}
}

static void returnStatement() {
	if(current -> type == TYPE_PROGRAM) 
		error("Cannot return from top level program!");
	
	if (match(TOKEN_SEMICOLON)) 
		emitReturn();
	else {
		if(current -> type == TYPE_INITIALIZER) 
			error("Cannot return value from initializer!");
		
		expression();
		
		semicolon("Expected a nice ';' after return value!", true);
		
		emitByte(OP_RETURN);
	}
}

static void forStatement() {
	// To bound the variable declaration to local.
	beginScope();
	
	startLoop();
	
	consume(TOKEN_LEFT_PAREN, "Expected a '(' after keyword 'for'!");
	
	if(match(TOKEN_SEMICOLON)) ; // Do nothing.
	else if(match(TOKEN_TAKE)) 
		takeDeclaration(true);
	else expressionStatement(true);
	
	uint32_t loopStart = currentChunk() -> count;
	
	uint32_t exitJump = -1;
	
	if(!match(TOKEN_SEMICOLON)) {
		expression();
		
		consume(TOKEN_SEMICOLON, "Expected a ';' after for loop's condition expression!");
		
		exitJump = emitJump(OP_JUMP_IF_FALSE);
	}
	
	if(!match(TOKEN_RIGHT_PAREN)) {
		uint32_t bodyJump = emitJump(OP_JUMP);
		
		uint32_t increamentStart = currentChunk() -> count;
		
		expression();
		
		emitByte(OP_SILENT_POP);
		
		consume(TOKEN_RIGHT_PAREN, "Expected a ')' at end of the loop!");
		
		emitLoop(loopStart);

		loopStart = increamentStart;

		patchJump(bodyJump);
	}
	
	beginScope();
	
	if(match(TOKEN_SHOW)) 
		showStatement(false);
	else if(match(TOKEN_SHOWL)) 
		showStatement(true);
	else if(match(TOKEN_IF)) 
		ifStatement(true);
	else if(match(TOKEN_WHILE)) 
		whileStatement();
	else if(match(TOKEN_FOR)) 
		forStatement();
	else if(match(TOKEN_DO)) 
		doStatement();
	else if(match(TOKEN_BREAK)) 
		breakStatement(true);
	else if(match(TOKEN_CONTINUE)) 
		continueStatement(true);
	else if(match(TOKEN_SWITCH)) 
		switchStatement();
	else if(match(TOKEN_RETURN)) 
		returnStatement();
	else if(match(TOKEN_LEFT_BRACE)) 
		block(true);
	else expressionStatement(true);
	
	endScope();
	
	patchRequests();
	
	emitLoop(loopStart);
	
	if(exitJump != -1) {
		patchJump(exitJump);
	}
	
	endLoop();
	endScope();
}

static void delStatement() {
	bool globalOnly = false;
	
	if(match(TOKEN_COLON)) 
		globalOnly = true;
	
	consume(TOKEN_IDENTIFIER, "Expected an identifer of dictionary or instance type!");
	
	if(!check(TOKEN_DOT) && !check(TOKEN_LEFT_SQUARE) && !check(TOKEN_COLON_COLON)) {
		errorAtCurrent("Expected a property name or expression after identifier!");
		
		return;
	}
	
	variable(false, globalOnly);
	
	bool exp = false;
	bool isStatic = false;
	
	while(true) {
		if(match(TOKEN_DOT)) {
			consume(TOKEN_IDENTIFIER, "Expected a property name!");
			
			if(check(TOKEN_DOT) || check(TOKEN_LEFT_SQUARE) || check(TOKEN_COLON_COLON)) {
				size_t propertyArg = identifierConstant(&parser.previous);
				
				SET_N_GET(OP_GET_PROPERTY, propertyArg);
			} else break;
		}
		else if(match(TOKEN_LEFT_SQUARE)) {
			expression();
			
			consume(TOKEN_RIGHT_SQUARE, "Expected a ']' after expression!");
			
			if(check(TOKEN_DOT) || check(TOKEN_LEFT_SQUARE) || check(TOKEN_COLON_COLON)) {
				emitByte(OP_DNM_GET_PROPERTY);
			} else {
				exp = true;
				
				break;
			}
		}
		else if(match(TOKEN_COLON_COLON)) {
			consume(TOKEN_IDENTIFIER, "Expected a property name!");
			
			if(check(TOKEN_DOT) || check(TOKEN_LEFT_SQUARE) || check(TOKEN_COLON_COLON)) {
				size_t propertyArg = identifierConstant(&parser.previous);
				
				SET_N_GET(OP_GET_STATIC_PROPERTY, propertyArg);
			} else {
				isStatic = true;
				
				break;
			}
		} else break;
	}
	
	if(!exp) {
		size_t field = identifierConstant(&parser.previous);
		
		SET_N_GET(OP_DELETE_PROPERTY, field);
		emitByte(isStatic ? 1u : 0u);
	} else emitByte(OP_DNM_DELETE_PROPERTY);
	
	semicolon("Expected a ';' after del statement!", true);
}

static void statement(bool inLoop) {
	if(match(TOKEN_SHOW)) 
		showStatement(false);
	else if(match(TOKEN_SHOWL)) 
		showStatement(true);
	else if(match(TOKEN_IF)) 
		ifStatement(inLoop);
	else if(match(TOKEN_WHILE)) 
		whileStatement();
	else if(match(TOKEN_FOR)) 
		forStatement();
	else if(match(TOKEN_DO)) 
		doStatement();
	else if(match(TOKEN_SEMICOLON)) { /* Do nothing. */ }
	else if(match(TOKEN_BREAK)) 
		breakStatement(inLoop);
	else if(match(TOKEN_CONTINUE)) 
		continueStatement(inLoop);
	else if(match(TOKEN_SWITCH)) 
		switchStatement();
	else if(match(TOKEN_RETURN)) 
		returnStatement();
	else if(match(TOKEN_DEL)) 
		delStatement();
	else if(match(TOKEN_LEFT_BRACE)) {
		beginScope();

		block(inLoop);

		endScope();
	} else expressionStatement(inLoop);    // It's an expression acting as a statement or the source code is starting with an expression.
}

static void synchronize() {
	parser.panicMode = false;

	while(parser.current.type != TOKEN_EOF) {
		if(parser.current.type == TOKEN_SEMICOLON) 
			return;

		switch(parser.previous.type) {
			case TOKEN_CLASS:
			case TOKEN_SHOWL:
			case TOKEN_SHOW: 
			case TOKEN_TAKE: 
			case TOKEN_FOR: 
			case TOKEN_WHILE: 
			case TOKEN_IF: 
			case TOKEN_RETURN: 
			case TOKEN_FUNCTION: 
				return;

			default: 
				;    // Do nothing.
		}

		advance();
	}
}

static void markInitialized() {
	if(current -> scopeDepth == 0) 
		return;
	
	current -> locals[current -> localCount - 1u].depth = current -> scopeDepth;
}

static void defineVariable(size_t var, bool isConst) {
	if(current -> scopeDepth > 0) {
		markInitialized();
		
		return;
	}
	
	DEFINE(OP_DEFINE_GLOBAL, var, isConst);
}

#define VARIABLE(cnst, inLoop) \
	while(!check(TOKEN_SEMICOLON) && !check(TOKEN_EOF) && !parser.hadError) {\
		size_t var = parseVariable("Expected a variable name!", cnst);\
		if(match(TOKEN_EQUAL)) \
			expression();\
		else emitByte(OP_NULL);\
		defineVariable(var, cnst);\
		if(!check(TOKEN_SEMICOLON)) \
			consume(TOKEN_COMMA, "Expected a ';' or ',' after variable declaration!");\
	}\
	semicolon("Expected a ';' after variable declaration!", !inLoop);\

static void takeDeclaration(bool inLoop) {		// In loop specially for 'for'.
	VARIABLE(false, inLoop);
}

static void and(bool canAssign) {
	uint32_t endJump = emitJump(OP_JUMP_OPR);
	
	emitByte(OP_SILENT_POP);
	
	parsePrecedence(PREC_AND);
	
	patchJump(endJump);
}

static void or(bool canAssign) {
	uint32_t elseJump = emitJump(OP_JUMP_OPR);
	uint32_t endJump  = emitJump(OP_JUMP);

	patchJump(elseJump);
	emitByte(OP_SILENT_POP);

	parsePrecedence(PREC_OR);
	patchJump(endJump);
}

static void function(FunctionType type, bool inExpr) {
	Compiler compiler;
	
	initCompiler(&compiler, type, inExpr);
	
	beginScope();
	
	consume(TOKEN_LEFT_PAREN, "Expected a '(' after function name!");
	
	if(!check(TOKEN_RIGHT_PAREN)) {
		do {
			current -> function -> arity++;
			
			if(current -> function -> arity > 255) 
				errorAtCurrent("Cannot have more than 255 parameters!");
			
			bool isConst = false;
			
			if(match(TOKEN_CONST)) 
				isConst = true;
			
			size_t param = parseVariable("Expected a parameter name!", isConst);
			
			defineVariable(param, isConst);
		} while(match(TOKEN_COMMA));
	}
	
	consume(TOKEN_RIGHT_PAREN, "Expected a ')' after function parameters!");
	consume(TOKEN_LEFT_BRACE, "Expected a '{' at the start of function body!");
	
	block(false);
	
	ObjFunction* function = endCompiler();
	
	checkConstantPoolOverflow("Constant pool overflow while creating closure!");
	
	size_t ticket = writeValueArray(&currentChunk() -> constants, OBJECT_VAL(function));
	
	if(function -> upvalueCount > 0u) {
		if(ticket > 255u) {
			emitByte(OP_CLOSURE_LONG);

			NEEDLE_BYTES(ticket);
		} else emitBytes(OP_CLOSURE, (uint8_t) ticket);

		for(int i = 0; i < compiler.function -> upvalueCount; i++) {
			emitBytes(compiler.upvalues[i].isLocal ? 1u : 0u, compiler.upvalues[i].isConst ? 1u : 0u);
			
			NEEDLE_BYTES(compiler.upvalues[i].index);
		}
	} else {
		// If there is no upvalue, there is no need to linger with closures. Emit it
		// like a normal function.
		
		if(ticket > 255u) {
			emitByte(OP_CONSTANT_LONG);
			
			NEEDLE_BYTES(ticket);
		} else emitBytes(OP_CONSTANT, (uint8_t) ticket);
	}
}

static void functionDeclaration(bool isConst) {
	size_t global = parseVariable("Expected a function name!", isConst);
	
	markInitialized();
	
	function(TYPE_FUNCTION, false);
	
	defineVariable(global, isConst);\
}

static void fnExpr(bool canAssign) {
	function(TYPE_FUNCTION, true);
}

static void method(bool isConst, bool isStatic) {
	consume(TOKEN_IDENTIFIER, "Expected an identifier!");
	
	size_t constant = identifierConstant(&parser.previous);
	
	if(match(TOKEN_EQUAL)) {
		if(!isStatic) {
			error("Fields only can be static in classes!");
		} else {
			expression();
			
			SET_N_GET(OP_SET_STATIC, constant);
			
			emitByte(isConst ? 1u : 0u);
		}
		
		if(check(TOKEN_SEMICOLON)) 
			semicolon("Expected a ';' at the end of static field!", true);
		
		return;
	}
	
	FunctionType type = TYPE_METHOD;
	
	if(parser.previous.length == 4 && !memcmp(parser.previous.start, "init", 4u)) {
		if(isStatic) {
			error("Initilizers cannot be static!");
			
			return;
		}
		
		type = TYPE_INITIALIZER;
	}
	
	currentClass -> inStatic = isStatic;
	
	function(type, false);
	
	if(isStatic) 
		currentClass -> inStatic = false;
	
	SET_N_GET(OP_METHOD, constant);
	
	emitByte(isStatic ? 1u : 0u);
	
	emitByte((uint8_t) isConst ? 1u : 0u);
}

static void classDeclaration(bool inLoop, bool isConst) {
	consume(TOKEN_IDENTIFIER, "Expected a class name!");
	
	Token className = parser.previous;

	size_t constant = writeValueArray(&currentChunk() -> constants, OBJECT_VAL(copyString(getVM(), parser.previous.start, parser.previous.length)));

	declareVariable(isConst);

	SET_N_GET(OP_CLASS, constant);

	defineVariable(constant, isConst);
	
	ClassCompiler classCompiler;
	
	classCompiler.hasSuperClass = false;
	classCompiler.enclosing     = currentClass;
	
	currentClass = &classCompiler;

	if(match(TOKEN_IS)) {
		consume(TOKEN_IDENTIFIER, "Expected a superclass name!");

		if(identifiersEqual(&className, &parser.previous)) {
			error("Cannot inherit own class!");
		}
		else if((parser.previous.length == 4u && !memcmp(parser.previous.start, "List", parser.previous.length)) ||
		        (parser.previous.length == 6u && !memcmp(parser.previous.start, "String", parser.previous.length)) ||
		        (parser.previous.length == 10u && !memcmp(parser.previous.start, "Dictionary", parser.previous.length)) ||
		        (parser.previous.length == 8u && !memcmp(parser.previous.start, "Function", parser.previous.length)) ||
		        (parser.previous.length == 6u && !memcmp(parser.previous.start, "Number", parser.previous.length))) {
			error("Inheriting wrapper classes are forbidden!");
		}

		variable(false, false);

		beginScope();

		Token super = syntheticToken("super");
		addLocal(&super, true);
		defineVariable(0, true);

		namedVariable(&className, false, false);
		emitByte(OP_INHERIT);

		classCompiler.hasSuperClass = true;
	}
	
	namedVariable(&className, false, false);

	consume(TOKEN_LEFT_BRACE, "Expected a nice '{' at the beginning of class body!");
	
	while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		bool isConst   = false;
		bool isStatic  = false;
		
		if(match(TOKEN_CONST)) 
			isConst = true;
		
		if(match(TOKEN_STATIC)) 
			isStatic = true;
		
		method(isConst, isStatic);

		// Consume all the semicolon(s) if provided.

		while(match(TOKEN_SEMICOLON));
	}
	
	consume(TOKEN_RIGHT_BRACE, "Expected a nice '}' at the end of class body!");
	
	emitByte(check(TOKEN_EOF) && !inLoop ? OP_POP : OP_SILENT_POP);

	if(classCompiler.hasSuperClass == true) 
		endScope();
	
	currentClass = currentClass -> enclosing;
}

static void constDeclaration(bool inLoop) {
	if(match(TOKEN_FUNCTION)) {
		functionDeclaration(true);
		
		return;
	}
	else if(match(TOKEN_CLASS)) {
		classDeclaration(inLoop, true);
		
		return;
	}
	
	VARIABLE(true, false);
}

#undef VARIABLE

static void declaration(bool inLoop) {
	if(match(TOKEN_TAKE)) 
		takeDeclaration(false);
	else if(match(TOKEN_CONST)) 
		constDeclaration(inLoop);
	else if(match(TOKEN_FUNCTION)) 
		functionDeclaration(false);
	else if(match(TOKEN_CLASS)) 
		classDeclaration(inLoop, false);
	else statement(inLoop);

	// if(parser.panicMode) synchronize();
}

ObjFunction* compile(VM* vm, const char* source) {
	Scanner scanner;

	initScanner(&scanner, source);

	parser.hadError = parser.panicMode = false;
	
	globalScanner = &scanner;
	globalVM      = vm;

	Compiler compiler;

	initCompiler(&compiler, TYPE_PROGRAM, false);

	advance();

	__compile: 
	while(!match(TOKEN_EOF)) {
		declaration(false);
	}

	ObjFunction* function = endCompiler();
	
	freeScanner(&scanner);
	
	if(parser.hadError && vm -> bytesAllocated > vm -> nextGC) 
		garbageCollector();

	return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
	Compiler* compiler = current;

	while(compiler != NULL) {
		markObject((Obj*) compiler -> function);

		compiler = compiler -> enclosing;
	}
}
