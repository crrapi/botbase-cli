// Checks that sys-botbase can read the running game's memory, then finds the
// first mapped page of the heap. The heap region rarely starts mapped.
//
// usage: example_memory_probe HOST

#include <botbase/botbase.hpp>

#include <format>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: example_memory_probe HOST\n";
        return 2;
    }

    try {
        botbase::Client console({.host = argv[1]});
        std::uint64_t heap = console.require_memory_access();
        std::cout << std::format("heap region {:#x}, main NSO {:#x}\n", heap, console.main_nso_base().value_or(0));

        for (int shift = 12; shift < 32; ++shift) {
            std::int64_t offset = std::int64_t{1} << shift;
            try {
                console.peek(offset, 4);
                std::cout << std::format("first readable heap offset: {:#x}\n", offset);
                return 0;
            } catch (const botbase::MemoryAccessError&) {
            }
        }
        std::cout << "no readable heap page below 4 GiB\n";
        return 1;
    } catch (const botbase::Error& error) {
        std::cerr << error.kind() << ": " << error.what() << '\n';
        return static_cast<int>(error.code());
    }
}
