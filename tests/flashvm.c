#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// --- Types ---
typedef struct { long i; } Value; // Tối giản chỉ dùng Int cho bài test này

// --- VM State ---
#define STACK_MAX 256
#define VARS_MAX 16 

typedef struct {
    Value stack[STACK_MAX];
    Value vars[VARS_MAX]; 
    int sp;
} VM;

// --- Instruction Set ---
typedef enum {
    OP_CONST,       
    OP_LOAD_VAR,    
    OP_STORE_VAR,   
    OP_ADD,
    OP_LESS,         // So sánh <
    OP_JUMP_IF_TRUE, // Nhảy nếu True (khác logic cũ tí)
    OP_PRINT,       
    OP_HALT
} OpCode;

// --- Interpreter ---
void run(int* code, int code_size, Value* constants) {
    VM vm;
    vm.sp = 0;
    
    register int rip = 0; 
    register int instruction;
    
    // Cache stack pointer vào register để tối ưu
    register Value* stack = vm.stack; 
    register int sp = 0;
    
    while (1) {
        instruction = code[rip];
        switch (instruction) {
            case OP_CONST: {
                stack[sp++].i = constants[code[rip + 1]].i;
                rip += 2;
                break;
            }
            case OP_LOAD_VAR: {
                stack[sp++].i = vm.vars[code[rip + 1]].i;
                rip += 2;
                break;
            }
            case OP_STORE_VAR: {
                vm.vars[code[rip + 1]].i = stack[--sp].i;
                rip += 2;
                break;
            }
            case OP_ADD: {
                // Stack: [..., a, b] -> a + b
                sp--;
                stack[sp-1].i += stack[sp].i;
                rip++;
                break;
            }
            case OP_LESS: {
                // Stack: [..., a, b] -> (a < b)
                sp--;
                stack[sp-1].i = (stack[sp-1].i < stack[sp].i);
                rip++;
                break;
            }
            case OP_JUMP_IF_TRUE: {
                int offset = code[rip + 1];
                sp--;
                if (stack[sp].i != 0) { // If True
                    rip += offset;
                } else {
                    rip += 2;
                }
                break;
            }
            case OP_PRINT: {
                sp--;
                printf("Result (Total): %ld\n", stack[sp].i);
                rip++;
                break;
            }
            case OP_HALT:
                return;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <limit>\n", argv[0]);
        return 1;
    }
    long limit = atol(argv[1]);

    // Constants setup
    Value constants[] = {
        {0},    // [0]
        {1},    // [1]
        {limit} // [2]
    };

    // --- MAPPING LOGIC MEOW SANG STACK ---
    // Register Map:
    // vars[0] = Total
    // vars[1] = i
    // vars[2] = Step (1)
    // vars[3] = Limit

    int code[] = {
        // --- INIT ---
        // chunk.write_byte(LOAD_INT); 0; 0; -> var[0] = 0
        OP_CONST, 0, OP_STORE_VAR, 0,
        // chunk.write_byte(LOAD_INT); 1; 0; -> var[1] = 0
        OP_CONST, 0, OP_STORE_VAR, 1,
        // chunk.write_byte(LOAD_INT); 2; 1; -> var[2] = 1 (step)
        OP_CONST, 1, OP_STORE_VAR, 2,
        // chunk.write_byte(LOAD_INT); 3; LIMIT; -> var[3] = limit
        OP_CONST, 2, OP_STORE_VAR, 3,

        // --- LOOP START (Offset tương đối từ đây = 0) ---
        
        // 1. ADD 0 0 2 (Total = Total + Step)
        // Stack conversion: Load 0, Load 2, Add, Store 0
        /* 0 */ OP_LOAD_VAR, 0,
        /* 2 */ OP_LOAD_VAR, 2,
        /* 4 */ OP_ADD,
        /* 5 */ OP_STORE_VAR, 0, // Tổng length: 7 ints

        // 2. ADD 1 1 2 (i = i + Step)
        // Stack conversion: Load 1, Load 2, Add, Store 1
        /* 7 */ OP_LOAD_VAR, 1,
        /* 9 */ OP_LOAD_VAR, 2,
        /* 11 */ OP_ADD,
        /* 12 */ OP_STORE_VAR, 1, // Tổng length: 7 ints

        // 3. LT 4 1 3 (Check i < Limit)
        // Stack conversion: Load 1 (i), Load 3 (limit), LESS
        // (Kết quả nằm trên stack, không cần store vào vars[4] như register VM)
        /* 14 */ OP_LOAD_VAR, 1,
        /* 16 */ OP_LOAD_VAR, 3,
        /* 18 */ OP_LESS,         // Tổng length: 5 ints

        // 4. JUMP_IF_TRUE loop_start
        /* 19 */ OP_JUMP_IF_TRUE, -19, 
        // Tính offset: Ta đang ở index 19. Loop start ở index 0 (tính từ lệnh đầu trong loop).
        // Tổng length loop body = 7 + 7 + 5 = 19.
        // Vậy phải lùi 19 bước.

        // --- END ---
        OP_LOAD_VAR, 0, // Load Total ra để in
        OP_PRINT,
        OP_HALT
    };

    printf("Benchmarking FlashVM (Meow Logic)... Input: %ld\n", limit);
    clock_t start = clock();
    run(code, sizeof(code)/sizeof(int), constants);
    clock_t end = clock();
    
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", time_taken);

    return 0;
}