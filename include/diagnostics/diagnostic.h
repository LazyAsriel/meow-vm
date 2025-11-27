#pragma once

#include "common/pch.h"

namespace meow {

enum class Severity { Note, Warning, Error, Total };

struct Span {
    std::string file;
    size_t start_line = 0, start_col = 0;
    size_t end_line = 0, end_col = 0;
};

struct StackFrame {
    std::string function;
    std::string file;
    size_t line = 0, col = 0;
};

struct Diagnostic;

struct LocaleSource {
    virtual ~LocaleSource() noexcept = default;
    virtual std::optional<std::string> get_template(const std::string& message_id) = 0;
};

struct Diagnostic {
    std::string code;

    Severity severity = Severity::Error;
    std::unordered_map<std::string, std::string> args;
    std::vector<Span> spans;
    std::vector<Diagnostic> notes;
    std::vector<StackFrame> callstack;
};

struct RenderOptions {
    bool enable_color = true;
    size_t context_lines = 2;
    size_t max_stack_frames = 10;
};

std::string render_to_human(const Diagnostic& diag, LocaleSource& locale, const RenderOptions& options);

}