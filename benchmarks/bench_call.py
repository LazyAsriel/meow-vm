import sys
import time

# ⚠️ QUAN TRỌNG: Python có giới hạn stack mặc định là 1000.
# Vì bài test của bạn chạy n=1000, ta cần nới limit lên để tránh RecursionError.
sys.setrecursionlimit(2000)

def add_recursive(n, acc):
    # Tương đương: LOAD_INT 2, 1 -> LT 2, 0, 2
    if n < 1:
        # Tương đương: stop: RETURN 1 (Trả về R1 là acc)
        return acc
    
    # Tương đương:
    # SUB 2, 0, 2  (n - 1)
    # ADD 3, 1, 0  (acc + n)
    # CALL ...     (Gọi đệ quy với tham số mới)
    return add_recursive(n - 1, acc + n)

def main():
    # Setup tham số giống hệt @main trong .meow
    n_input = 1000
    acc_input = 0
    
    # Số lần lặp để đo cho chính xác (VM của bạn nếu chạy qua C++ harness chắc cũng loop?)
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