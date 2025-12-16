#!/usr/bin/env bash
set -euo pipefail

# 1. Cấu hình
MAX_STAGES="${1:-5}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

VM="$ROOT_DIR/build/release/bin/meow-vm"
MASM="$ROOT_DIR/build/release/bin/masm"
COMPILER_SRC="$ROOT_DIR/compiler/src/main.meow"
COMPILER_INCLUDE="$ROOT_DIR/compiler/src"

# Màu mè hoa lá cành
BOLD="\e[1m"
DIM="\e[2m"
GREEN="\e[32m"
CYAN="\e[36m"
YELLOW="\e[33m"
RED="\e[31m"
MAGENTA="\e[35m"
BLUE="\e[34m"
RESET="\e[0m"

# Mảng lưu thời gian để in báo cáo cuối
declare -a STAGE_TIMES

# --- HELPER: SPINNER & TIMER (Đã nâng cấp) ---
run_task() {
    local label="$1"
    shift
    local cmd=("$@")
    
    # Canh lề trái 35 ký tự cho label để thẳng tắp
    printf "   ${DIM}│${RESET} %-35s " "$label..."
    
    local temp_log=$(mktemp)
    local start_ts=$(date +%s%N)
    
    # Chạy lệnh background
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
    
    # Xóa spinner character
    printf "\b"
    
    if [ $exit_code -eq 0 ]; then
        # Canh lề phải cho thời gian (độ rộng 6 ký tự)
        printf "${GREEN}✅ Done${RESET} ${DIM}(%6dms)${RESET}\n" "$duration"
        rm "$temp_log"
        return 0
    else
        printf "${RED}❌ Fail${RESET} ${DIM}(%6dms)${RESET}\n" "$duration"
        echo -e "\n${RED}╔════ ERROR LOG ════════════════════════════════════╗${RESET}"
        cat "$temp_log"
        echo -e "${RED}╚═══════════════════════════════════════════════════╝${RESET}"
        rm "$temp_log"
        return 1
    fi
}

assemble_files() {
    local src_dir="$1"
    local dist_dir="$2"
    
    if [ ! -d "$src_dir" ]; then
        echo "Error: Source dir not found: $src_dir"
        return 1
    fi

    mkdir -p "$dist_dir"
    local count=0

    while IFS= read -r -d $'\0' file; do
        local rel_path="${file#$src_dir/}"
        local target_dir="$dist_dir/$(dirname "$rel_path")"
        mkdir -p "$target_dir"
        
        local filename=$(basename "$file")
        local name_no_ext="${filename%.*}"
        local temp_asm="$target_dir/$name_no_ext.temp.asm"
        local out_bin="$target_dir/$name_no_ext.meowc"

        sed 's/\.meowb"/\.meowc"/g' "$file" > "$temp_asm"

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

print_header() {
    clear
    echo -e "${MAGENTA}   __  __                  __     ____  __ ${RESET}"
    echo -e "${MAGENTA}  |  \/  | ___  _____      _\ \   / /  \/  |${RESET}"
    echo -e "${MAGENTA}  | |\/| |/ _ \/ _ \ \ /\ / /\ \ / /| |\/| |${RESET}"
    echo -e "${MAGENTA}  | |  | |  __/ (_) \ V  V /  \ V / | |  | |${RESET}"
    echo -e "${MAGENTA}  |_|  |_|\___|\___/ \_/\_/    \_/  |_|  |_|${RESET}"
    echo -e "  ${CYAN}🚀 BOOTSTRAPPER sequence initiated...${RESET}\n"
}

# ==============================================================================
# MAIN FLOW
# ==============================================================================

print_header

if [[ ! -x "$VM" || ! -x "$MASM" ]]; then
    echo -e "${RED}❌ Thiếu file thực thi. Build C++ trước đê!${RESET}"
    exit 1
fi

CURRENT_COMPILER="$ROOT_DIR/dist/main.meowc"
TOTAL_START=$(date +%s%N)

# --- STAGE 0: RECOVERY ---
if [ ! -f "$CURRENT_COMPILER" ]; then
    echo -e "${BOLD}🌱 STAGE 0: Recovery${RESET}"
    
    RAW_STAGE0="$ROOT_DIR/compiler/builds/build-stage0"
    
    build_stage0_asm() {
        cd "$ROOT_DIR/compiler" && ./scripts/build.sh "0" >/dev/null 2>&1 && cd "$ROOT_DIR"
    }
    run_task "Generating ASM from Source" build_stage0_asm

    task_assemble_0() { assemble_files "$RAW_STAGE0" "$ROOT_DIR/dist"; }
    run_task "Assembling Stage 0" task_assemble_0
    
    if [ ! -f "$CURRENT_COMPILER" ]; then
        echo -e "${RED}❌ Stage 0 toang rồi (No binary).${RESET}"
        exit 1
    fi
    echo ""
fi

# --- STAGE 1..N: SELF-HOSTING ---
FIXED_POINT_FOUND=0

for ((i=1; i<=MAX_STAGES; i++)); do
    STAGE_START=$(date +%s%N)
    
    # Header của Stage
    if [ $FIXED_POINT_FOUND -eq 1 ]; then
        echo -e "${DIM}💤 STAGE $i: Verifying Stability...${RESET}"
    else
        echo -e "${BLUE}${BOLD}🚀 STAGE $i: Self-Hosting${RESET}"
    fi
    
    BUILD_DIR="$ROOT_DIR/builds/bootstrap/stage$i"
    DIST_DIR="$ROOT_DIR/dist/bootstrap/stage$i"
    
    rm -rf "$BUILD_DIR"
    rm -rf "$DIST_DIR"
    
    # 1. Compile
    run_task "Compiling (Meow -> ASM)" \
        "$VM" -b "$CURRENT_COMPILER" "$COMPILER_SRC" \
        --include "$COMPILER_INCLUDE" \
        --buildDir "$BUILD_DIR" \
        --nocache

    # 2. Assemble
    task_assemble_current() { assemble_files "$BUILD_DIR" "$DIST_DIR"; }
    
    run_task "Assembling (ASM -> Bytecode)" task_assemble_current

    NEW_BINARY="$DIST_DIR/main.meowc"

    # 3. Verify
    if [ ! -f "$NEW_BINARY" ]; then
        echo -e "   ${RED}❌ Output binary missing!${RESET}"
        exit 1
    fi

    HASH_OLD=$(md5sum "$CURRENT_COMPILER" | awk '{print $1}')
    HASH_NEW=$(md5sum "$NEW_BINARY" | awk '{print $1}')
    
    STAGE_END=$(date +%s%N)
    STAGE_DURATION=$(( (STAGE_END - STAGE_START) / 1000000 ))
    STAGE_TIMES+=("$STAGE_DURATION")

    if [ "$HASH_OLD" == "$HASH_NEW" ]; then
        if [ $FIXED_POINT_FOUND -eq 0 ]; then
            echo -e "   ✨ ${GREEN}Fixed Point Reached!${RESET} (Compiler is stable)"
            echo -e "   📦 Updating Seed: ${YELLOW}$(basename $NEW_BINARY)${RESET}"
            cp -r "$DIST_DIR/"* "$ROOT_DIR/dist/"
            FIXED_POINT_FOUND=1
        else
            # In ngắn gọn hơn khi đã stable
            # echo -e "   💤 Identical Binary"
            :
        fi
    else
        echo -e "   ⚡ ${YELLOW}Binary Changed${RESET} (Compiler evolving)"
        CURRENT_COMPILER="$NEW_BINARY"
    fi
    echo "" # Dòng trống giữa các stage cho thoáng
done

# --- SUMMARY ---
TOTAL_END=$(date +%s%N)
TOTAL_DURATION=$(( (TOTAL_END - TOTAL_START) / 1000000 ))

echo -e "${MAGENTA}╔════════════════════════════════════════════╗${RESET}"
echo -e "${MAGENTA}║             BOOTSTRAP SUMMARY              ║${RESET}"
echo -e "${MAGENTA}╠════════════════════════════════════════════╣${RESET}"
for ((j=0; j<${#STAGE_TIMES[@]}; j++)); do
    idx=$((j+1))
    time=${STAGE_TIMES[$j]}
    printf "${MAGENTA}║${RESET} Stage %-2d : %6dms                       ${MAGENTA}║${RESET}\n" "$idx" "$time"
done
echo -e "${MAGENTA}╠════════════════════════════════════════════╣${RESET}"
printf "${MAGENTA}║${RESET} ${BOLD}TOTAL    : %6dms${RESET}                       ${MAGENTA}║${RESET}\n" "$TOTAL_DURATION"
echo -e "${MAGENTA}╚════════════════════════════════════════════╝${RESET}"
echo -e "\n🏁 Ready to purr!"