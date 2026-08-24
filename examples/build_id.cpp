// Prints where Atmosphere looks for the running game's cheat file:
// atmosphere/contents/<TITLE ID>/cheats/<BUILD ID>.txt
//
// usage: example_build_id HOST

#include <botbase/botbase.hpp>

#include <format>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: example_build_id HOST\n";
        return 2;
    }

    try {
        botbase::Client console({.host = argv[1]});
        auto title_id = console.title_id();
        if (!title_id) {
            std::cerr << "no game is running\n";
            return 1;
        }
        std::cout << std::format("{} v{}\n", console.game_field("name"), console.game_field("version"));
        std::cout << std::format("atmosphere/contents/{:016X}/cheats/{}.txt\n", *title_id,
                                 botbase::to_upper(console.build_id().value_or("")));
    } catch (const botbase::Error& error) {
        std::cerr << error.kind() << ": " << error.what() << '\n';
        return static_cast<int>(error.code());
    }
}
