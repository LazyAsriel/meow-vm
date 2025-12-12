import os

# Bản đồ thay thế đường dẫn (Cũ -> Mới)
REPLACEMENTS = {
    # Public API
    '"vm/machine.h"': '<meow/machine.h>',
    '"core/value.h"': '<meow/value.h>',
    '"common/definitions.h"': '<meow/definitions.h>',
    '"common/config.h"': '<meow/config.h>',
    '"common/cast.h"': '<meow/cast.h>',  # Nếu bạn đã move cast.h ra public
    '"core/meow_object.h"': '<meow/core/meow_object.h>',
    '"core/objects.h"': '<meow/core/objects.h>',
    '"memory/memory_manager.h"': '<meow/memory/memory_manager.h>',
    
    # Internal Includes (Chuyển sang relative hoặc giữ nguyên nếu cùng folder)
    '"common/pch.h"': '"pch.h"', 
    
    # Core Objects (đã bị move vào meow/core/)
    '"core/objects/array.h"': '<meow/core/array.h>',
    '"core/objects/string.h"': '<meow/core/string.h>',
    '"core/objects/function.h"': '<meow/core/function.h>',
    '"core/objects/module.h"': '<meow/core/module.h>',
    '"core/objects/hash_table.h"': '<meow/core/hash_table.h>',
    '"core/objects/oop.h"': '<meow/core/oop.h>',
    '"core/objects/shape.h"': '<meow/core/shape.h>',
}

def process_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        new_content = content
        for old, new in REPLACEMENTS.items():
            new_content = new_content.replace(f'#include {old}', f'#include {new}')
        
        # Fix thêm namespace includes nếu cần
        if new_content != content:
            print(f"Fixed: {filepath}")
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(new_content)
    except Exception as e:
        print(f"Error reading {filepath}: {e}")

# Quét các thư mục
dirs = ['src', 'include', 'benchmarks', 'tests']
for d in dirs:
    for root, _, files in os.walk(d):
        for file in files:
            if file.endswith(('.h', '.cpp', '.hpp')):
                process_file(os.path.join(root, file))

print("✅ Đã fix xong các đường dẫn include!")
