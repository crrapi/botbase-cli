// Tests for the parts of the library that need no console.

#include <botbase/botbase.hpp>

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;
std::vector<std::pair<std::string, std::function<void()>>> tests;

struct Register {
    Register(std::string name, std::function<void()> body) { tests.emplace_back(std::move(name), std::move(body)); }
};

#define TEST(name) \
    void name();   \
    Register register_##name(#name, name); \
    void name()

#define EXPECT(condition)                                                               \
    do {                                                                                \
        if (!(condition)) {                                                             \
            std::cerr << "  " << __FILE__ << ":" << __LINE__ << ": " #condition "\n";   \
            ++failures;                                                                 \
        }                                                                               \
    } while (0)

#define EXPECT_THROWS(expression, Type)        \
    do {                                       \
        bool threw = false;                    \
        try {                                  \
            (void)(expression);                \
        } catch (const Type&) {                \
            threw = true;                      \
        }                                      \
        EXPECT(threw && #expression " throws " #Type); \
    } while (0)

using namespace botbase;

TEST(integers_parse_like_python) {
    EXPECT(parse_i64("42") == 42);
    EXPECT(parse_i64("0x1F") == 31);
    EXPECT(parse_i64("-0x20") == -32);
    EXPECT(parse_i64("0b101") == 5);
    EXPECT(parse_i64("1_000") == 1000);
    EXPECT(parse_u64("0xFFFFFFFFFFFFFFFF") == 0xFFFFFFFFFFFFFFFFull);
    EXPECT(parse_i64("-9223372036854775808") == INT64_MIN);
    EXPECT_THROWS(parse_i64("12abc"), UsageError);
    EXPECT_THROWS(parse_u64("-1"), UsageError);
    EXPECT_THROWS(parse_u64("0x10000000000000000"), UsageError);
}

TEST(hex_arguments_put_the_sign_first) {
    EXPECT(hex_arg(0x10) == "0x10");
    EXPECT(hex_arg(-0x20) == "-0x20");
    EXPECT(hex_arg(0) == "0x0");
    EXPECT(hex_arg(INT64_MIN) == "-0x8000000000000000");
}

TEST(hex_bytes_round_trip) {
    EXPECT(to_hex(Bytes{0xDE, 0xAD}) == "DEAD");
    EXPECT(decode_hex("dead") == Bytes({0xDE, 0xAD}));
    EXPECT(!decode_hex("abc"));
    EXPECT(!decode_hex("zz"));
    EXPECT(parse_hex_bytes("0xf") == Bytes({0x0F}));
    EXPECT(parse_hex_bytes("ff 01") == Bytes({0xFF, 0x01}));
    EXPECT(parse_hex_number("0000000000000004") == 4u);
    EXPECT(parse_hex_number("0x1000000000") == 0x1000000000u);
    EXPECT(!parse_hex_number(""));
}

TEST(versions_are_recognised) {
    EXPECT(is_version_string("2.6"));
    EXPECT(is_version_string("2"));
    EXPECT(!is_version_string("2..6"));
    EXPECT(!is_version_string("2.6."));
    EXPECT(!is_version_string("DEAD"));
}

TEST(buttons_are_validated) {
    EXPECT(normalize_button("a") == "A");
    EXPECT(normalize_button("up") == "DUP");
    EXPECT(normalize_button("+") == "PLUS");
    EXPECT(normalize_button("dr") == "DR");
    EXPECT_THROWS(normalize_button("PLSU"), UsageError);
}

TEST(keys_resolve_by_name_and_number) {
    EXPECT(key_code("Return") == 40);
    EXPECT(key_code("a") == 4);
    EXPECT(key_code("225") == 225);
    EXPECT(key_code("F13") == 104);
    EXPECT(key_code("RightGui") == 231);
    EXPECT_THROWS(key_code(2), UsageError);
    EXPECT_THROWS(key_code("Nope"), UsageError);
    EXPECT(modifier_bit("shift") == 1);
}

TEST(us_layout_marks_shifted_characters) {
    EXPECT(us_layout('a')->code == 4 && !us_layout('a')->shift);
    EXPECT(us_layout('B')->code == 5 && us_layout('B')->shift);
    EXPECT(us_layout('0')->code == 39);
    EXPECT(us_layout('!')->code == 30 && us_layout('!')->shift);
    EXPECT(us_layout('\n')->code == 40);
    EXPECT(!us_layout('\x01'));
}

TEST(sequences_parse_and_build) {
    std::vector<std::string> tokens = {"A,W250,+L,%-100,-200,-L,&0,0"};
    auto steps = parse_sequence(tokens);
    std::vector<Step> expected = {
        Step::click("A"),   Step::wait(250),           Step::press("L"),
        Step::stick(Side::Left, -100, -200), Step::release("L"), Step::stick(Side::Right, 0, 0),
    };
    EXPECT(steps == expected);
    EXPECT(build_click_seq(steps) == "A,W250,+L,%-100,-200,-L,&0,0");

    auto clamped = std::vector<Step>{Step::stick(Side::Left, -0x9000, 100)};
    EXPECT(build_click_seq(clamped) == "%-32768,100");

    EXPECT_THROWS(parse_sequence(std::string_view("%5")), UsageError);
    EXPECT_THROWS(parse_sequence(std::string_view("PLSU")), UsageError);
    auto hold = std::vector<Step>{Step::hold("A", 100)};
    EXPECT_THROWS(build_click_seq(hold), UsageError);
}

TEST(values_encode_and_show) {
    EXPECT(encode_value("0x10", Format::U32) == Bytes({0x10, 0, 0, 0}));
    EXPECT(encode_value("-1", Format::I16) == Bytes({0xFF, 0xFF}));
    EXPECT(encode_value("ff01", Format::Hex) == Bytes({0xFF, 0x01}));
    EXPECT(encode_value("1.5", Format::F32) == Bytes({0, 0, 0xC0, 0x3F}));
    EXPECT_THROWS(encode_value("256", Format::U8), UsageError);
    EXPECT_THROWS(encode_value("-1", Format::U32), UsageError);

    EXPECT(show_bytes(Bytes{0, 0, 0xC0, 0x3F}, Format::F32) == "1.5");
    EXPECT(show_bytes(Bytes{0xFF, 0xFF}, Format::I16) == "-1");
    EXPECT(show_bytes(Bytes{0xFF, 0xFF}, Format::U16) == "65535");
    EXPECT(show_bytes(Bytes{0x41, 0x00}, Format::Dump, 0x10) ==
           "000000000010  41 00                                            A.");
    EXPECT_THROWS(show_bytes(Bytes{1}, Format::U32), UsageError);
}

TEST(doubles_print_like_python) {
    EXPECT(format_double(1.5) == "1.5");
    EXPECT(format_double(100.0) == "100.0");
    EXPECT(format_double(0.0) == "0.0");
    EXPECT(format_double(1e16) == "1e+16");
    EXPECT(format_double(0.0001) == "0.0001");
    EXPECT(format_double(0.00001) == "1e-05");
    EXPECT(format_double(-2.25) == "-2.25");
}

TEST(scripts_split_like_a_shell) {
    auto commands = split_commands("click A; wait 100\n# comment\ntype \"hi there\" 'a;b'\n");
    EXPECT(commands.size() == 3);
    EXPECT((commands[0] == std::vector<std::string>{"click", "A"}));
    EXPECT((commands[1] == std::vector<std::string>{"wait", "100"}));
    EXPECT((commands[2] == std::vector<std::string>{"type", "hi there", "a;b"}));
    EXPECT_THROWS(split_commands("type \"open"), UsageError);
}

TEST(protocol_tables) {
    EXPECT(is_reply_command("peek"));
    EXPECT(!is_reply_command("poke"));
    EXPECT(is_known_command("poke"));
    EXPECT(!is_known_command("bogus"));
    EXPECT(parse_controller_type("joycon-right") == 1);
    EXPECT(parse_controller_type("3") == 3);
    EXPECT(language_name(1) == "American English");
    EXPECT(find_config_key("pollRate")->default_value == 17);
}

}  // namespace

int main() {
    for (const auto& [name, body] : tests) {
        int before = failures;
        try {
            body();
        } catch (const std::exception& error) {
            std::cerr << "  unexpected exception: " << error.what() << '\n';
            ++failures;
        }
        std::cout << (failures == before ? "ok    " : "FAIL  ") << name << '\n';
    }
    std::cout << tests.size() << " tests, " << failures << " failed checks\n";
    return failures == 0 ? 0 : 1;
}
