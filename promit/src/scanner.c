#include <string.h>

#include "common.h"
#include "memory.h"
#include "scanner.h"

void initScanner(Scanner* scanner, const char* source) {
	scanner -> start               = scanner -> current = source;
	scanner -> buffer              = source;
	scanner -> line                = 1;
	scanner -> stringFragmented    = false;
	scanner -> singleInterpolation = false;
	scanner -> globalInterpolation = false;
	scanner -> interpolationDepth  = 0;

	// Queue.
	
	scanner -> queue.head     = 0;
	scanner -> queue.tail     = 0;
	scanner -> queue.capacity = 64;
	scanner -> queue.quantity = 0;
	scanner -> queue.data     = GROW_ARRAY(Token, NULL, 0u, scanner -> queue.capacity);
}

void freeScanner(Scanner* scanner) {
	FREE_ARRAY(Token, scanner -> queue.data, scanner -> queue.capacity);
}

static void enqueue(Scanner* scanner, Token token) {
	if(scanner -> queue.quantity >= scanner -> queue.capacity) {
		int oldCapacity = scanner -> queue.capacity;

		scanner -> queue.capacity *= 2u;

		scanner -> queue.data = GROW_ARRAY(Token, scanner -> queue.data, oldCapacity, scanner -> queue.capacity);

		if(scanner -> queue.head > scanner -> queue.tail) {
			for(int i = scanner -> queue.head; i < oldCapacity; i++) 
				scanner -> queue.data[i + oldCapacity] = scanner -> queue.data[i];
		}

		scanner -> queue.head += oldCapacity;
	}
	
	scanner -> queue.data[scanner -> queue.tail] = token;
	scanner -> queue.tail = (scanner -> queue.tail + 1) % scanner -> queue.capacity;
	scanner -> queue.quantity++;
}

static Token dequeue(Scanner* scanner) {
	Token token;

	if(scanner -> queue.quantity > 0) {
		token = scanner -> queue.data[scanner -> queue.head];

		scanner -> queue.head = (scanner -> queue.head + 1) % scanner -> queue.capacity;
		scanner -> queue.quantity--;
	}

	return token;
}

static bool queueEmpty(Scanner* scanner) {
	return scanner -> queue.quantity == 0;
}

static bool isAtEnd(Scanner* scanner) {
	return *scanner -> current == 0;
}

static Token makeToken(Scanner* scanner, TokenType tokenType) {
	Token token;

	token.type   = tokenType;
	token.start  = scanner -> start;
	token.length = (int) (scanner -> current - scanner -> start);
	token.line   = scanner -> line;

	return token;
}

static Token errorToken(Scanner* scanner, const char* msg) {
	Token token;

	token.type   = TOKEN_ERROR;
	token.start  = msg;
	token.length = strlen(msg);
	token.line   = scanner -> line;

	return token;
}

static char advance(Scanner* scanner) {
	return *scanner -> current++;
}

static bool match(Scanner* scanner, char expected) {
	if(isAtEnd(scanner)) return false;

	if(*scanner -> current != expected) return false;

	scanner -> current++;

	return true;
}

static char peek(Scanner* scanner) {
	if(isAtEnd(scanner)) return 0;

	return *scanner -> current;
}

static char peekNext(Scanner* scanner) {
	if(isAtEnd(scanner)) return 0;
	
	return scanner -> current[1];
}

static void skipWhitespace(Scanner* scanner) {
	char c;

	while(true) {
		c = peek(scanner);

		switch(c) {
			case ' ':
			case '\r':
			case '\t':
				advance(scanner);
				break;
			case '\n': {
				scanner -> line++;
				advance(scanner);
				break;
			}
			case '#': {
				while(!isAtEnd(scanner) && advance(scanner) != '\n');
				scanner -> line++;
				break;
			}
			case '/': 
				if(peekNext(scanner) == '/') {
					while(!isAtEnd(scanner) && advance(scanner) != '\n');
					scanner -> line++;
				}
				else if(peekNext(scanner) == '*') {
					while(!isAtEnd(scanner)) {
						if(advance(scanner) == '*' && peek(scanner) == '/') {
							advance(scanner);
							
							break;
						}
						else if(peek(scanner) == '\n') 
							scanner -> line++;
					}
				} else return;
				break;
			default: 
				return;
		}
	}
}

static bool isDigit(const char c) {
	return c >= '0' && c <= '9';
}

static bool isTerm(const char c) {
	return c == '+' || c == '-';
}

static void number(Scanner* scanner) {
	while(isDigit(peek(scanner))) advance(scanner);

	if(peek(scanner) == '.') {
		advance(scanner);
		
		if(isDigit(peek(scanner))) {
			while(isDigit(peek(scanner))) advance(scanner);
		}
	}
	
	if((peek(scanner) == 'e' || peek(scanner) == 'E') && (isDigit(peekNext(scanner)) || isTerm(peekNext(scanner)))) {
		advance(scanner);
		
		while(isDigit(peek(scanner)) || isTerm(peek(scanner))) advance(scanner);
	}

	 enqueue(scanner, makeToken(scanner, TOKEN_NUMBER));
}

static bool isAlpha(const char c) {
	return (c >= 'A' && c <= 'Z') || 
		   (c >= 'a' && c <= 'z') ||
		   c == '$' || c == '_';
}

static TokenType checkKeyword(Scanner* scanner, int start, const char* rest, size_t length, TokenType expected) {
	if((scanner -> current - scanner -> start == start + length) && !memcmp(scanner -> start + start, rest, length)) 
		return expected;

	return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Scanner* scanner) {
	switch(*scanner -> start) {
		case 'b': return checkKeyword(scanner, 1, "reak", 4, TOKEN_BREAK);
		case 'c': if(scanner -> current - scanner -> start > 1) {
			switch(scanner -> start[1]) {
				case 'a': return checkKeyword(scanner, 2, "se", 2, TOKEN_CASE);
				case 'l': return checkKeyword(scanner, 2, "ass", 3, TOKEN_CLASS);
				case 'o': if(scanner -> current - scanner -> start - 1 > 1) {
					TokenType ttype = checkKeyword(scanner, 2, "ntinue", 6, TOKEN_CONTINUE);
					
					if(ttype != TOKEN_CONTINUE) 
						ttype = checkKeyword(scanner, 2, "nst", 3, TOKEN_CONST);
					
					return ttype;
				}
			}
		}
		case 'd': if(scanner -> current - scanner -> start > 1) {
			switch(scanner -> start[1]) {
				case 'e': {
					TokenType ttype = checkKeyword(scanner, 2, "fault", 5, TOKEN_DEFAULT);
					
					if(ttype != TOKEN_DEFAULT) 
						ttype = checkKeyword(scanner, 2, "l", 1, TOKEN_DEL);
							
					return ttype;
				}
				case 'o': return checkKeyword(scanner, 2, "", 0, TOKEN_DO);
			}
		}
		case 'e': return checkKeyword(scanner, 1, "lse", 3, TOKEN_ELSE);
		case 'f': if(scanner -> current - scanner -> start > 1) {
			switch(scanner -> start[1]) {
				case 'o': return checkKeyword(scanner, 2, "r", 1, TOKEN_FOR);
				case 'a': return checkKeyword(scanner, 2, "lse", 3, TOKEN_FALSE);
				case 'n': return checkKeyword(scanner, 2, "", 0, TOKEN_FUNCTION);
			}
		}
		case 'i': if(scanner -> current - scanner -> start > 1) {
			switch(scanner -> start[1]) {
				case 'f': return checkKeyword(scanner, 2, "", 0, TOKEN_IF);
				case 'n': return checkKeyword(scanner, 2, "stof", 4, TOKEN_INSTOF);
			}
		}
		case 'I': return checkKeyword(scanner, 1, "nfinity", 7, TOKEN_INFINITY);
		case 'n': return checkKeyword(scanner, 1, "ull", 3, TOKEN_NULL);
		case 'N': return checkKeyword(scanner, 1, "aN", 2, TOKEN_NAN);
		case 'r': if(scanner -> current - scanner -> start > 1) {
			TokenType ttype = checkKeyword(scanner, 1, "eturn", 5, TOKEN_RETURN);
					
			if(ttype != TOKEN_RETURN) 
				ttype = checkKeyword(scanner, 1, "ecieve", 6, TOKEN_RECIEVE);
					
			return ttype;
		}
		case 's': if(scanner -> current - scanner -> start > 1) {
			switch(scanner -> start[1]) {
				case 'u': return checkKeyword(scanner, 2, "per", 3, TOKEN_SUPER);
				case 'h': if(scanner -> current - scanner -> start - 1 > 1) {
					TokenType ttype = checkKeyword(scanner, 2, "owl", 3, TOKEN_SHOWL);
					
					if(ttype != TOKEN_SHOWL) 
						ttype = checkKeyword(scanner, 2, "ow", 2, TOKEN_SHOW);
					
					return ttype;
				}
				case 'w': return checkKeyword(scanner, 2, "itch", 4, TOKEN_SWITCH);
				case 't': return checkKeyword(scanner, 2, "atic", 4, TOKEN_STATIC);
			}
		}
		case 't': if(scanner -> current - scanner -> start > 1) {
			switch(scanner -> start[1]) {
				case 'r': return checkKeyword(scanner, 2, "ue", 2, TOKEN_TRUE);
				case 'a': return checkKeyword(scanner, 2, "ke", 2, TOKEN_TAKE);
				case 'h': return checkKeyword(scanner, 2, "is", 2, TOKEN_THIS);
				case 'y': return checkKeyword(scanner, 2, "peof", 4, TOKEN_TYPEOF);
			}
		}
		case 'w': return checkKeyword(scanner, 1, "hile", 4, TOKEN_WHILE);
	}

	return TOKEN_IDENTIFIER;
}

static bool usableIdentifier(TokenType type) {
	return type == TOKEN_FALSE ||
	       type == TOKEN_INFINITY ||
		   type == TOKEN_RECIEVE ||
		   type == TOKEN_NAN ||
		   type == TOKEN_TRUE ||
		   type == TOKEN_NULL ||
		   type == TOKEN_TYPEOF ||
		   type == TOKEN_THIS ||
		   type == TOKEN_SUPER ||
		   type == TOKEN_IDENTIFIER;
}

static void identifier(Scanner* scanner) {
	while(isAlpha(peek(scanner)) || isDigit(peek(scanner))) advance(scanner);

	TokenType ttype = identifierType(scanner);

	if(scanner -> interpolationDepth > 0 && !usableIdentifier(ttype)) 
		enqueue(scanner, errorToken(scanner, "Reserved keywords are forbidden to use in string interpolation!"));
	else {
		ttype = scanner -> singleInterpolation ? (scanner -> globalInterpolation ? TOKEN_GLOBAL_INTERPOLATION : TOKEN_INTERPOLATION_IDENTIFIER) : ttype;

		enqueue(scanner, makeToken(scanner, ttype));
	}

	if(scanner -> singleInterpolation) {
		scanner -> singleInterpolation = false;
		scanner -> interpolationDepth--;
	}
}

static void string(Scanner* scanner, int recursionDepth, char quotation) {
	char c;

	while(!isAtEnd(scanner)) {
		if(scanner -> interpolationDepth - recursionDepth > 0) {
			skipWhitespace(scanner);
			
			scanner -> start = scanner -> current;
			
			c = advance(scanner);
			
			if(isDigit(c)) {
				if(scanner -> singleInterpolation) {
					enqueue(scanner, errorToken(scanner, "Expected an identifier in singular string interpolation, e.g. '$identifier'!"));
					
					scanner -> singleInterpolation = false;
					
					scanner -> interpolationDepth--;
				}
				else number(scanner);
			}
			else if(isAlpha(c)) identifier(scanner);
			else {
				if(scanner -> singleInterpolation && c != ':') {
					enqueue(scanner, errorToken(scanner, "Expected an identifier in singular string interpolation, e.g. '$identifier'!"));
					
					scanner -> singleInterpolation = false;
					
					scanner -> interpolationDepth--;
				}
				else {
					switch(c) {
						case '(': enqueue(scanner, makeToken(scanner, TOKEN_LEFT_PAREN)); break;
						case ')': enqueue(scanner, makeToken(scanner, TOKEN_RIGHT_PAREN)); break;
						case '}': {
							scanner -> interpolationDepth--;

							Token token;

							token.start  = scanner -> start;
							token.length = 1;
							token.type   = TOKEN_INTERPOLATION_END;
							token.line   = scanner -> line;

							enqueue(scanner, token);
							
							break;
						}
						case '+': enqueue(scanner, makeToken(scanner, match(scanner, '+') ? TOKEN_INCREMENT: TOKEN_PLUS)); break;
						case '-': enqueue(scanner, makeToken(scanner, match(scanner, '-') ? TOKEN_DECREMENT : TOKEN_MINUS)); break;
						case '*': enqueue(scanner, makeToken(scanner, TOKEN_ASTERISK)); break;
						case '/': enqueue(scanner, makeToken(scanner, TOKEN_SLASH)); break;	
						case ',': enqueue(scanner, makeToken(scanner, TOKEN_COMMA)); break;
						case '.': enqueue(scanner, makeToken(scanner, TOKEN_DOT)); break;
						case '^': enqueue(scanner, makeToken(scanner, TOKEN_XOR)); break;
						case '~': enqueue(scanner, makeToken(scanner, TOKEN_TILDE)); break;

						case '!': enqueue(scanner, makeToken(scanner, match(scanner, '=') ? TOKEN_NOT_EQUAL : TOKEN_NOT)); break;
						case '=': enqueue(scanner, makeToken(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL)); break;
						case '>': {
							if(match(scanner, '>')) enqueue(scanner, makeToken(scanner, TOKEN_RIGHT_SHIFT));
							else if(match(scanner, '=')) enqueue(scanner, makeToken(scanner, TOKEN_GREATER_EQUAL));
							
							enqueue(scanner, makeToken(scanner, TOKEN_GREATER)); break;
						}
						case '<': {
							if(match(scanner, '<')) enqueue(scanner, makeToken(scanner, TOKEN_LEFT_SHIFT));
							else if(match(scanner, '=')) enqueue(scanner, makeToken(scanner, TOKEN_LESS_EQUAL));
							
							enqueue(scanner, makeToken(scanner, TOKEN_LESS)); break;
						}
						case '%': enqueue(scanner, makeToken(scanner, TOKEN_MODULUS)); break;
						case ':': {
							if(scanner -> singleInterpolation) 
								scanner -> globalInterpolation = true;
							else enqueue(scanner, makeToken(scanner, match(scanner, ':') ? TOKEN_COLON_COLON : TOKEN_COLON));
							
							break;
						}
						case '?': enqueue(scanner, makeToken(scanner, TOKEN_QUESTION)); break;
						case '&': enqueue(scanner, makeToken(scanner, match(scanner, '&') ? TOKEN_AND : TOKEN_BITWISE_AND)); break;
						case '|': enqueue(scanner, makeToken(scanner, match(scanner, '|') ? TOKEN_OR : TOKEN_BITWISE_OR)); break;
						case '\0': {
							scanner -> current--;
							
							goto ustr;
						}
						case '[': enqueue(scanner, makeToken(scanner, TOKEN_LEFT_SQUARE)); break;
						case ']': enqueue(scanner, makeToken(scanner, TOKEN_RIGHT_SQUARE)); break;
						case '\'': 
						case '"': {
							char q = *scanner -> start++;
							
							advance(scanner);

							string(scanner, recursionDepth + 1, q);
							
							break;
						}
						default: enqueue(scanner, errorToken(scanner, "Unexpected character!"));
					}
				}	
			}
			
			scanner -> start = scanner -> current;
		} else {
			c = peek(scanner);
			
			if(c == '\n') scanner -> line++;

			if(c == '\\') advance(scanner);

			if(c == '$') {
				if(scanner -> stringFragmented) enqueue(scanner, makeToken(scanner, TOKEN_STRING_CONTINUE));
				else {
					scanner -> stringFragmented = true;
					
					enqueue(scanner, makeToken(scanner, TOKEN_STRING));
				}

				const char* prev = scanner -> current;
				
				advance(scanner);

				skipWhitespace(scanner);

				if(peek(scanner) == '{') {
					Token token;

					token.start  = prev;
					token.length = (int) (scanner -> current - prev) + 1;
					token.type   = TOKEN_INTERPOLATION_START;
					token.line   = scanner -> line;

					enqueue(scanner, token);
					
					advance(scanner);
				} else scanner -> singleInterpolation = true;

				scanner -> interpolationDepth++;

				scanner -> start = scanner -> current;
				
				continue;
			}

			if((c == '\'' && quotation == '\'') || (c == '"' && quotation == '"')) {
				if(scanner -> stringFragmented) {
					if(scanner -> current != scanner -> start)  enqueue(scanner, makeToken(scanner, TOKEN_STRING_CONTINUE));
				} else enqueue(scanner, makeToken(scanner, TOKEN_STRING));

				advance(scanner);

				break;
			}

			advance(scanner);
		}
	}
	
	ustr: 
		if(isAtEnd(scanner)) enqueue(scanner, errorToken(scanner, "Unterminated string!"));
}

Token scanToken(Scanner* scanner) {
	skipWhitespace(scanner);

	scanner -> start = scanner -> current;

	if(queueEmpty(scanner)) {
		if(isAtEnd(scanner)) return makeToken(scanner, TOKEN_EOF);

		char c = advance(scanner);

		if(isDigit(c)) {
			number(scanner);

			return dequeue(scanner);
		}
		else if(isAlpha(c)) {
			identifier(scanner);

			return dequeue(scanner);
		}

		switch(c) {
			case '(': return makeToken(scanner, TOKEN_LEFT_PAREN);
			case ')': return makeToken(scanner, TOKEN_RIGHT_PAREN);
			case '{': return makeToken(scanner, TOKEN_LEFT_BRACE);
			case '}': return makeToken(scanner, TOKEN_RIGHT_BRACE);
			case '+': return makeToken(scanner, match(scanner, '+') ? TOKEN_INCREMENT : TOKEN_PLUS);
			case '-': return makeToken(scanner, match(scanner, '-') ? TOKEN_DECREMENT : TOKEN_MINUS);
			case '*': return makeToken(scanner, TOKEN_ASTERISK);
			case '/': return makeToken(scanner, TOKEN_SLASH);
			case ';': return makeToken(scanner, TOKEN_SEMICOLON);
			case ',': return makeToken(scanner, TOKEN_COMMA);
			case '.': return makeToken(scanner, TOKEN_DOT);
			case '^': return makeToken(scanner, TOKEN_XOR);
			case '~': return makeToken(scanner, TOKEN_TILDE);

			case '!': return makeToken(scanner, match(scanner, '=') ? TOKEN_NOT_EQUAL : TOKEN_NOT);
			case '=': return makeToken(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
			case '>': {
				if(match(scanner, '>')) return makeToken(scanner, TOKEN_RIGHT_SHIFT);
				else if(match(scanner, '=')) return makeToken(scanner, TOKEN_GREATER_EQUAL);
				
				return makeToken(scanner, TOKEN_GREATER);
			}
			case '<': {
				if(match(scanner, '<')) return makeToken(scanner, TOKEN_LEFT_SHIFT);
				else if(match(scanner, '=')) return makeToken(scanner, TOKEN_LESS_EQUAL);
				
				return makeToken(scanner, TOKEN_LESS);
			}
			case '[': return makeToken(scanner, TOKEN_LEFT_SQUARE); break;
			case ']': return makeToken(scanner, TOKEN_RIGHT_SQUARE); break;
			case '%': return makeToken(scanner, TOKEN_MODULUS);
			case ':': return makeToken(scanner, match(scanner, ':') ? TOKEN_COLON_COLON : TOKEN_COLON);
			case '?': return makeToken(scanner, TOKEN_QUESTION);
			case '&': return makeToken(scanner, match(scanner, '&') ? TOKEN_AND : TOKEN_BITWISE_AND);
			case '|': return makeToken(scanner, match(scanner, '|') ? TOKEN_OR : TOKEN_BITWISE_OR);
			case '\'': 
			case '"': {
				char q = *scanner -> start++;

				scanner -> stringFragmented = false;

				string(scanner, 0, q);

				return dequeue(scanner);
			}
		}

		return errorToken(scanner, "Unexpected character!");
	}

	return dequeue(scanner);
}
