#include <doctest/doctest.h>

#include <string>

#include "app/progress.hpp"
#include "support/shell_harness.hpp"

using playtube::app::ProgressStore;
using playtube::app::WatchProgress;

TEST_CASE("progress: where videos were left, kept across a restart; where they start again") {
    playtube::test::ShellDir dir;
    const std::string path = dir.path + "/progress.json";
    {
        ProgressStore store(path);
        CHECK_FALSE(store.find("aaaaaaaaaaa"));
        CHECK(store.percent("aaaaaaaaaaa") == -1);
        store.set("aaaaaaaaaaa", 300, 1200, 100);
        store.set("bbbbbbbbbbb", 1190, 1200, 101);  // watched to its end
        store.set("ccccccccccc", 10, 1200, 102);    // barely started
        store.set("live0000000", 50, 0, 103);       // a live: not kept
        REQUIRE(store.save());
    }
    ProgressStore again(path);
    REQUIRE(again.find("aaaaaaaaaaa"));
    CHECK(again.find("aaaaaaaaaaa")->position == 300);
    CHECK(again.percent("aaaaaaaaaaa") == 25);
    CHECK(again.percent("bbbbbbbbbbb") == 100);
    CHECK_FALSE(again.find("live0000000"));
    CHECK(ProgressStore::resumeAt(*again.find("aaaaaaaaaaa")) == 300);
    CHECK(ProgressStore::resumeAt(*again.find("bbbbbbbbbbb")) == 0);
    CHECK(ProgressStore::resumeAt(*again.find("ccccccccccc")) == 0);
}

TEST_CASE("progress: past 1000 videos the oldest goes") {
    ProgressStore store;
    for (int i = 0; i < 1001; ++i) {
        store.set("v" + std::to_string(i), 100, 1000, 1000 + i);
    }
    CHECK_FALSE(store.find("v0"));
    CHECK(store.find("v1"));
    CHECK(store.find("v1000"));
}
