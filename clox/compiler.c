#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"     
#endif       

typedef struct {
	Token current;
	Token previous;
	bool hadError;
	bool panicMode;
} Parser;

typedef enum {
	PREC_NONE,
	PREC_ASSIGNMENT,  // =        
	PREC_OR,          // or       
	PREC_AND,         // and      
	PREC_EQUALITY,    // == !=    
	PREC_COMPARISON,  // < > <= >=
	PREC_TERM,        // + -      
	PREC_FACTOR,      // * /      
	PREC_UNARY,       // ! -      
	PREC_CALL,        // . ()     
	PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

typedef struct {
	Token name;
	int depth;
} Local;

typedef struct Compiler {
	Local locals[UINT8_COUNT];
	int localCount;
	int scopeDepth;
} Compiler;

Parser parser;

Compiler* current = NULL;

Chunk* compilingChunk;

static void adLocal(Token name);
static void advance();
static void beginScope();
static void binary(bool canAssign);
static bool check(TokenType type);
static void block();
static void consume(TokenType type, const char* message);
static Chunk* currentChunk();
static void declaration();
static void declareVariable();
static void defineVariable(uint8_t gobal);
static void emitByte(uint8_t byte);
static void emitBytes(uint8_t byte1, uint8_t byte2);
static void endCompiler();
static void endScope();
static void emitConstant(Value value);
static void error(const char* message);
static void errorAt(Token* token, const char* message);
static void errorAtCurrent(const char* message);
static void expression();
static void expressionStatement();
static ParseRule* getRule(TokenType type);
static void grouping(bool canAssign);
static uint8_t identifierConstant(Token* name);
static bool identifiersEqual(Token* a, Token* b);
static void literal(bool canAssign);
static void markInitialized();
static bool match(TokenType type);
static void namedVariable(Token token, bool canAccess);
static void number(bool canAssign);
static uint8_t makeConstant(Value value);
static void parsePrecedence(Precedence precedence);
static uint8_t parseVariable(const char* errorMessage);
static void printStatement();
static int resolveLocal(Compiler* compiler, Token* name);
static void statement();
static void string(bool canAssign);
static void synchronize();
static void varDeclaration();
static void variable(bool canAccess);
static void unary(bool canAssign);

ParseRule rules[] = {
  { grouping, NULL,    PREC_NONE },       // TOKEN_LEFT_PAREN      
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_PAREN     
  { NULL,     NULL,    PREC_NONE },      // TOKEN_LEFT_BRACE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACE     
  { NULL,     NULL,    PREC_NONE },       // TOKEN_COMMA           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_DOT             
  { unary,    binary,  PREC_TERM },       // TOKEN_MINUS           
  { NULL,     binary,  PREC_TERM },       // TOKEN_PLUS            
  { NULL,     NULL,    PREC_NONE },       // TOKEN_SEMICOLON       
  { NULL,     binary,  PREC_FACTOR },     // TOKEN_SLASH           
  { NULL,     binary,  PREC_FACTOR },     // TOKEN_STAR            
  { unary,    NULL,    PREC_NONE },       // TOKEN_BANG            
  { NULL,     binary,  PREC_EQUALITY },   // TOKEN_BANG_EQUAL      
  { NULL,     NULL,    PREC_NONE },       // TOKEN_EQUAL           
  { NULL,     binary,  PREC_EQUALITY },   // TOKEN_EQUAL_EQUAL     
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER         
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER_EQUAL   
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS            
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS_EQUAL      
  { variable,     NULL,    PREC_NONE },       // TOKEN_IDENTIFIER      
  { string,   NULL,    PREC_NONE },       // TOKEN_STRING          
  { number,   NULL,    PREC_NONE },       // TOKEN_NUMBER          
  { NULL,     NULL,    PREC_NONE },       // TOKEN_AND             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_CLASS           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_ELSE            
  { literal,  NULL,    PREC_NONE },       // TOKEN_FALSE           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_FOR             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_FUN             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_IF              
  { literal,  NULL,    PREC_NONE },       // TOKEN_NIL             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_OR              
  { NULL,     NULL,    PREC_NONE },       // TOKEN_PRINT           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RETURN          
  { NULL,     NULL,    PREC_NONE },       // TOKEN_SUPER           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_THIS            
  { literal,  NULL,    PREC_NONE },       // TOKEN_TRUE            
  { NULL,     NULL,    PREC_NONE },       // TOKEN_VAR             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_WHILE           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_ERROR           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_EOF             
};

static void initCompiler(Compiler* compiler) {
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	current = compiler;
}

bool compile(const char* source, Chunk* chunk) {
	initScanner(source);
	Compiler compiler;
	initCompiler(&compiler);
	compilingChunk = chunk;

	parser.hadError = false;
	parser.panicMode = false;

	advance();

	while (!match(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_EOF, "Expect end of expression.");
	endCompiler();
	return !parser.hadError;
}

// *******************
// **** Front end ****
// *******************

static void advance() {
	parser.previous = parser.current;

	for (;;) {
		parser.current = scanToken();
		if (parser.current.type != TOKEN_ERROR) break;

		errorAtCurrent(parser.current.start);
	}
}

static bool check(TokenType type) {
	return parser.current.type == type;
}

static void consume(TokenType type, const char* message) {
	if (check(type)) {
		advance();
		return;
	}

	errorAtCurrent(message);
}

static void error(const char* message) {
	errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
	errorAt(&parser.current, message);
}

static void errorAt(Token* token, const char* message) {
	if (parser.panicMode) return;
	parser.panicMode = true;

	fprintf(stderr, "[line %d] Error", token->line);

	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	}
	else if (token->type == TOKEN_ERROR) {
		// Nothing.
	}
	else {
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}

	fprintf(stderr, ": %s\n", message);

	parser.hadError = true;
}

static bool match(TokenType type) {
	if (check(type)) {
		advance();
		return true;
	}

	return false;
}

// ******************
// **** Back end ****
// ******************


// Statement related functions 
// ---------------------------------

static void adLocal(Token name) {
	if (current->localCount == UINT8_MAX) {
		error("Too many local variables in function.");
		return;
	}

	Local* local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = -1;
}

static void beginScope() {
	current->scopeDepth++;
}

static void block() {
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void declaration() {
	if (match(TOKEN_VAR)) {
		varDeclaration();
	}
	else {
		statement();
	}

	if (parser.panicMode) synchronize();
}

static void declareVariable() {
	// Global variables are implicitly declared
	if (current->scopeDepth == 0) return;

	Token* name = &parser.previous;

	// Make sure no redeclaration of variable name in same scope 
	for (int i = current->localCount - 1; i >= 0; i--) {
		Local* local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth) {
			break;
		}

		if (identifiersEqual(name, &local->name)) {
			error("Variable with this name already declared in this scope.");
		}
	}

	adLocal(*name);
}

static void defineVariable(uint8_t global) {
	if (current->scopeDepth > 0) {
		markInitialized();
		return;
	}

	emitBytes(OP_DEFINE_GLOBAL, global);
}

static void markInitialized() {
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void endScope() {
	current->scopeDepth--;

	while (current->localCount > 0
		&& current->locals[current->localCount - 1].depth > current->scopeDepth) {
		emitByte(OP_POP);
		current->localCount--;
	}
}

static void expressionStatement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
	emitByte(OP_POP);
}

static uint8_t identifierConstant(Token* token) {
	return makeConstant(OBJ_VAL(copyString(token->start, token->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
	if (a->length != b->length) return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

static uint8_t parseVariable(const char* errorMessage) {
	consume(TOKEN_IDENTIFIER, errorMessage);

	declareVariable();
	if (current->scopeDepth > 0) return 0;

	return identifierConstant(&parser.previous);
}

static void printStatement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
	emitByte(OP_PRINT);
}

static int resolveLocal(Compiler* compiler, Token* name) {
	for (int i = current->localCount - 1; i >= 0; i--) {
		Local* local = &current->locals[i];

		if (identifiersEqual(name, &local->name)) {
			if (local->depth == -1) {
				error("Cannot read local variable in its own initializer.");
			}

			return i;
		}
	}

	return -1;
}

static void statement() {
	if (match(TOKEN_PRINT)) {
		printStatement();
	}
	else if (match(TOKEN_LEFT_BRACE)) {
		beginScope();
		block();
		endScope();
	}
	else {
		expressionStatement();
	}
}

static void synchronize() {
	parser.panicMode = false;

	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON) return;
		switch (parser.current.type) {
		case TOKEN_CLASS:
		case TOKEN_FUN:
		case TOKEN_VAR:
		case TOKEN_FOR:
		case TOKEN_IF:
		case TOKEN_WHILE:
		case TOKEN_PRINT:
		case TOKEN_RETURN:
			return;

		default:
			// Do nothing.                                  
			;
		}

		advance();
	}
}

static void varDeclaration() {
	uint8_t global = parseVariable("Expect variable name");

	if (match(TOKEN_EQUAL)) {
		expression();
	}
	else {
		emitByte(OP_NIL);
	}

	consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration");

	defineVariable(global);
}

// Expression related functions
// ----------------------------

static void binary(bool canAssign) {
	// Remember the operator
	TokenType operatorType = parser.previous.type;

	// Compile the right operand.
	ParseRule* rule = getRule(operatorType);
	parsePrecedence((Precedence)(rule->precedence + 1));


	// Emit the operator instruction.
	switch (operatorType) {
	case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
	case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
	case TOKEN_GREATER: emitByte(OP_GREATER); break;
	case TOKEN_GREATER_EQUAL: emitByte(OP_LESS, OP_NOT); break;
	case TOKEN_LESS: emitByte(OP_LESS); break;
	case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;
	case TOKEN_PLUS: emitByte(OP_ADD); break;
	case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
	case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
	case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
	default:
		return; // Unreachable
	}
}

static Chunk* currentChunk() {
	return compilingChunk;
}

static void expression() {
	parsePrecedence(PREC_ASSIGNMENT);
}

static ParseRule* getRule(TokenType type) {
	return &rules[type];
}

static void grouping(bool canAssign) {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void literal(bool canAssign) {
	switch (parser.previous.type) {
	case TOKEN_FALSE: emitByte(OP_FALSE); break;
	case TOKEN_NIL: emitByte(OP_NIL); break;
	case TOKEN_TRUE: emitByte(OP_TRUE); break;
	default:
		return; // Unreachable
	}
}

static void namedVariable(Token name, bool canAssign) {
	uint8_t getOp, setOp;
	int arg = resolveLocal(current, &name);

	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}
	else {
		arg = identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}

	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		emitBytes(setOp, (uint8_t)arg);
	}
	else {
		emitBytes(getOp, (uint8_t)arg);
	}
}

static void number(bool canAssign) {
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

static void parsePrecedence(Precedence precedence) {
	advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	if (prefixRule == NULL) {
		error("Expect expression.");
		return;
	}

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(canAssign);

	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule(canAssign);
	}

	if (canAssign && match(TOKEN_EQUAL)) {
		error("Invalid assignment target.");
	}
}

static void string(bool canAssign) {
	emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void unary(bool canAssign) {
	TokenType operatorType = parser.previous.type;

	// Compile the operand
	parsePrecedence(PREC_UNARY);

	// Emit the operator instruction
	switch (operatorType) {
	case TOKEN_MINUS: emitByte(OP_NEGATE); break;
	case TOKEN_BANG: emitByte(OP_NOT); break;
	default: return; // Unreachable
	}
}

static void variable(bool canAssign) {
	namedVariable(parser.previous, canAssign);
}

// Code generation related functions
// ---------------------------------

static void emitByte(uint8_t byte) {
	writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte(byte1);
	emitByte(byte2);
}

static void emitConstant(Value value) {
	emitBytes(OP_CONSTANT, makeConstant(value));
}

static void emitReturn() {
	emitByte(OP_RETURN);
}

static void endCompiler() {
#ifdef DEBUG_PRINT_CODE                      
	if (!parser.hadError) {
		disassembleChunk(currentChunk(), "code");
	}
#endif   
	emitReturn();
}

static uint8_t makeConstant(Value value) {
	int constant = addConstant(currentChunk(), value);
	if (constant > UINT8_MAX) {
		error("Too many constants in one chunk.");
		return 0;
	}

	return (uint8_t)constant;
}