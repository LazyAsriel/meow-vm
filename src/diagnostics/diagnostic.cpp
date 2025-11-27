#include "diagnostics/diagnostic.h"

namespace meow {

// ============================================================================
// 🎨 Severity level string table
// ============================================================================
static constexpr std::array<std::string_view, static_cast<size_t>(Severity::Total)> severities = {"note", "warning", "error"};

/// Convert a Severity enum to its lowercase string representation.
inline constexpr std::string_view severity_to_string(Severity severity) noexcept {
    return severities.at(static_cast<size_t>(severity));
}

// ============================================================================
// 🧩 Template rendering (e.g. "expected {token}" -> fill args)
// ============================================================================
inline std::string render_template(const std::string& tmpl, const std::unordered_map<std::string, std::string>& args) {
    std::string out;
    out.reserve(tmpl.size() * 2);

    for (size_t i = 0; i < tmpl.size();) {
        size_t p = tmpl.find('{', i);
        if (p == std::string::npos) {
            out.append(tmpl, i, std::string::npos);
            break;
        }

        out.append(tmpl, i, p - i);
        size_t q = tmpl.find('}', p + 1);
        if (q == std::string::npos) {
            out.append(tmpl, p, std::string::npos);
            break;
        }

        std::string key = tmpl.substr(p + 1, q - p - 1);
        auto it = args.find(key);
        if (it != args.end())
            out += it->second;
        else {
            out.push_back('{');
            out += key;
            out.push_back('}');
        }
        i = q + 1;
    }
    return out;
}

// ============================================================================
// 📖 Utility: read specific line(s) from a file for context
// ============================================================================
inline std::pair<std::vector<std::string>, bool> read_snippet(const std::string& file, size_t start_line, size_t end_line) {
    std::ifstream fin(file);
    if (!fin) return {{}, false};

    std::vector<std::string> lines;
    lines.reserve(end_line - start_line + 1);

    std::string line;
    size_t lineno = 0;

    while (std::getline(fin, line)) {
        ++lineno;
        if (lineno >= start_line && lineno <= end_line) lines.push_back(std::move(line));
        if (lineno > end_line) break;
    }

    return {std::move(lines), !lines.empty()};
}

/// Read a single line from a file.
inline std::pair<std::string, bool> read_line(const std::string& file, size_t line_no) {
    std::ifstream fin(file);
    if (!fin) return {"", false};

    std::string line;
    size_t ln = 0;

    while (std::getline(fin, line)) {
        if (++ln == line_no) return {line, true};
    }
    return {"", false};
}

// ============================================================================
// 🌈 ANSI Color Helper
// ============================================================================
inline std::string color_wrap(const std::string& msg, std::string_view code, bool enable) noexcept {
    if (!enable) return msg;
    return std::string("\x1b[") + std::string(code) + "m" + msg + "\x1b[0m";
}

// ============================================================================
// 🧼 JSON Escape Helper (for safe serialization)
// ============================================================================
inline std::string json_escape(const std::string& s) noexcept {
    std::ostringstream output;
    for (char c : s) {
        switch (c) {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\b':
                output << "\\b";
                break;
            case '\f':
                output << "\\f";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    output << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c)) << std::dec;
                } else {
                    output << c;
                }
        }
    }
    return output.str();
}

// ============================================================================
// 🧠 Human-readable rendering
// ============================================================================
std::string render_to_human(const Diagnostic& d, LocaleSource& locale, const RenderOptions& options) {
    std::ostringstream out;

    const auto severity = severity_to_string(d.severity);
    const std::string severity_color = (d.severity == Severity::Error) ? "1;31" : (d.severity == Severity::Warning) ? "1;33" : "1;34";

    const auto tmpl = locale.get_template(d.code);
    const std::string hdr = tmpl ? render_template(*tmpl, d.args) : d.code;

    out << color_wrap(std::string(severity) + ": ", severity_color, options.enable_color) << color_wrap(hdr, "1", options.enable_color) << "\n";

    // --- Stack trace (if present)
    if (!d.callstack.empty()) {
        out << "Stack trace (most recent call last):\n";
        size_t shown = 0;
        for (const auto& f : d.callstack) {
            if (shown++ >= options.max_stack_frames) break;

            out << "  " << (shown == 1 ? "=>" : "  ") << " " << f.function;
            if (!f.file.empty()) out << " at " << f.file << ":" << f.line << ":" << f.col;
            out << "\n";

            // Show line context for top frame
            if (!f.file.empty() && f.line > 0) {
                auto [line, ok] = read_line(f.file, f.line);
                if (ok) {
                    std::ostringstream num;
                    num << std::setw(6) << f.line;
                    out << " " << color_wrap(num.str(), "34", options.enable_color) << " | " << line << "\n";

                    const size_t caret_pos = std::max<size_t>(1, f.col);
                    std::string pad(caret_pos - 1 + 1 + 6 + 3, ' ');
                    out << pad << color_wrap("^", "33", options.enable_color) << "\n";
                }
            }
        }
        out << "\n";
    }

    // --- Span context (source excerpts)
    for (const auto& sp : d.spans) {
        const size_t start_context = std::max<size_t>(1, sp.start_line - options.context_lines);
        const size_t end_context = sp.end_line + options.context_lines;

        auto [lines, ok] = read_snippet(sp.file, start_context, end_context);

        out << "  " << color_wrap("-->", "34", options.enable_color) << " " << sp.file << ":" << sp.start_line << ":" << sp.start_col << "\n";

        if (!ok) continue;

        size_t lineno = start_context;
        for (const auto& line : lines) {
            std::ostringstream num;
            num << std::setw(4) << lineno;
            out << " " << color_wrap(num.str(), "34", options.enable_color) << " | " << line << "\n";

            // highlight affected lines with carets
            if (lineno >= sp.start_line && lineno <= sp.end_line) {
                size_t caret_start = (lineno == sp.start_line) ? sp.start_col : 1;
                size_t caret_end = (lineno == sp.end_line) ? sp.end_col : std::max<size_t>(1, line.size());

                caret_start = std::max<size_t>(1, caret_start);
                caret_end = std::max(caret_end, caret_start);

                std::string pad(caret_start - 1 + 1 + 4 + 3, ' ');
                out << pad << color_wrap(std::string(caret_end - caret_start + 1, '^'), "33", options.enable_color) << "\n";
            }
            ++lineno;
        }
    }

    // --- Notes
    for (const auto& n : d.notes) {
        const std::string ntmpl = locale.get_template(n.code).value_or(n.code);
        out << color_wrap("note: ", "1;34", options.enable_color) << render_template(ntmpl, n.args) << "\n";
    }

    return out.str();
}

// ============================================================================
// 📦 JSON Rendering (machine-friendly diagnostic format)
// ============================================================================
std::string render_to_json(const Diagnostic& d, LocaleSource& loc) {
    std::ostringstream out;
    const auto tmpl = loc.get_template(d.code);
    const std::string message = tmpl ? render_template(*tmpl, d.args) : d.code;

    out << "{\n"
        << "  \"code\": \"" << json_escape(d.code) << "\",\n"
        << "  \"severity\": \"" << severity_to_string(d.severity) << "\",\n"
        << "  \"message\": \"" << json_escape(message) << "\",\n"
        << "  \"spans\": [\n";

    for (size_t i = 0; i < d.spans.size(); ++i) {
        const auto& s = d.spans[i];
        out << "    {\"file\":\"" << json_escape(s.file) << "\", \"start_line\":" << s.start_line << ", \"start_col\":" << s.start_col << ", \"end_line\":" << s.end_line
            << ", \"end_col\":" << s.end_col << "}";
        if (i + 1 < d.spans.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n  \"notes\": [\n";
    for (size_t i = 0; i < d.notes.size(); ++i) {
        const auto& n = d.notes[i];
        const auto nt = loc.get_template(n.code).value_or(n.code);
        out << "    {\"code\":\"" << json_escape(n.code) << "\", \"message\": \"" << json_escape(render_template(nt, n.args)) << "\"}";
        if (i + 1 < d.notes.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n  \"callstack\": [\n";
    for (size_t i = 0; i < d.callstack.size(); ++i) {
        const auto& f = d.callstack[i];
        out << "    {\"function\":\"" << json_escape(f.function) << "\", \"file\":\"" << json_escape(f.file) << "\", \"line\":" << f.line << ", \"col\":" << f.col << "}";
        if (i + 1 < d.callstack.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

}
