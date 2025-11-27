#pragma once

#include <cstddef>

namespace meow {
struct MeowObject;
/**
 * @class GarbageCollector
 * @brief Dọn dẹp các object không còn được sử dụng, tránh memory leak
 */
class GarbageCollector {
   public:
    virtual ~GarbageCollector() noexcept = default;

    /**
     * @brief Đăng kí một object để GC quản lí
     * @param[in] object Object cần được GC quản li
     */
    virtual void register_object(const MeowObject* object) = 0;

    /**
     * @brief Dọn dẹp các object không còn dược sử dụng
     */
    virtual size_t collect() noexcept = 0;
};
}