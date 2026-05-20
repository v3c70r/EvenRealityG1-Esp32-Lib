#include <catch2/catch_test_macros.hpp>
#include "eveng1/eveng1.hpp"

using namespace eveng1;

TEST_CASE("Rect union with empty", "[rect]") {
    Rect a{10, 10, 20, 20};
    Rect empty{};
    Rect u = a.unionWith(empty);
    REQUIRE(u.x == 10);
    REQUIRE(u.y == 10);
    REQUIRE(u.width == 20);
    REQUIRE(u.height == 20);
}

TEST_CASE("Rect union two rects", "[rect]") {
    Rect a{0, 0, 10, 10};
    Rect b{5, 5, 10, 10};
    Rect u = a.unionWith(b);
    REQUIRE(u.x == 0);
    REQUIRE(u.y == 0);
    REQUIRE(u.width == 15);
    REQUIRE(u.height == 15);
}

TEST_CASE("Rect contains", "[rect]") {
    Rect r{10, 10, 20, 20};
    REQUIRE(r.contains(10, 10) == true);
    REQUIRE(r.contains(29, 29) == true);
    REQUIRE(r.contains(30, 10) == false);
    REQUIRE(r.contains(10, 30) == false);
}

TEST_CASE("DirtyRect expand from empty", "[dirtyrect]") {
    DirtyRect dr;
    REQUIRE(dr.isEmpty());
    dr.expand(5, 5);
    REQUIRE(dr.x == 5);
    REQUIRE(dr.y == 5);
    REQUIRE(dr.width == 1);
    REQUIRE(dr.height == 1);
}

TEST_CASE("DirtyRect expand multiple points", "[dirtyrect]") {
    DirtyRect dr;
    dr.expand(0, 0);
    dr.expand(10, 10);
    REQUIRE(dr.x == 0);
    REQUIRE(dr.y == 0);
    REQUIRE(dr.width == 11);
    REQUIRE(dr.height == 11);
}

TEST_CASE("DirtyRect expand with rect", "[dirtyrect]") {
    DirtyRect dr;
    dr.expand(Rect{5, 5, 10, 10});
    dr.expand(Rect{20, 20, 5, 5});
    REQUIRE(dr.x == 5);
    REQUIRE(dr.y == 5);
    REQUIRE(dr.width == 20);
    REQUIRE(dr.height == 20);
}

TEST_CASE("DirtyRect reset", "[dirtyrect]") {
    DirtyRect dr;
    dr.expand(5, 5);
    dr.reset();
    REQUIRE(dr.isEmpty());
}
