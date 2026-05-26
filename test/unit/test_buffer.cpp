#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>

#include "buffer.h"

TEST_CASE("StaticBuffer tracks remaining capacity from its offset",
          "[buffer]") {
    StaticBuffer<8> buffer{};

    REQUIRE(buffer.size() == 8);
    REQUIRE(buffer.capacity() == 8);
    REQUIRE(buffer.remain() == 8);
    REQUIRE_FALSE(buffer.done());
    REQUIRE(buffer.seek() == buffer.data());

    buffer += 3;
    REQUIRE(buffer.remain() == 5);
    REQUIRE(buffer.seek() == buffer.data() + 3);
    REQUIRE_FALSE(buffer.done());

    buffer += 5;
    REQUIRE(buffer.remain() == 0);
    REQUIRE(buffer.done());

    buffer.clear();
    REQUIRE(buffer.remain() == 8);
    REQUIRE(buffer.seek() == buffer.data());
}

TEST_CASE("DynamicBuffer tracks written bytes and can be reused",
          "[buffer]") {
    DynamicBuffer buffer{6};

    REQUIRE(buffer.size() == 6);
    REQUIRE(buffer.capacity() == 6);
    REQUIRE(buffer.read_count() == 0);
    REQUIRE(buffer.remain() == 6);

    buffer += 4;
    REQUIRE(buffer.read_count() == 4);
    REQUIRE(buffer.remain() == 2);
    REQUIRE(buffer.seek() == buffer.data() + 4);

    buffer.clear();
    REQUIRE(buffer.read_count() == 0);
    REQUIRE(buffer.remain() == 6);
    REQUIRE_FALSE(buffer.done());
}

TEST_CASE("Buffer_View is a non-owning view over caller storage",
          "[buffer]") {
    std::array<char, 5> bytes{'h', 'e', 'l', 'l', 'o'};
    Buffer_View view{bytes.data(), bytes.size()};

    REQUIRE(view.data() == bytes.data());
    REQUIRE(view.size() == bytes.size());
    REQUIRE(view.remain() == bytes.size());

    view += 2;
    REQUIRE(view.read_count() == 2);
    REQUIRE(view.seek() == bytes.data() + 2);
    REQUIRE(view.remain() == 3);

    view += 3;
    REQUIRE(view.done());

    view.clear();
    REQUIRE(view.read_count() == 0);
    REQUIRE(view.seek() == bytes.data());
}
