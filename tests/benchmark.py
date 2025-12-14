import time

def benchmark():
    print("Benchmarking CPython...")
    start = time.time()
    
    total = 0
    # Loop 100 triệu lần
    # Đây là so sánh công bằng: Dynamic Type check int cộng int
    for _ in range(100000000):
        total += 1
        
    end = time.time()
    print(f"CPython Result: {total}")
    print(f"Time taken: {end - start:.6f} seconds")

if __name__ == "__main__":
    benchmark()
