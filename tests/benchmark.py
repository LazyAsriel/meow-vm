import time
import sys

def benchmark(limit):
    print(f"Benchmarking CPython (Meow Logic)... Input: {limit}")
    start = time.time()
    
    # Mapping registers
    reg_0_total = 0
    reg_1_i = 0
    reg_2_step = 1
    reg_3_limit = limit
    
    # Logic:
    # ADD 0 0 2
    # ADD 1 1 2
    # LT 4 1 3
    # JUMP_IF_TRUE
    
    while True:
        reg_0_total = reg_0_total + reg_2_step
        reg_1_i = reg_1_i + reg_2_step
        
        # Check LT
        if reg_1_i < reg_3_limit:
            continue
        else:
            break
            
    end = time.time()
    print(f"CPython Result (Total): {reg_0_total}")
    print(f"Time taken: {end - start:.6f} seconds")

if __name__ == "__main__":
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 100000000
    benchmark(limit)