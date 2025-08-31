#include "../input.hpp"
#include <cassert>
#include <cstdio>

int main() {
    using namespace botbase;
    assert(normalize_button("a") == "A");
    assert(normalize_button("up") == "DUP");
    assert(normalize_button("+") == "PLUS");
    try {
        normalize_button("PLSU");
        assert(false);
    } catch (...) {}
    puts("ok");
}
