import sys
import time

sys.setrecursionlimit(2000)

def add_recursive(n, acc):
    if n < 1:
        return acc
    return add_recursive(n - 1, acc + n)

def main():
    n_input = 1000
    acc_input = 0
    
    iterations = 10000 
    
    print(f"🔥 Bắt đầu benchmark Python Recursive (Depth={n_input}, Iterations={iterations})...")
    
    start_time = time.time()
    
    val = 0
    for _ in range(iterations):
        val = add_recursive(n_input, acc_input)
        
    end_time = time.time()
    
    total_time_ms = (end_time - start_time) * 1000
    avg_time_ms = total_time_ms / iterations
    
    print(f"--------------------------------------------------")
    print(f"✅ Kết quả (R3): {val}") # Mong đợi: 500500
    print(f"⏱️ Tổng thời gian: {total_time_ms:.2f} ms")
    print(f"⚡ Trung bình/lần chạy: {avg_time_ms:.4f} ms")
    print(f"--------------------------------------------------")

if __name__ == "__main__":
    main()
