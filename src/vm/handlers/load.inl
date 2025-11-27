#pragma once

inline void Machine::op_load_const(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    Value value = READ_CONSTANT();
    REGISTER(dst) = value;
}

inline void Machine::op_load_null(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    REGISTER(dst) = Value(null_t{});
    printl("load_null r{}", dst);
}

inline void Machine::op_load_true(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    REGISTER(dst) = Value(true);
    printl("load_true r{}", dst);
}

inline void Machine::op_load_false(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    REGISTER(dst) = Value(false);
    printl("load_false r{}", dst);
}

inline void Machine::op_move(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    uint16_t src = READ_U16();
    REGISTER(dst) = REGISTER(src);
}

inline void Machine::op_load_int(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    int64_t value = READ_I64();
    REGISTER(dst) = Value(value);
    printl("load_int r{}, {}", dst, value);
}

inline void Machine::op_load_float(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    double value = READ_F64();
    REGISTER(dst) = Value(value);
    printl("load_float r{}, {}", dst, value);
}