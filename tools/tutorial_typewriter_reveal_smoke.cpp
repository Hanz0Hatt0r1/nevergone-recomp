#include <cassert>
#include <cstddef>
#include <iostream>

#include "tutorial_typewriter_reveal.h"

int main() {
    using nevergone::tutorial_typewriter_reveal::visible_characters;
    constexpr double kStep = 1.0 / 35.0;

    assert(visible_characters(20, 0, 1.0, kStep) == 0);
    assert(visible_characters(20, 1, 1.0, kStep) == 0);
    assert(visible_characters(20, 7, 1.0, kStep) == 4);
    assert(visible_characters(20, 35, 1.0, kStep) == 20);
    assert(visible_characters(20, 70, 1.0, kStep) == 20);
    assert(visible_characters(20, 0, 0.0, kStep) == 20);
    assert(visible_characters(0, 10, 1.0, kStep) == 0);

    std::cout << "tutorial typewriter reveal smoke: ok\n";
}
