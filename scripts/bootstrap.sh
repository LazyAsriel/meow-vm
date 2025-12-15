#!/usr/bin/env bash
set -euo pipefail

# 1. Cấu hình
MAX_STAGES="${1:-5}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

VM="$ROOT_DIR/build/release/bin/meow-vm"
MASM="$ROOT_DIR/build/release/bin/masm"
COMPILER_SRC="$ROOT_DIR/compiler/src/main.meow"
COMPILER_INCLUDE="$ROOT_DIR/compiler/src"

# Màu mè
GREEN="\e[92m"
CYAN="\e[96m"
YELLOW="\e[93m"
RED="\e[91m"
MAGENTA="\e[95m"
BLUE="\e[34m"
RESET="\e[0m"

# --- HELPER: SPINNER & TIMER ---
run_task() {
    local msg="$1"
    shift
    local cmd=("$@")
    
    echo -ne "   $msg... "
    
    local temp_log=$(mktemp)
    local start_ts=$(date +%s%N)
    
    # Chạy lệnh trong background
    "${cmd[@]}" > "$temp_log" 2>&1 &
    local pid=$!
    
    local delay=0.1
    local spin='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
    local i=0
    
    # Loop spinner
    while kill -0 "$pid" 2>/dev/null; do
        i=$(( (i+1) % ${#spin} ))
        printf "\b${MAGENTA}%s${RESET}" "${spin:$i:1}"
        sleep "$delay"
    done
    
    # Đợi lấy exit code
    wait "$pid"
    local exit_code=$?
    local end_ts=$(date +%s%N)
    local duration=$(( (end_ts - start_ts) / 1000000 )) # ms
    
    printf "\b"
    
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}Done${RESET} (${YELLOW}${duration}ms${RESET})"
        rm "$temp_log"
        return 0
    else
        echo -e "${RED}Failed${RESET} (${duration}ms)"
        echo -e "${RED}=== ERROR LOG ===${RESET}"
        cat "$temp_log"
        echo -e "${RED}=================${RESET}"
        rm "$temp_log"
        return 1
    fi
}

# Hàm assemble logic
assemble_files() {
    local src_dir="$1"
    local dist_dir="$2"
    
    if [ ! -d "$src_dir" ]; then
        echo "Error: Source dir not found: $src_dir"
        return 1
    fi

    mkdir -p "$dist_dir"
    local count=0

    # Tìm file source (.meowc text hoặc .meowb cũ)
    while IFS= read -r -d $'\0' file; do
        local rel_path="${file#$src_dir/}"
        local target_dir="$dist_dir/$(dirname "$rel_path")"
        mkdir -p "$target_dir"
        
        local filename=$(basename "$file")
        local name_no_ext="${filename%.*}"
        local temp_asm="$target_dir/$name_no_ext.temp.asm"
        local out_bin="$target_dir/$name_no_ext.meowc"

        sed 's/\.meowb"/\.meowc"/g' "$file" > "$temp_asm"

        # Gọi MASM binary
        if ! "$MASM" "$temp_asm" "$out_bin"; then
            echo "Failed to assemble: $filename"
            exit 1
        fi
        rm "$temp_asm"
        count=$((count+1))
    done < <(find "$src_dir" -type f \( -name "*.meowc" -o -name "*.meowb" \) -print0)
    
    if [ "$count" -eq 0 ]; then 
        echo "No assembly files found in $src_dir"
        exit 1
    fi
}

# ==============================================================================
# MAIN FLOW
# ==============================================================================

echo -e "${MAGENTA}╔══════════════════════════════════════════════════╗${RESET}"
echo -e "${MAGENTA}║ 🚀 MEOW BOOTSTRAPPER (Direct Mode)               ║${RESET}"
echo -e "${MAGENTA}╚══════════════════════════════════════════════════╝${RESET}"

# Check Binary
if [[ ! -x "$VM" || ! -x "$MASM" ]]; then
    echo -e "${RED}❌ Thiếu file thực thi. Build C++ trước!${RESET}"
    exit 1
fi

CURRENT_COMPILER="$ROOT_DIR/dist/main.meowc"

# --- STAGE 0: RECOVERY ---
if [ ! -f "$CURRENT_COMPILER" ]; then
    echo -e "\n${CYAN}🌱 [STAGE 0]${RESET} Recovering from Source..."
    
    RAW_STAGE0="$ROOT_DIR/compiler/builds/build-stage0"
    
    # 1. Gen ASM
    build_stage0_asm() {
        cd "$ROOT_DIR/compiler" && ./scripts/build.sh "0" && cd "$ROOT_DIR"
    }
    run_task "Generating ASM" build_stage0_asm

    # 2. Assemble Stage 0
    task_assemble_0() { assemble_files "$RAW_STAGE0" "$ROOT_DIR/dist"; }
    run_task "Assembling Stage 0" task_assemble_0
    
    if [ ! -f "$CURRENT_COMPILER" ]; then
        echo -e "${RED}❌ Stage 0 failed (No binary created).${RESET}"
        exit 1
    fi
fi

# --- STAGE 1..N: SELF-HOSTING ---
FIXED_POINT_FOUND=0

for ((i=1; i<=MAX_STAGES; i++)); do
    echo -e "\n${BLUE}🚀 [STAGE $i]${RESET} Self-Hosting..."
    
    BUILD_DIR="$ROOT_DIR/builds/bootstrap/stage$i"
    DIST_DIR="$ROOT_DIR/dist/bootstrap/stage$i"
    
    rm -rf "$BUILD_DIR"
    rm -rf "$DIST_DIR"
    
    # 1. Compile
    run_task "Compiling (VM)" \
        "$VM" -b "$CURRENT_COMPILER" "$COMPILER_SRC" \
        --include "$COMPILER_INCLUDE" \
        --buildDir "$BUILD_DIR" \
        --nocache

    # 2. Assemble
    task_assemble_current() { assemble_files "$BUILD_DIR" "$DIST_DIR"; }
    
    run_task "Assembling (MASM)" task_assemble_current

    NEW_BINARY="$DIST_DIR/main.meowc"

    # 3. Verify & Check Fixed Point
    if [ ! -f "$NEW_BINARY" ]; then
        echo -e "   ${RED}❌ Output binary missing!${RESET}"
        exit 1
    fi

    HASH_OLD=$(md5sum "$CURRENT_COMPILER" | awk '{print $1}')
    HASH_NEW=$(md5sum "$NEW_BINARY" | awk '{print $1}')

    if [ "$HASH_OLD" == "$HASH_NEW" ]; then
        if [ $FIXED_POINT_FOUND -eq 0 ]; then
            echo -e "   ✨ ${GREEN}Fixed Point Reached!${RESET} (Compiler is stable)"
            echo -e "   📦 Updating Seed: ${YELLOW}$NEW_BINARY${RESET}"
            cp -r "$DIST_DIR/"* "$ROOT_DIR/dist/"
            FIXED_POINT_FOUND=1
        else
            echo -e "   💤 ${CYAN}Stable${RESET} (Identical binary)"
        fi
    else
        echo -e "   ⚡ ${YELLOW}Changed${RESET} (Compiler evolving)"
        CURRENT_COMPILER="$NEW_BINARY"
    fi
done

echo -e "\n${MAGENTA}🏁 DONE!${RESET}"