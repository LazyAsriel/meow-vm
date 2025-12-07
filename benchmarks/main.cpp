#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <random>
#include <bit>
#include <iomanip>
#include <variant>
#include <algorithm>

// --- INCLUDES CỦA CẬU ---
#include "meow_variant.h"

using namespace meow;

// --- 1. RAW NANBOX (Thủ công) ---
struct RawNanBox {
    uint64_t _data;
    static constexpr uint64_t QNAN_MASK = 0x7FF8000000000000;
    static constexpr uint64_t TAG_INT   = 0x0001000000000000;
    static constexpr uint64_t SIG_INT   = QNAN_MASK | TAG_INT;

    // Demo Int & Double thôi cho gọn
    RawNanBox(double v)  { _data = std::bit_cast<uint64_t>(v); }
    RawNanBox(int64_t v) { _data = SIG_INT | (static_cast<uint32_t>(v)); } // Truncate to 32bit for simple nanbox

    inline bool is_int() const { return (_data & (QNAN_MASK | TAG_INT)) == SIG_INT; }
    inline bool is_double() const { return (_data & QNAN_MASK) != QNAN_MASK; }

    inline int64_t as_int() const { return static_cast<int32_t>(_data & 0xFFFFFFFF); }
    inline double as_double() const { return std::bit_cast<double>(_data); }
};

// --- ĐỊNH NGHĨA CÁC ĐẤU THỦ ---
// Lưu ý: int64_t trong nanbox thường bị giới hạn 48-52 bit, ở đây ta test hiệu năng truy cập là chính.
using StdVar      = std::variant<int64_t, double>;
using MeowFallback = meow::fallback_variant<int64_t, double>;
using MeowNanbox   = meow::nanboxed_variant<int64_t, double>;

// Helper cho std::visit
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// --- HÀM ĐO GIỜ ---
template <typename Func>
double measure(const char* name, Func func) {
    auto start = std::chrono::high_resolution_clock::now();
    volatile double result = func(); // Volatile để tránh compiler optimize mất loop
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    std::cout << "  👉 " << std::left << std::setw(30) << name << ": " 
              << "\033[1;32m" << std::fixed << std::setprecision(2) << ms << " ms\033[0m" 
              << " (Check: " << (long long)result << ")\n";
    return ms;
}

int main() {
    // Tăng size lên để phá vỡ L3 Cache (thường là vài chục MB)
    constexpr size_t N = 20'000'000; 
    
    std::cout << "\n🐱 === MEOW BATTLE ROYALE: THE CACHE WARS === 🐱\n";
    std::cout << "Data Size: " << N << " elements\n";
    
    // --- CHECK SIZEOF ---
    std::cout << "\n--- 📏 CÂN KÝ (SIZEOF) ---\n";
    std::cout << "Raw NanBox       : " << sizeof(RawNanBox) << " bytes\n";
    std::cout << "meow::nanboxed   : " << sizeof(MeowNanbox) << " bytes (Mèo Gầy)\n";
    std::cout << "meow::fallback   : " << sizeof(MeowFallback) << " bytes (Mèo Béo)\n";
    std::cout << "std::variant     : " << sizeof(StdVar) << " bytes\n";
    
    // --- PREPARE DATA ---
    std::cout << "\n🔄 Đang nạp đạn (Generating Data)... ";
    std::vector<int64_t> inputs; inputs.reserve(N);
    std::vector<int>     indices(N); // Mảng index để nhảy cóc
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> val_dist(0, 100);
    
    for(size_t i=0; i<N; ++i) {
        inputs.push_back(val_dist(rng));
        indices[i] = i;
    }
    
    // Xáo trộn index để tạo Random Access
    std::shuffle(indices.begin(), indices.end(), rng);

    // Tạo 4 vector riêng biệt
    std::vector<RawNanBox>    vec_raw; vec_raw.reserve(N);
    std::vector<StdVar>       vec_std; vec_std.reserve(N);
    std::vector<MeowFallback> vec_fb;  vec_fb.reserve(N);
    std::vector<MeowNanbox>   vec_nb;  vec_nb.reserve(N);

    for(auto v : inputs) {
        // Xen kẽ int và double để branch predictor không đoán mò được type
        if (v % 2 == 0) {
            vec_raw.emplace_back((int64_t)v);
            vec_std.emplace_back((int64_t)v);
            vec_fb.emplace_back((int64_t)v);
            vec_nb.emplace_back((int64_t)v);
        } else {
            double d = (double)v + 0.5;
            vec_raw.emplace_back(d);
            vec_std.emplace_back(d);
            vec_fb.emplace_back(d);
            vec_nb.emplace_back(d);
        }
    }
    std::cout << "Done! 🥊\n";

    // ==========================================================
    // ROUND 1: SEQUENTIAL ACCESS (Duyệt tuần tự)
    // ==========================================================
    std::cout << "\n--- 🏎️  ROUND 1: SEQUENTIAL ACCESS (Linear Scan) ---\n";
    std::cout << "Mục tiêu: Test tốc độ giải mã (Decode Overhead).\n";

    measure("Raw NanBox", [&]() {
        double sum = 0;
        for(const auto& v : vec_raw) {
            if(v.is_int()) sum += v.as_int(); else sum += v.as_double();
        }
        return sum;
    });

    measure("meow::nanboxed (Visit)", [&]() {
        double sum = 0;
        for(auto v : vec_nb) { // Pass by value cho Nanbox (8 bytes) là tối ưu
            v.visit([&](auto x) { sum += x; });
        }
        return sum;
    });

    measure("meow::fallback (Visit)", [&]() {
        double sum = 0;
        for(auto v : vec_fb) { // Pass by value (16 bytes)
            v.visit([&](auto x) { sum += x; });
        }
        return sum;
    });
    
    measure("std::variant (Visit)", [&]() {
        double sum = 0;
        for(const auto& v : vec_std) {
            std::visit([&](auto x) { sum += x; }, v);
        }
        return sum;
    });

    // ==========================================================
    // ROUND 2: RANDOM ACCESS (Truy cập ngẫu nhiên)
    // ==========================================================
    std::cout << "\n--- 🌪️  ROUND 2: RANDOM ACCESS (Cache Miss Hell) ---\n";
    std::cout << "Mục tiêu: Test Cache Locality (8 bytes vs 16 bytes).\n";
    std::cout << "Duyệt qua mảng indices đã bị xáo trộn...\n";

    double t_raw = measure("Raw NanBox", [&]() {
        double sum = 0;
        for(size_t idx : indices) {
            const auto& v = vec_raw[idx];
            if(v.is_int()) sum += v.as_int(); else sum += v.as_double();
        }
        return sum;
    });

    double t_nb = measure("meow::nanboxed (8 bytes)", [&]() {
        double sum = 0;
        for(size_t idx : indices) {
            vec_nb[idx].visit([&](auto x) { sum += x; });
        }
        return sum;
    });

    double t_fb = measure("meow::fallback (16 bytes)", [&]() {
        double sum = 0;
        for(size_t idx : indices) {
            vec_fb[idx].visit([&](auto x) { sum += x; });
        }
        return sum;
    });
    
    measure("std::variant (16 bytes)", [&]() {
        double sum = 0;
        for(size_t idx : indices) {
            std::visit([&](auto x) { sum += x; }, vec_std[idx]);
        }
        return sum;
    });

    std::cout << "\n--------------------------------------------------\n";
    std::cout << "📊 TỔNG KẾT ROUND 2:\n";
    double ratio = t_fb / t_nb;
    std::cout << "Mèo Gầy (Nanbox) vs Mèo Béo (Fallback): ";
    if (t_nb < t_fb) {
        std::cout << "\033[1;33m" << std::fixed << std::setprecision(2) << ratio << "x faster\033[0m ⚡\n";
        std::cout << "(Nanbox thắng nhờ nhét được nhiều item vào Cache hơn!)\n";
    } else {
        std::cout << "Hòa hoặc thua (Có thể do bộ nhớ chưa bị nghẽn).\n";
    }

    return 0;
}