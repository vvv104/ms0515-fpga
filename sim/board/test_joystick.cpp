// joystick: the lines a run's file says are held, and when.
#include <doctest/doctest.h>

#include "joystick.h"

#include <sstream>

TEST_CASE("joystick: no file, no line held")
{
    std::istringstream none("");
    const Joystick joy(none);
    CHECK(joy.held(0) == 0);
    CHECK(joy.held(1'000'000'000) == 0);
}

TEST_CASE("joystick: each line holds its lines from its time to the next line's")
{
    std::istringstream file("# right, then up and fire, then let go\n"
                            "100 1\n"
                            "250 30   # up 010, fire 020\n"
                            "\n"
                            "400 0\n");
    const Joystick joy(file);
    const std::uint64_t ms = Joystick::kTicksPerMs;
    CHECK(joy.held(99 * ms) == 0);
    CHECK(joy.held(100 * ms) == 1);
    CHECK(joy.held(249 * ms) == 1);
    CHECK(joy.held(250 * ms) == 030);
    CHECK(joy.held(399 * ms) == 030);
    CHECK(joy.held(400 * ms) == 0);
    CHECK(joy.held(5000 * ms) == 0);
}

TEST_CASE("joystick: the lines may come in any order, and only five lines there are")
{
    std::istringstream file("300 2\n100 4\n200 377\n");
    const Joystick joy(file);
    const std::uint64_t ms = Joystick::kTicksPerMs;
    CHECK(joy.held(150 * ms) == 4);
    CHECK(joy.held(250 * ms) == 037);
    CHECK(joy.held(350 * ms) == 2);
}
