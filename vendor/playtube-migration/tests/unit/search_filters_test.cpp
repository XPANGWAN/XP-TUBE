#include <doctest/doctest.h>

#include "youtube/search_filters.hpp"

using playtube::youtube::SearchFilters;
using playtube::youtube::searchParams;

TEST_CASE("search filters: YouTube's own params for each filter") {
    SearchFilters f;
    CHECK(searchParams(f).empty());
    CHECK_FALSE(f.any());
    f.type = SearchFilters::Type::Short;
    CHECK(searchParams(f) == "EgIQCQ==");  // what youtube.com sends for Shorts
    f = {};
    f.type = SearchFilters::Type::Video;
    CHECK(searchParams(f) == "EgIQAQ==");
    f = {};
    f.upload = SearchFilters::Upload::Hour;
    CHECK(searchParams(f) == "EgIIAQ==");
    f = {};
    f.length = SearchFilters::Length::Under4;
    CHECK(searchParams(f) == "EgIYAQ==");
    f = {};
    f.live = true;
    CHECK(searchParams(f) == "EgJAAQ==");
    f = {};
    f.sort = SearchFilters::Sort::Views;
    CHECK(searchParams(f) == "CAM=");
    f.upload = SearchFilters::Upload::Week;
    f.type = SearchFilters::Type::Video;
    CHECK(searchParams(f) == "CAMSBAgDEAE=");
    CHECK(f.any());
}
