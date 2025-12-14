#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h> // <--- Đã thêm thư viện này

// ==========================================
// 1. COMMON & MEMORY
// ==========================================

// Helper macro for allocation
#define ALLOC(type, count) (type*)malloc(sizeof(type) * (count))

typedef enum { VAL_BOOL, VAL_NIL, VAL_INT, VAL_OBJ } ValueType;
typedef enum { OBJ_STRING } ObjType;

typedef struct Obj {
    ObjType type;
    struct Obj* next;
} Obj;

typedef struct {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
} ObjString;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        long number;
        Obj* obj;
    } as;
} Value;

// Macros Check Type
#define IS_BOOL(v)    ((v).type == VAL_BOOL)
#define IS_NIL(v)     ((v).type == VAL_NIL)
#define IS_INT(v)     ((v).type == VAL_INT)
#define IS_OBJ(v)     ((v).type == VAL_OBJ)

// Helper Check Object Type
#define AS_OBJ(v)     ((v).as.obj)
#define IS_STRING(v)  (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_STRING)

// Accessor Macros
#define AS_STRING(v)  ((ObjString*)AS_OBJ(v))
#define AS_CSTRING(v) (AS_STRING(v)->chars)
#define AS_BOOL(v)    ((v).as.boolean)
#define AS_INT(v)     ((v).as.number)

// Value Constructors (Đổi tên thành _VAL để tránh xung đột với Enum)
#define BOOL_VAL(v)   ((Value){VAL_BOOL, {.boolean = v}})
#define NIL_VAL       ((Value){VAL_NIL, {.number = 0}})
#define INT_VAL(v)    ((Value){VAL_INT, {.number = v}})
#define OBJ_VAL(v)    ((Value){VAL_OBJ, {.obj = (Obj*)v}})

// --- Hashing (FNV-1a) ---
uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

// ==========================================
// 2. VM STRUCTURE & OPCODES
// ==========================================

#define STACK_MAX 256
#define TABLE_MAX_LOAD 0.75

typedef enum {
    OP_CONSTANT, OP_NIL, OP_TRUE, OP_FALSE,
    OP_POP, OP_GET_GLOBAL, OP_SET_GLOBAL, OP_DEFINE_GLOBAL,
    OP_EQUAL, OP_GREATER, OP_LESS,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_NOT, OP_NEGATE,
    OP_PRINT, OP_JUMP, OP_JUMP_IF_FALSE, OP_LOOP,
    OP_RETURN
} OpCode;

// Hash Table Entry
typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

// Chunk definition moved up needed for VM
typedef struct {
    int count;
    int capacity;
    Value* values;
} ValueArray;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    ValueArray constants;
} Chunk;

typedef struct {
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    Obj* objects; // GC list head
    Chunk* chunk; // Reference to current chunk
} VM;

VM vm;
Chunk mainChunk;

// --- Chunk Utils ---
void initChunk(Chunk* chunk) {
    chunk->count = 0; chunk->capacity = 0; chunk->code = NULL;
    chunk->constants.count = 0; chunk->constants.capacity = 0; chunk->constants.values = NULL;
}

void writeChunk(Chunk* chunk, uint8_t byte) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = (oldCapacity < 8) ? 8 : oldCapacity * 2;
        chunk->code = realloc(chunk->code, sizeof(uint8_t) * chunk->capacity);
    }
    chunk->code[chunk->count] = byte;
    chunk->count++;
}

void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = (oldCapacity < 8) ? 8 : oldCapacity * 2;
        array->values = realloc(array->values, sizeof(Value) * array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

int addConstant(Chunk* chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

// --- Table Utils ---
void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash % capacity;
    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == NULL || entry->key == key) return entry;
        index = (index + 1) % capacity;
    }
}

void adjustCapacity(Table* table, int capacity) {
    Entry* entries = ALLOC(Entry, capacity); // Used ALLOC here
    for (int i = 0; i < capacity; i++) entries[i].key = NULL;
    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;
        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }
    free(table->entries);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = table->capacity < 8 ? 8 : table->capacity * 2;
        adjustCapacity(table, capacity);
    }
    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey) table->count++;
    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) return false;
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;
    *value = entry->value;
    return true;
}

// ==========================================
// 3. MEMORY MANAGER
// ==========================================

Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)malloc(size);
    object->type = type;
    object->next = vm.objects;
    vm.objects = object;
    return object;
}

ObjString* allocateString(char* chars, int length, uint32_t hash) {
    ObjString* string = (ObjString*)allocateObject(sizeof(ObjString), OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    return string;
}

ObjString* copyString(const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    char* heapChars = malloc(length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

// ==========================================
// 4. VM IMPLEMENTATION
// ==========================================

void push(Value value) { *vm.stackTop = value; vm.stackTop++; }
Value pop() { vm.stackTop--; return *vm.stackTop; }
Value peek(int distance) { return vm.stackTop[-1 - distance]; }

bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

void interpret() {
    vm.ip = mainChunk.code;
    vm.stackTop = vm.stack;
    vm.chunk = &mainChunk;
    
    for (;;) {
        uint8_t instruction = *vm.ip++;
        switch (instruction) {
            case OP_RETURN: return;
            case OP_CONSTANT: {
                Value constant = vm.chunk->constants.values[*vm.ip++];
                push(constant);
                break;
            }
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_POP: pop(); break;
            case OP_GET_GLOBAL: {
                ObjString* name = AS_STRING(vm.chunk->constants.values[*vm.ip++]);
                Value value;
                if (!tableGet(&vm.globals, name, &value)) {
                    printf("Undefined variable '%s'.\n", name->chars);
                    exit(1);
                }
                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = AS_STRING(vm.chunk->constants.values[*vm.ip++]);
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = AS_STRING(vm.chunk->constants.values[*vm.ip++]);
                if (tableSet(&vm.globals, name, peek(0))) { 
                    tableSet(&vm.globals, name, peek(0)); 
                }
                break;
            }
            case OP_EQUAL: {
                Value b = pop(); Value a = pop();
                if (a.type != b.type) push(BOOL_VAL(false));
                else if (IS_INT(a)) push(BOOL_VAL(AS_INT(a) == AS_INT(b)));
                else push(BOOL_VAL(a.as.number == b.as.number)); 
                break;
            }
            case OP_GREATER: {
                Value b = pop(); Value a = pop();
                push(BOOL_VAL(AS_INT(a) > AS_INT(b)));
                break;
            }
            case OP_LESS: {
                Value b = pop(); Value a = pop();
                push(BOOL_VAL(AS_INT(a) < AS_INT(b)));
                break;
            }
            case OP_ADD: {
                Value b = peek(0); Value a = peek(1);
                if (IS_STRING(a) && IS_STRING(b)) { // Đã có macro IS_STRING
                    ObjString* s2 = AS_STRING(pop());
                    ObjString* s1 = AS_STRING(pop());
                    int len = s1->length + s2->length;
                    char* chars = malloc(len + 1);
                    memcpy(chars, s1->chars, s1->length);
                    memcpy(chars + s1->length, s2->chars, s2->length);
                    chars[len] = '\0';
                    ObjString* result = allocateString(chars, len, hashString(chars, len));
                    push(OBJ_VAL(result));
                } else if (IS_INT(a) && IS_INT(b)) {
                    long bVal = AS_INT(pop()); long aVal = AS_INT(pop());
                    push(INT_VAL(aVal + bVal));
                }
                break;
            }
            case OP_SUB: { long b = AS_INT(pop()); long a = AS_INT(pop()); push(INT_VAL(a - b)); break; }
            case OP_PRINT: {
                Value v = pop();
                if (IS_INT(v)) printf("%ld\n", AS_INT(v));
                else if (IS_BOOL(v)) printf(AS_BOOL(v) ? "true\n" : "false\n");
                else if (IS_STRING(v)) printf("%s\n", AS_CSTRING(v));
                else printf("nil\n");
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = (vm.ip[0] << 8) | vm.ip[1];
                vm.ip += 2;
                if (isFalsey(peek(0))) vm.ip += offset;
                break;
            }
            case OP_JUMP: {
                uint16_t offset = (vm.ip[0] << 8) | vm.ip[1];
                vm.ip += 2;
                vm.ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = (vm.ip[0] << 8) | vm.ip[1];
                vm.ip += 2;
                vm.ip -= offset;
                break;
            }
        }
    }
}

// ==========================================
// 5. COMPILER (SCANNER & PARSER)
// ==========================================

typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

Scanner scanner;

typedef enum {
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN, TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_MINUS, TOKEN_PLUS, TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
    TOKEN_BANG, TOKEN_BANG_EQUAL, TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE, TOKEN_FOR, TOKEN_FUN,
    TOKEN_IF, TOKEN_NIL, TOKEN_OR, TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER,
    TOKEN_THIS, TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,
    TOKEN_ERROR, TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

Token scanToken() {
    while (*scanner.current == ' ' || *scanner.current == '\r' || *scanner.current == '\t' || *scanner.current == '\n') {
        if (*scanner.current == '\n') scanner.line++;
        scanner.current++;
    }
    scanner.start = scanner.current;
    if (*scanner.current == '\0') return (Token){TOKEN_EOF, scanner.start, 0, scanner.line};

    char c = *scanner.current++;
    if (isdigit(c)) { // <ctype.h> needed
        while (isdigit(*scanner.current)) scanner.current++;
        return (Token){TOKEN_NUMBER, scanner.start, (int)(scanner.current - scanner.start), scanner.line};
    }
    if (isalpha(c)) { // <ctype.h> needed
        while (isalnum(*scanner.current)) scanner.current++;
        int len = (int)(scanner.current - scanner.start);
        TokenType type = TOKEN_IDENTIFIER;
        // Simple manual keyword check
        if (len == 3 && memcmp(scanner.start, "var", 3) == 0) type = TOKEN_VAR;
        if (len == 5 && memcmp(scanner.start, "print", 5) == 0) type = TOKEN_PRINT;
        if (len == 2 && memcmp(scanner.start, "if", 2) == 0) type = TOKEN_IF;
        if (len == 4 && memcmp(scanner.start, "else", 4) == 0) type = TOKEN_ELSE;
        if (len == 5 && memcmp(scanner.start, "while", 5) == 0) type = TOKEN_WHILE;
        if (len == 4 && memcmp(scanner.start, "true", 4) == 0) type = TOKEN_TRUE;
        if (len == 5 && memcmp(scanner.start, "false", 5) == 0) type = TOKEN_FALSE;
        return (Token){type, scanner.start, len, scanner.line};
    }
    if (c == '"') {
        while (*scanner.current != '"' && *scanner.current != '\0') scanner.current++;
        if (*scanner.current == '"') scanner.current++;
        return (Token){TOKEN_STRING, scanner.start + 1, (int)(scanner.current - scanner.start) - 2, scanner.line};
    }

    switch (c) {
        case '(': return (Token){TOKEN_LEFT_PAREN, scanner.start, 1, scanner.line};
        case ')': return (Token){TOKEN_RIGHT_PAREN, scanner.start, 1, scanner.line};
        case '{': return (Token){TOKEN_LEFT_BRACE, scanner.start, 1, scanner.line};
        case '}': return (Token){TOKEN_RIGHT_BRACE, scanner.start, 1, scanner.line};
        case ';': return (Token){TOKEN_SEMICOLON, scanner.start, 1, scanner.line};
        case '+': return (Token){TOKEN_PLUS, scanner.start, 1, scanner.line};
        case '-': return (Token){TOKEN_MINUS, scanner.start, 1, scanner.line};
        case '=': return (Token){TOKEN_EQUAL, scanner.start, 1, scanner.line};
        case '<': return (Token){TOKEN_LESS, scanner.start, 1, scanner.line};
        case '>': return (Token){TOKEN_GREATER, scanner.start, 1, scanner.line};
    }
    return (Token){TOKEN_ERROR, scanner.start, 1, scanner.line};
}

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

Parser parser;

void errorAtCurrent(const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error: %s\n", parser.current.line, message);
    parser.hadError = true;
}

void advance() {
    parser.previous = parser.current;
    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent("Unexpected character.");
    }
}

void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

void emitByte(uint8_t byte) { writeChunk(&mainChunk, byte); }
void emitBytes(uint8_t byte1, uint8_t byte2) { emitByte(byte1); emitByte(byte2); }

int makeConstant(Value value) { return addConstant(&mainChunk, value); }
void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

// Forward decls
void expression();
void statement();
void declaration();

void number() {
    long value = strtol(parser.previous.start, NULL, 10);
    emitConstant(INT_VAL(value));
}

void string() {
    emitConstant(OBJ_VAL(copyString(parser.previous.start, parser.previous.length)));
}

void variable(bool canAssign) {
    int arg = makeConstant(OBJ_VAL(copyString(parser.previous.start, parser.previous.length)));
    if (canAssign && parser.current.type == TOKEN_EQUAL) {
        advance();
        expression();
        emitBytes(OP_SET_GLOBAL, (uint8_t)arg);
    } else {
        emitBytes(OP_GET_GLOBAL, (uint8_t)arg);
    }
}

void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

void primary() {
    if (parser.previous.type == TOKEN_FALSE) emitByte(OP_FALSE);
    else if (parser.previous.type == TOKEN_TRUE) emitByte(OP_TRUE);
    else if (parser.previous.type == TOKEN_NUMBER) number();
    else if (parser.previous.type == TOKEN_STRING) string();
    else if (parser.previous.type == TOKEN_IDENTIFIER) variable(true); 
    else if (parser.previous.type == TOKEN_LEFT_PAREN) grouping();
}

void term() {
    primary(); 
    while (parser.current.type == TOKEN_PLUS || parser.current.type == TOKEN_MINUS) {
        TokenType op = parser.current.type;
        advance();
        primary(); 
        emitByte(op == TOKEN_PLUS ? OP_ADD : OP_SUB);
    }
}

void comparison() {
    term();
    while (parser.current.type == TOKEN_LESS || parser.current.type == TOKEN_GREATER) {
        TokenType op = parser.current.type;
        advance();
        term();
        emitByte(op == TOKEN_LESS ? OP_LESS : OP_GREATER);
    }
}

void expression() {
    comparison();
}

void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff); emitByte(0xff);
    return mainChunk.count - 2;
}

void patchJump(int offset) {
    int jump = mainChunk.count - offset - 2;
    mainChunk.code[offset] = (jump >> 8) & 0xff;
    mainChunk.code[offset + 1] = jump & 0xff;
}

void emitLoop(int loopStart) {
    emitByte(OP_LOOP);
    int offset = mainChunk.count - loopStart + 2;
    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

void ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); 
    statement();
    
    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP); 
    
    if (parser.current.type == TOKEN_ELSE) {
        advance();
        statement();
    }
    patchJump(elseJump);
}

void whileStatement() {
    int loopStart = mainChunk.count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    
    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);
    
    patchJump(exitJump);
    emitByte(OP_POP);
}

void block() {
    while (parser.current.type != TOKEN_RIGHT_BRACE && parser.current.type != TOKEN_EOF) {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

void statement() {
    if (parser.current.type == TOKEN_PRINT) {
        advance();
        printStatement();
    } else if (parser.current.type == TOKEN_IF) {
        advance();
        ifStatement();
    } else if (parser.current.type == TOKEN_WHILE) {
        advance();
        whileStatement();
    } else if (parser.current.type == TOKEN_LEFT_BRACE) {
        advance();
        block();
    } else {
        expressionStatement();
    }
}

void varDeclaration() {
    consume(TOKEN_IDENTIFIER, "Expect variable name.");
    int global = makeConstant(OBJ_VAL(copyString(parser.previous.start, parser.previous.length)));
    
    if (parser.current.type == TOKEN_EQUAL) {
        advance();
        expression();
    } else {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    emitBytes(OP_DEFINE_GLOBAL, (uint8_t)global);
}

void declaration() {
    if (parser.current.type == TOKEN_VAR) {
        advance();
        varDeclaration();
    } else {
        statement();
    }
}

void compile(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    parser.hadError = false;
    parser.panicMode = false;
    
    advance();
    while (parser.current.type != TOKEN_EOF) {
        declaration();
    }
    emitByte(OP_RETURN);
}

// ==========================================
// 6. MAIN
// ==========================================

int main(int argc, char* argv[]) {
    initTable(&vm.globals);
    initChunk(&mainChunk);

    // TEST SCRIPT
    const char* source = 
        "var start = 1000;"
        "var i = 0;"
        "var sum = 0;"
        "print \"FlashLang Starting...\";"
        
        "while (i < 10) {"
        "  sum = sum + i;"
        "  print \"Count: \" + i;"
        "  i = i + 1;"
        "}"
        
        "if (sum > 20) {"
        "  print \"Sum is big: \" + sum;"
        "} else {"
        "  print \"Sum is small\";"
        "}"
        
        "print \"Done.\";";

    printf("Source:\n%s\n\n", source);
    printf("Compiling...\n");
    compile(source);
    
    if (parser.hadError) return 1;

    printf("Running...\n----------------\n");
    interpret();
    printf("----------------\n");

    return 0;
}