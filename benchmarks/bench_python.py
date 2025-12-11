import time

def run_bench():
    limit = 10000000
    sum_val = 0
    counter = 0
    step = 1
    
    # Logic y hệt VM của cậu
    while counter < limit:
        sum_val = sum_val + step
        counter = counter + step
    
    return sum_val

print(f"Running Python benchmark (10M iterations)...")
start = time.time()
run_bench()
end = time.time()

print(f"Python Time: {(end - start) * 1000:.2f} ms")
