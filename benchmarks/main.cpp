#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <random>
#include <bit>
#include <iomanip>

#ifdef _WIN32
#include <windows.h> // Cần thiết để set UTF-8 trên Windows
#endif

// Include headers của cậu (Giữ nguyên theo yêu cầu)
#include "core/value.h" 

using namespace meow;

struct NanBoxValue {
    uint64_t _data;
    static constexpr uint64_t QNAN_MASK    = 0x7FF8000000000000;
    static constexpr uint64_t TAG_INT      = 0x0001000000000000;
    static constexpr uint64_t TAG_BOOL     = 0x0002000000000000;
    static constexpr uint64_t SIG_INT      = QNAN_MASK | TAG_INT;
    static constexpr uint64_t SIG_BOOL     = QNAN_MASK | TAG_BOOL;

    NanBoxValue() : _data(QNAN_MASK) {}
    NanBoxValue(double val) { _data = std::bit_cast<uint64_t>(val); }
    NanBoxValue(int64_t val) { _data = SIG_INT | (static_cast<uint32_t>(val)); }
    NanBoxValue(bool val) { _data = SIG_BOOL | (val ? 1 : 0); }

    inline bool is_double() const { return (_data & QNAN_MASK) != QNAN_MASK; }
    inline bool is_int() const { return (_data & (QNAN_MASK | TAG_INT)) == SIG_INT; }
    inline bool is_bool() const { return (_data & (QNAN_MASK | TAG_BOOL)) == SIG_BOOL; }

    inline double as_double() const { return std::bit_cast<double>(_data); }
    inline int64_t as_int() const { return static_cast<int32_t>(_data & 0xFFFFFFFF); }
    inline bool as_bool() const { return (_data & 0x1) == 1; }
};

template <typename Func>
double measure(const char* name, Func func) {
    auto start = std::chrono::high_resolution_clock::now();
    auto result = func(); // Lấy kết quả để tránh compiler optimize mất vòng lặp
    auto end = std::chrono::high_resolution_clock::now();
    
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << std::left << std::setw(30) << name << ": " 
              << std::fixed << std::setprecision(2) << ms << " ms (Sum: " << result << ")\n";
    return ms;
}

int main() {
    // Bật Tiếng Việt trên Windows Console
    #ifdef _WIN32
    SetConsoleOutputCP(65001);
    #endif

    constexpr size_t N = 10'000'000; 
    std::cout << "=== MEOW ULTIMATE BENCHMARK (N = " << N << ") ===\n\n";
    
    // --- PHẦN BỔ SUNG: ĐO SIZEOF ---
    std::cout << "--- KIỂM TRA KÍCH THƯỚC BỘ NHỚ (SIZEOF) ---\n";
    std::cout << "sizeof(meow::Value) : " << sizeof(meow::Value) << " bytes\n";
    std::cout << "sizeof(NanBoxValue) : " << sizeof(NanBoxValue) << " bytes\n";
    std::cout << "-------------------------------------------\n\n";

    std::cout << "Đang tạo dữ liệu hỗn loạn (Generating chaos data)..." << std::endl;
    std::vector<int> types;     
    std::vector<double> values; 
    types.reserve(N);
    values.reserve(N);
    
    std::mt19937 rng(42); 
    std::uniform_int_distribution<int> type_dist(0, 2);
    std::uniform_real_distribution<double> val_dist(0.0, 100.0);

    for(size_t i = 0; i < N; ++i) {
        types.push_back(type_dist(rng));
        values.push_back(val_dist(rng));
    }

    std::vector<meow::Value> vec_variant;
    std::vector<NanBoxValue> vec_nanbox;
    vec_variant.reserve(N);
    vec_nanbox.reserve(N);

    for(size_t i = 0; i < N; ++i) {
        if (types[i] == 0) { // Int
            int64_t v = static_cast<int64_t>(values[i]);
            vec_variant.emplace_back(static_cast<meow::int_t>(v));
            vec_nanbox.emplace_back(v);
        } else if (types[i] == 1) { // Double
            double v = values[i];
            vec_variant.emplace_back(v);
            vec_nanbox.emplace_back(v);
        } else { // Bool
            bool v = values[i] > 50.0;
            vec_variant.emplace_back(v);
            vec_nanbox.emplace_back(v);
        }
    }
    std::cout << "Dữ liệu đã sẵn sàng. Chiến thôi! 🥊\n\n";

    double t_variant_visit = measure("meow::Value (Visit)", [&]() -> double {
        double sum = 0;

        for (const auto& v : vec_variant) {
            v.visit(
                [&sum](meow::int_t v)   { sum += v; },
                [&sum](meow::float_t v) { sum += v; },
                [&sum](meow::bool_t v)  { sum += (v ? 1.0 : 0.0); },
                [](auto) {
                    std::unreachable();
                }
            );
        }
        return sum;
    });

    double t_variant_if = measure("meow::Value (If-Else)", [&]() -> double {
        double sum = 0;
        for(const auto& v : vec_variant) {
            if (v.is_int()) sum += v.as_int();
            else if (v.is_float()) sum += v.as_float(); // Giả định float_t = double
            else if (v.is_bool()) sum += (v.as_bool() ? 1.0 : 0.0);
        }
        return sum;
    });

    double t_nanbox = measure("Raw NanBox (Bitwise)", [&]() -> double {
        double sum = 0;
        for(const auto& v : vec_nanbox) {
            if (v.is_int()) sum += v.as_int();
            else if (v.is_double()) sum += v.as_double();
            else if (v.is_bool()) sum += (v.as_bool() ? 1.0 : 0.0);
        }
        return sum;
    });

    std::cout << "\n--------------------------------------------------\n";
    std::cout << "KẾT QUẢ PHÂN TÍCH:\n";
    
    double ratio = t_variant_visit / t_nanbox;
    std::cout << "Visit vs Raw: " << std::fixed << std::setprecision(2) << ratio << "x ";
    if (ratio <= 1.1) std::cout << "(Ngang ngửa! Value quá xịn! 🚀)\n";
    else std::cout << "(Raw vẫn nhanh hơn chút, nhưng Value an toàn hơn)\n";

    double if_penalty = ((t_variant_if - t_variant_visit) / t_variant_visit) * 100.0;
    std::cout << "Visit nhanh hơn If-Else: " << std::fixed << std::setprecision(1) << if_penalty << "% ";
    if (if_penalty > 0) std::cout << "📉 (Dùng Visit tối ưu hơn)\n";
    else std::cout << "📈 (Compiler optimize If-Else quá tốt)\n";

    return 0;
}