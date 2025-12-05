#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <random>
#include <bit>
#include <iomanip>
#include <variant>

// --- INCLUDES ---
#include "core/value.h"      // NanBoxed Value (8 bytes)
#include "meow_variant.h"    // <--- TÂN BINH MỚI (Generic Variant)

using namespace meow;

// --- NANBOX RAW (Dùng làm mốc so sánh) ---
struct NanBoxValue {
    uint64_t _data;
    static constexpr uint64_t QNAN_MASK = 0x7FF8000000000000;
    static constexpr uint64_t TAG_INT   = 0x0001000000000000;
    static constexpr uint64_t TAG_BOOL  = 0x0002000000000000;
    static constexpr uint64_t SIG_INT   = QNAN_MASK | TAG_INT;
    static constexpr uint64_t SIG_BOOL  = QNAN_MASK | TAG_BOOL;

    NanBoxValue(double v)  { _data = std::bit_cast<uint64_t>(v); }
    NanBoxValue(int64_t v) { _data = SIG_INT | (static_cast<uint32_t>(v)); }
    NanBoxValue(bool v)    { _data = SIG_BOOL | (v ? 1 : 0); }

    inline bool is_int() const { return (_data & (QNAN_MASK | TAG_INT)) == SIG_INT; }
    inline bool is_double() const { return (_data & QNAN_MASK) != QNAN_MASK; }
    inline bool is_bool() const { return (_data & (QNAN_MASK | TAG_BOOL)) == SIG_BOOL; }

    inline int64_t as_int() const { return static_cast<int32_t>(_data & 0xFFFFFFFF); }
    inline double as_double() const { return std::bit_cast<double>(_data); }
    inline bool as_bool() const { return (_data & 0x1); }
};

// --- DEFINITIONS ---
using StdVariant  = std::variant<int64_t, double, bool>;
using MeowVariant = meow::variant<int64_t, double, bool>; // Định nghĩa kiểu variant của cậu

// Helper cho std::visit
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// --- MEASURE FUNCTION ---
template <typename Func>
double measure(const char* name, Func func) {
    auto start = std::chrono::high_resolution_clock::now();
    auto result = func(); 
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    // In màu mè tí cho dễ nhìn
    std::cout << "  👉 " << std::left << std::setw(30) << name << ": " 
              << "\033[1;32m" << std::fixed << std::setprecision(2) << ms << " ms\033[0m" 
              << " (Sum: " << (long long)result << ")\n";
    return ms;
}

int main() {
    constexpr size_t N = 10'000'000; 
    std::cout << "\n🐱 === MEOW BATTLE ROYALE: VARIANT WAR === 🐱\n";
    std::cout << "Data Size: " << N << " elements\n\n";
    
    // --- 1. ROUND 1: CÂN KÝ (SIZEOF) ---
    std::cout << "--- 📏 ROUND 1: SIZE MATTERS ---\n";
    std::cout << "sizeof(NanBoxValue)  : " << sizeof(NanBoxValue) << " bytes (Raw Pointer)\n";
    std::cout << "sizeof(meow::Value)  : " << sizeof(meow::Value) << " bytes (Optimized VM Value)\n";
    std::cout << "sizeof(meow::variant): " << sizeof(MeowVariant) << " bytes (Generic Custom)\n";
    std::cout << "sizeof(std::variant) : " << sizeof(StdVariant) << " bytes (Standard Lib)\n";
    std::cout << "-------------------------------------------\n\n";

    // --- SETUP DATA ---
    std::cout << "🔄 Loading ammo (Generating data)... ";
    std::vector<int> types;     
    std::vector<double> values; 
    types.reserve(N); values.reserve(N);
    std::mt19937 rng(42); 
    std::uniform_int_distribution<int> type_dist(0, 2);
    std::uniform_real_distribution<double> val_dist(0.0, 100.0);
    for(size_t i=0; i<N; ++i) { types.push_back(type_dist(rng)); values.push_back(val_dist(rng)); }

    std::vector<NanBoxValue> vec_nan;
    std::vector<meow::Value> vec_val;
    std::vector<StdVariant>  vec_std;
    std::vector<MeowVariant> vec_cst; // Custom variant vector

    vec_nan.reserve(N); vec_val.reserve(N); vec_std.reserve(N); vec_cst.reserve(N);

    for(size_t i = 0; i < N; ++i) {
        if (types[i] == 0) { // Int
            int64_t v = (int64_t)values[i];
            vec_nan.emplace_back(v);
            vec_val.emplace_back((meow::int_t)v);
            vec_std.emplace_back(v);
            vec_cst.emplace_back(v);
        } else if (types[i] == 1) { // Double
            double v = values[i];
            vec_nan.emplace_back(v);
            vec_val.emplace_back(v);
            vec_std.emplace_back(v);
            vec_cst.emplace_back(v);
        } else { // Bool
            bool v = values[i] > 50.0;
            vec_nan.emplace_back(v);
            vec_val.emplace_back(v);
            vec_std.emplace_back(v);
            vec_cst.emplace_back(v);
        }
    }
    std::cout << "Done! 🥊\n\n";

    // --- 2. ROUND 2: SPEED RUN ---
    std::cout << "--- 🚀 ROUND 2: FIGHT! ---\n";

    // 1. RAW NANBOX
    double t_nan = measure("Raw NanBox (Baseline)", [&]() -> double {
        double sum = 0;
        for(const auto& v : vec_nan) {
            if (v.is_int()) sum += v.as_int();
            else if (v.is_double()) sum += v.as_double();
            else sum += (v.as_bool() ? 1.0 : 0.0);
        }
        return sum;
    });

    // 2. MEOW::VALUE (NanBoxed Class)
    double t_val = measure("meow::Value (Visit)", [&]() -> double {
        double sum = 0;
        for (auto v : vec_val) {
            v.visit(
                [&sum](meow::int_t x)   { sum += x; },
                [&sum](meow::float_t x) { sum += x; },
                [&sum](meow::bool_t x)  { sum += (x ? 1.0 : 0.0); },
                [](auto) { std::unreachable(); }
            );
        }
        return sum;
    });

    // 3. MEOW::VARIANT (Custom Generic)
    double t_cst = measure("meow::variant (Visit)", [&]() -> double {
        double sum = 0;
        for (auto v : vec_cst) {
            v.visit(
                [&sum](int64_t x) { sum += x; },
                [&sum](double x)  { sum += x; },
                [&sum](bool x)    { sum += (x ? 1.0 : 0.0); }
            );
        }
        return sum;
    });

    // 4. STD::VARIANT
    double t_std = measure("std::variant (std::visit)", [&]() -> double {
        double sum = 0;
        for (const auto& v : vec_std) {
            std::visit(overloaded {
                [&sum](int64_t x) { sum += x; },
                [&sum](double x)  { sum += x; },
                [&sum](bool x)    { sum += (x ? 1.0 : 0.0); }
            }, v);
        }
        return sum;
    });

    // --- ANALYTICS ---
    std::cout << "\n--------------------------------------------------\n";
    std::cout << "📊 TỔNG KẾT:\n";
    
    auto compare = [](const char* name, double base, double target) {
        double r = target / base;
        std::cout << name << ": " << "\033[1;33m" << std::fixed << std::setprecision(2) << r << "x \033[0m";
        if (r < 1.0) std::cout << "⚡ (Nhanh hơn!)\n";
        else std::cout << "🐢 (Chậm hơn)\n";
    };

    compare("meow::variant vs std::variant", t_std, t_cst);
    compare("meow::variant vs meow::Value", t_val, t_cst);
    compare("meow::variant vs Raw NanBox", t_nan, t_cst);

    return 0;
}


// #include <iostream>
// #include <vector>
// #include <chrono>
// #include <numeric>
// #include <iomanip>
// #include <variant>
// #include <bit>

// // --- INCLUDES ---
// #include "core/value.h"      
// #include "meow_variant.h"    

// using namespace meow;

// // --- NANBOX RAW (Như cũ) ---
// struct NanBoxValue {
//     uint64_t _data;
//     static constexpr uint64_t TAG_INT = 0x0001000000000000;
//     static constexpr uint64_t SIG_INT = 0x7FF8000000000000 | TAG_INT;

//     // Lưu ý: NanBox này chỉ chứa được 32-bit int trong 48-bit payload
//     // Nhưng để test tốc độ decode thì vẫn ok.
//     NanBoxValue(int64_t v) { _data = SIG_INT | (static_cast<uint32_t>(v)); }
    
//     inline bool is_int() const { return (_data & 0xFFFF000000000000) == SIG_INT; }
//     inline int64_t as_int() const { return static_cast<int32_t>(_data & 0xFFFFFFFF); }
// };

// using StdVariant = std::variant<int64_t, double, bool>;
// using MeowVariant = meow::variant<int64_t, double, bool>;

// // Helper
// template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// template <typename Func>
// double measure(const char* name, Func func) {
//     auto start = std::chrono::high_resolution_clock::now();
//     auto result = func(); 
//     auto end = std::chrono::high_resolution_clock::now();
//     double ms = std::chrono::duration<double, std::milli>(end - start).count();
    
//     std::cout << "  👉 " << std::left << std::setw(30) << name << ": " 
//               << "\033[1;32m" << std::fixed << std::setprecision(2) << ms << " ms\033[0m" 
//               << " (Sum: " << (long long)result << ")\n";
//     return ms;
// }

// int main() {
//     constexpr size_t N = 10'000'000; 
//     std::cout << "\n🐱 === MONO TYPE BENCHMARK (ONLY INT64) === 🐱\n";
//     std::cout << "Data Size: " << N << " integers\n\n";

//     // --- SETUP DATA (Toàn là INT) ---
//     std::vector<int64_t> vec_native;
//     std::vector<NanBoxValue> vec_nan;
//     std::vector<meow::Value> vec_val;
//     std::vector<MeowVariant> vec_cst; 
//     std::vector<StdVariant>  vec_std;

//     vec_native.reserve(N);
//     vec_nan.reserve(N);
//     vec_val.reserve(N);
//     vec_cst.reserve(N);
//     vec_std.reserve(N);

//     std::cout << "🔄 Generatng 10M Integers... ";
//     for(size_t i = 0; i < N; ++i) {
//         int64_t v = i % 1000; // Giá trị nhỏ để tránh overflow sum
//         vec_native.push_back(v);
//         vec_nan.emplace_back(v);
//         vec_val.emplace_back((meow::int_t)v);
//         vec_cst.emplace_back(v);
//         vec_std.emplace_back(v);
//     }
//     std::cout << "Done!\n\n";

//     std::cout << "--- 🚀 START RACING ---\n";

//     // 0. BASELINE (Tốc độ ánh sáng)
//     double t_native = measure("Native vector<int64>", [&]() -> double {
//         double sum = 0;
//         for (auto v : vec_native) sum += v;
//         return sum;
//     });

//     // 1. RAW NANBOX (Direct Access - Không check type)
//     // Giả lập trường hợp VM đã biết chắc đây là Int (thông qua phân tích bytecode)
//     double t_nan_raw = measure("NanBox (Direct Access)", [&]() -> double {
//         double sum = 0;
//         for(const auto& v : vec_nan) {
//             sum += v.as_int(); // Chỉ tốn công bitwise AND
//         }
//         return sum;
//     });

//     // 2. MEOW::VARIANT (Visit)
//     double t_cst = measure("meow::variant (Visit)", [&]() -> double {
//         double sum = 0;
//         for (auto v : vec_cst) sum += v.get<int64_t>();
//         return sum;
//     });

//     // 3. MEOW::VALUE (Visit)
//     double t_val = measure("meow::Value (Visit)", [&]() -> double {
//         double sum = 0;
//         for (auto v : vec_val) sum += v.as_int();
//         return sum;
//     });

//     // 4. STD::VARIANT
//     double t_std = measure("std::variant (Visit)", [&]() -> double {
//         double sum = 0;
//         for (const auto& v : vec_std) sum += std::get<int64_t>(v);
//         return sum;
//     });

//     // --- PHÂN TÍCH ---
//     std::cout << "\n--------------------------------------------------\n";
//     std::cout << "📊 OVERHEAD ANALYSIS (So với Native C++):\n";
    
//     auto overhead = [&](const char* name, double t) {
//         double extra = t - t_native;
//         double ratio = t / t_native;
//         std::cout << name << ": " 
//                   << std::fixed << std::setprecision(1) << ratio << "x slower "
//                   << "(Overhead: " << extra << " ms)\n";
//     };

//     overhead("Raw NanBox", t_nan_raw);
//     overhead("meow::variant", t_cst);
//     overhead("std::variant", t_std);

//     return 0;
// }