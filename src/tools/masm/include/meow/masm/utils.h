#pragma once
#include "common.h"
#include <string>
#include <string_view>

namespace meow::masm {

constexpr std::string_view get_error_msg(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK: return "No error";
        case ErrorCode::UNEXPECTED_TOKEN: return "Unexpected token";
        case ErrorCode::EXPECTED_FUNC_NAME: return "Expected function name";
        case ErrorCode::EXPECTED_NUMBER: return "Expected number";
        case ErrorCode::EXPECTED_U16: return "Expected u16 value (0-65535)";
        case ErrorCode::EXPECTED_INT64: return "Expected int64 value";
        case ErrorCode::EXPECTED_DOUBLE: return "Expected double value";
        case ErrorCode::EXPECTED_TYPE: return "Expected upvalue type ('local' or 'upvalue')";
        case ErrorCode::EXPECTED_SLOT: return "Expected register slot or index";
        case ErrorCode::EXPECTED_STRING: return "Expected string literal";
        case ErrorCode::UNKNOWN_OPCODE: return "Unknown opcode";
        case ErrorCode::UNKNOWN_ANNOTATION: return "Unknown annotation";
        case ErrorCode::UNKNOWN_CONSTANT: return "Unknown constant";
        case ErrorCode::UNDEFINED_LABEL: return "Undefined label target";
        case ErrorCode::UNDEFINED_PROTO_REF: return "Undefined prototype reference";
        case ErrorCode::OUTSIDE_FUNC: return "Instruction/Directive must be inside .func";
        case ErrorCode::LABEL_REDEFINITION: return "Label redefinition";
        case ErrorCode::FILE_OPEN_FAILED: return "Cannot open input file";
        case ErrorCode::WRITE_ERROR: return "Cannot write output file";
        case ErrorCode::INDEX_OUT_OF_BOUNDS: return "Index out of bounds";
        case ErrorCode::READ_ERROR: return "File read error";
        default: return "Unknown error";
    }
}

void report_error(const Status& s, std::string_view stage);
[[nodiscard]] Status compile_file(const std::string& input_path, const std::string& output_path);

} // namespace meow::masm