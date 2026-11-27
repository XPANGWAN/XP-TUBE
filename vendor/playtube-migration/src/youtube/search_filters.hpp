#pragma once

#include <string>

namespace playtube::youtube {

// The search filters YouTube's own pages offer.
struct SearchFilters {
    enum class Sort { Relevance, Date, Views, Rating };
    enum class Upload { Any, Hour, Today, Week, Month, Year };
    enum class Type { Any, Video, Channel, Playlist, Short };
    enum class Length { Any, Under4, From4To20, Over20 };
    Sort sort = Sort::Relevance;
    Upload upload = Upload::Any;
    Type type = Type::Any;
    Length length = Length::Any;
    bool live = false;  // only lives being streamed

    bool any() const {
        return sort != Sort::Relevance || upload != Upload::Any || type != Type::Any || length != Length::Any || live;
    }
};

// The search request's "params": YouTube's protobuf of the filters in base64 ("EgIQCQ==" for Shorts); "" for none.
std::string searchParams(const SearchFilters& filters);

}  // namespace playtube::youtube
