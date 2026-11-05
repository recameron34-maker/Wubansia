// engine_tests' main. No JUCE: just Catch2. Every .cpp in engine/tests/ (but pic/) is compiled into
// this binary, together with alloc_counter.cpp's replacement allocation functions.

#include <catch2/catch_session.hpp>

int main (int argc, char* argv[])
{
    return Catch::Session().run (argc, argv);
}
