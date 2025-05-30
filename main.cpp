#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <print>
#include <ranges>
#include <cassert>
#include <optional>

#include <magic_enum/magic_enum.hpp>

using namespace std::literals;


constexpr auto example = R"str(const lang = "tru";
runtime.print("Hello from {}", lang);
var num = 12345.6789;
)str"sv;


struct token {
    enum struct types {
        equal,
        semicolon,
        dot,
        comma,
        l_paren,
        r_paren,
        string_literal,
        number_literal,
        identifier,
        kw_const,
        kw_var,
        eof
    };

    types type;
    std::string_view lexeme;
    size_t line;
    size_t column;
};

namespace {
    auto space_buf = [] {
        auto buf = std::array<char, 1024>{};
        buf.fill(' ');
        return buf;
    }();
}

class scanner {
    //size_t pos = 0;
    size_t cur_line = 1;
    size_t cur_column = 0;
    std::string_view::const_pointer cursor = nullptr;
    std::string_view::const_pointer line_begin = nullptr;

    [[nodiscard]] char cur() const {
        assert(!eof());
        return *cursor;
    }

    [[nodiscard]] bool eof() const {
        assert(cursor);
        assert(cursor >= src.begin() && cursor <= src.end());
        return cursor == src.end();
    }

    [[nodiscard]] size_t pos() const {
        assert(!eof());
        return cursor - src.begin();
    }

    bool move_pos() {
        assert(!eof());
        if (*cursor == '\n') {
            ++cur_line;
            cur_column = 0;
            line_begin = nullptr;
        } else {
            ++cur_column;
        }
        ++cursor;
        if (line_begin == nullptr) {
            line_begin = cursor;
        }
        return !eof();
    }

    [[nodiscard]] std::optional<char> peek_next() const {
        assert(!eof());
        if (cursor + 1 == src.end()) {
            return std::nullopt;
        }
        return *(cursor + 1);
    }

    [[noreturn]]
    void failure(const char* message) {
        static auto empty = std::array<char, 512>();
        empty.fill(' ');

        std::println(stderr, "Error at line {} column {}: {}", cur_line, cur_column, message);
        auto eol = cursor;
        while (eol != src.end() && *eol != '\n') {
            ++eol;
        }
        std::println(stderr, "{}", src.substr(line_begin - src.begin(), eol - line_begin));
        std::println(stderr, "{}^", std::string_view(empty.begin(), cur_column));
        exit(1);
    }

    void emit(token::types t) {
        tokens.emplace_back(t, src.substr(pos(), 1), cur_line, cur_column);
    }

    void emit(token::types t, std::string_view lexeme) {
        auto diff = cursor - lexeme.begin();
        tokens.emplace_back(t, lexeme, cur_line, cur_column - diff);
    }

private:
    void scan_number() {
        auto start = cursor;
        do {
            move_pos();
        } while (!eof() && std::isdigit(cur()));

        if (cur() == '.') {
            move_pos();
            while (std::isdigit(cur())) {
                move_pos();
            }
        }
        auto lexeme = std::string_view(start, cursor - start);
        emit(token::types::number_literal, lexeme);
    }

    void scan_string() {
        auto start = cursor;

        do {
            move_pos();
        } while (!eof() && cur() != '"');

        if (cur() == '"') {
            move_pos();
        } else {
            failure("Unterminated string reached end of file");
        }

        auto lexeme = std::string_view(start, cursor - start);
        emit(token::types::string_literal, lexeme);
    }

    void scan_identifier() {
        auto start = cursor;

        do {
            move_pos();
        } while (!eof() && std::isalnum(cur()));

        auto lexeme = std::string_view(start, cursor - start);
        if (lexeme == "var") {
            emit(token::types::kw_var, lexeme);
        } else if (lexeme == "const") {
            emit(token::types::kw_const, lexeme);
        } else {
            emit(token::types::identifier, lexeme);
        }
    }

public:
    auto scan(std::string_view input) -> std::span<token> {
        using ttype = token::types;

        src = input;
        cursor = src.data();
        line_begin = src.data();

        while (!eof()) {
            switch (cur()) {
                case '=':
                    emit(ttype::equal);
                    move_pos();
                    break;
                case ';':
                    emit(ttype::semicolon);
                    move_pos();
                    break;
                case '\"': {
                    scan_string();
                    break;
                }
                case '.':
                    emit(ttype::dot);
                    move_pos();
                    break;
                case ',':
                    emit(ttype::comma);
                    move_pos();
                    break;
                case '(':
                    emit(ttype::l_paren);
                    move_pos();
                    break;
                case ')':
                    emit(ttype::r_paren);
                    move_pos();
                    break;
                default: {
                    if (isspace(cur())) {
                        move_pos();
                        break;
                    } else if (std::isalpha(cur())) {
                        scan_identifier();
                    } else if (std::isdigit(cur())) {
                        scan_number();
                    }
                    else {
                        failure("Unhandled text sequence");
                    }
                }
            }
        }
        tokens.emplace_back(token::types::eof, std::string_view(), cur_line, cur_column);

        return tokens;
    }

private:
    std::string_view src;
    std::vector<token> tokens;
};

template<>
struct std::formatter<token> {
    // ReSharper disable once CppMemberFunctionMayBeStatic
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    constexpr auto parse(std::format_parse_context &ctx) { // NOLINT(*-convert-member-functions-to-static)
        return ctx.begin();
    }

    auto format(const token &obj, auto &ctx) const {
        return std::format_to(ctx.out(), "token{{{}:{}:{}:{}}}", obj.line, obj.column, magic_enum::enum_name(obj.type), obj.lexeme);
    }
};

void print_tokens(std::span<token> result, bool compact = true) {
    std::println("Tokens:");
    auto last_line = -1uz;
    auto first_line = true;

    for (auto &token: result) {
        if (token.line != last_line) {
            if (!first_line) {
                std::println();
            }
            first_line = false;
            last_line = token.line;
            std::print("line {}: ", last_line);
        }
        if (compact) {
            switch (token.type) {
                case token::types::string_literal:
                case token::types::identifier:
                case token::types::number_literal:
                    std::print("{}:{} ", magic_enum::enum_name(token.type), token.lexeme);
                    break;
                default:
                    std::print("{} ", magic_enum::enum_name(token.type));
                    break;
            }
        } else {
            std::print("{} ", token);
        }
    }
}

void annotate(std::span<token> result, std::string_view source) {
    struct annotated_line {
        explicit annotated_line(size_t n, std::string_view l): line_no(n), line(l) {}
        std::size_t line_no;
        std::string_view line;
        std::vector<token> tokens;
    };

    namespace r = std::ranges;
    auto a_lines = source
                   | r::views::split('\n')
                   | r::views::transform([counter = 0](auto&& l) mutable  {
                       return annotated_line(++counter, std::string_view(l.begin(), l.size()));
                   })
                   | r::to<std::vector>();

    for (auto& t: result) {
        a_lines.at(t.line - 1).tokens.push_back(t);
    }

    auto tmp_buf = std::string(space_buf.data(), space_buf.size());

    for (auto& al: a_lines) {
        std::println("line {}: {}", al.line_no, al.line);

        for (auto idx = static_cast<ssize_t>(al.tokens.size()) - 1; idx >= 0; --idx) {
            const auto target_tok = al.tokens.at(idx);
            std::fill_n(tmp_buf.begin(), al.line.size(), ' ');
            for (auto i = 0; i < idx; ++i) {
                tmp_buf[al.tokens.at(i).column] = '|';
            }
            tmp_buf[target_tok.column] = '^';
            std::println("        {} {}", std::string_view(tmp_buf.data(), target_tok.column + 1), magic_enum::enum_name(target_tok.type));
        }
    }
}

int main() {

    auto s = scanner();
    auto result = s.scan(example);

    annotate(result, example);
    return 0;
}
