#ifndef _promit_scanner_
#define _promit_scanner_

#pragma once

typedef enum {
	TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,	// Parentheses.
	TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,	// Curly braces.
	TOKEN_MINUS, TOKEN_PLUS, TOKEN_SLASH, 
	TOKEN_INCREMENT, TOKEN_DECREMENT,
	TOKEN_DOT, TOKEN_COMMA, TOKEN_SEMICOLON,
	TOKEN_COLON, TOKEN_MODULUS, TOKEN_QUESTION,

	TOKEN_NOT, TOKEN_NOT_EQUAL, TOKEN_ASTERISK,
	TOKEN_GREATER, TOKEN_GREATER_EQUAL,
	TOKEN_EQUAL, TOKEN_EQUAL_EQUAL, TOKEN_IS,
	TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_STATIC,
	TOKEN_OR, TOKEN_AND, TOKEN_COLON_COLON,
	TOKEN_RIGHT_SHIFT, TOKEN_LEFT_SHIFT, TOKEN_INSTOF,

	TOKEN_BITWISE_AND, TOKEN_BITWISE_OR, TOKEN_DEFAULT,
	TOKEN_XOR, TOKEN_TILDE, TOKEN_NAN, TOKEN_SWITCH,

	TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_STRING, TOKEN_INFINITY,
	TOKEN_STRING_CONTINUE, TOKEN_INTERPOLATION_IDENTIFIER, TOKEN_CASE,
	TOKEN_INTERPOLATION_START, TOKEN_CONTINUE, TOKEN_RIGHT_SQUARE,
	TOKEN_GLOBAL_INTERPOLATION, TOKEN_DEL, TOKEN_LEFT_SQUARE, 

	TOKEN_IF, TOKEN_ELSE, TOKEN_TRUE, TOKEN_FALSE,
	TOKEN_CLASS, TOKEN_THIS, TOKEN_NULL, TOKEN_FUNCTION,
   	TOKEN_SHOW, TOKEN_RECEIVE, TOKEN_FOR, TOKEN_WHILE, 
	TOKEN_SUPER, TOKEN_TAKE, TOKEN_RETURN, TOKEN_SHOWL,
	TOKEN_CONST, TOKEN_TYPEOF, TOKEN_DO, TOKEN_BREAK, 

	TOKEN_ERROR, TOKEN_EOF
} TokenType;

typedef struct {
	TokenType type;
	const char* start;
	int length;
	int line;
} Token;

typedef struct {
	int head;
	int tail;
	int capacity;
	int quantity;
	Token* data;
} Queue;

typedef struct {
	const char* start;
	const char* current;
	const char* buffer;				// The actual unchanged buffer (crucial for compiler).
	int line;
	bool singleInterpolation;
	bool globalInterpolation;
	int interpolationDepth;
	Queue queue;
} Scanner;

void initScanner(Scanner*, const char*);
Token scanToken(Scanner*);
void freeScanner(Scanner*);

#endif
