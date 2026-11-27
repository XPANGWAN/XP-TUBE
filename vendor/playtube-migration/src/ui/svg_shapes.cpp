#include "ui/svg_shapes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace playtube::ui {

namespace {

constexpr std::size_t kMaxText = 64 * 1024;
constexpr std::size_t kMaxShapes = 512;
constexpr std::size_t kMaxPoints = 50000;
constexpr int kMaxHeight = 256;
constexpr int kMaxWidth = 2048;
constexpr int kCurveSteps = 16;
constexpr int kSubRows = 5;  // vertical samples per pixel (horizontal coverage is exact)

struct Point {
    double x = 0;
    double y = 0;
};

struct Shape {
    std::vector<std::vector<Point>> contours;
    std::uint8_t r = 0, g = 0, b = 0;
};

// the value of attribute `name` in the tag text `tag` (no entities), nullopt when absent
std::optional<std::string> attribute(std::string_view tag, std::string_view name) {
    std::size_t at = 0;
    while ((at = tag.find(name, at)) != std::string_view::npos) {
        const bool start = at > 0 && (tag[at - 1] == ' ' || tag[at - 1] == '\t' || tag[at - 1] == '\n' || tag[at - 1] == '\r');
        std::size_t i = at + name.size();
        while (i < tag.size() && (tag[i] == ' ' || tag[i] == '\t' || tag[i] == '\n' || tag[i] == '\r')) {
            ++i;
        }
        if (start && i < tag.size() && tag[i] == '=') {
            ++i;
            while (i < tag.size() && (tag[i] == ' ' || tag[i] == '\t' || tag[i] == '\n' || tag[i] == '\r')) {
                ++i;
            }
            if (i < tag.size() && (tag[i] == '"' || tag[i] == '\'')) {
                const char quote = tag[i];
                const std::size_t end = tag.find(quote, i + 1);
                if (end == std::string_view::npos) {
                    return std::nullopt;
                }
                return std::string(tag.substr(i + 1, end - i - 1));
            }
            return std::nullopt;
        }
        at += name.size();
    }
    return std::nullopt;
}

// numbers of a path or a points list: "12.5", "-3", ".5e2", separated by spaces, commas or a sign
class Numbers {
public:
    explicit Numbers(std::string_view text) : text_(text) {}
    void skip() {
        while (i_ < text_.size() && (text_[i_] == ' ' || text_[i_] == ',' || text_[i_] == '\t' || text_[i_] == '\n' ||
                                     text_[i_] == '\r')) {
            ++i_;
        }
    }
    bool atNumber() {
        skip();
        if (i_ >= text_.size()) {
            return false;
        }
        const char c = text_[i_];
        return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
    }
    bool atEnd() {
        skip();
        return i_ >= text_.size();
    }
    char command() { return text_[i_++]; }
    std::optional<double> number() {
        skip();
        std::size_t j = i_;
        if (j < text_.size() && (text_[j] == '-' || text_[j] == '+')) {
            ++j;
        }
        bool digits = false;
        bool dot = false;
        while (j < text_.size() && ((text_[j] >= '0' && text_[j] <= '9') || (text_[j] == '.' && !dot))) {
            dot = dot || text_[j] == '.';
            digits = digits || text_[j] != '.';
            ++j;
        }
        if (!digits) {
            return std::nullopt;
        }
        if (j < text_.size() && (text_[j] == 'e' || text_[j] == 'E')) {
            std::size_t k = j + 1;
            if (k < text_.size() && (text_[k] == '-' || text_[k] == '+')) {
                ++k;
            }
            if (k < text_.size() && text_[k] >= '0' && text_[k] <= '9') {
                j = k;
                while (j < text_.size() && text_[j] >= '0' && text_[j] <= '9') {
                    ++j;
                }
            }
        }
        const std::string piece(text_.substr(i_, j - i_));
        i_ = j;
        const double value = std::strtod(piece.c_str(), nullptr);
        if (!std::isfinite(value) || std::fabs(value) > 1e6) {
            return std::nullopt;
        }
        return value;
    }

private:
    std::string_view text_;
    std::size_t i_ = 0;
};

std::optional<Shape> colour(const std::string& fill) {
    Shape shape;
    if (fill.size() != 7 || fill[0] != '#') {
        return std::nullopt;
    }
    for (std::size_t i = 1; i < 7; ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(fill[i]))) {
            return std::nullopt;
        }
    }
    const long value = std::strtol(fill.c_str() + 1, nullptr, 16);
    shape.r = static_cast<std::uint8_t>(value >> 16);
    shape.g = static_cast<std::uint8_t>(value >> 8);
    shape.b = static_cast<std::uint8_t>(value);
    return shape;
}

void cubic(std::vector<Point>& out, Point p0, Point p1, Point p2, Point p3) {
    for (int s = 1; s <= kCurveSteps; ++s) {
        const double t = static_cast<double>(s) / kCurveSteps;
        const double u = 1 - t;
        out.push_back({u * u * u * p0.x + 3 * u * u * t * p1.x + 3 * u * t * t * p2.x + t * t * t * p3.x,
                       u * u * u * p0.y + 3 * u * u * t * p1.y + 3 * u * t * t * p2.y + t * t * t * p3.y});
    }
}

void quadratic(std::vector<Point>& out, Point p0, Point p1, Point p2) {
    for (int s = 1; s <= kCurveSteps; ++s) {
        const double t = static_cast<double>(s) / kCurveSteps;
        const double u = 1 - t;
        out.push_back({u * u * p0.x + 2 * u * t * p1.x + t * t * p2.x, u * u * p0.y + 2 * u * t * p1.y + t * t * p2.y});
    }
}

// a path's "d": its contours as points; false on anything not understood
bool parsePath(std::string_view d, std::vector<std::vector<Point>>& contours, std::size_t& points) {
    Numbers in(d);
    Point current;
    Point start;
    Point lastControl;
    char last = 0;
    char command = 0;
    std::size_t commands = 0;
    std::vector<Point>* contour = nullptr;
    const auto read = [&](double& v) {
        const auto n = in.number();
        if (!n) {
            return false;
        }
        v = *n;
        return true;
    };
    while (!in.atEnd()) {
        if (!in.atNumber()) {
            command = in.command();
        } else if (command == 0 || command == 'z' || command == 'Z') {
            return false;  // numbers with no command before them
        }
        const bool rel = command >= 'a' && command <= 'z';
        const char upper = static_cast<char>(rel ? command - 32 : command);
        const Point base = rel ? current : Point{0, 0};
        double a = 0, b = 0, c = 0, e = 0, f = 0, g = 0;
        switch (upper) {
        case 'M':
            if (!read(a) || !read(b)) {
                return false;
            }
            current = {base.x + a, base.y + b};
            start = current;
            contours.push_back({current});
            contour = &contours.back();
            command = rel ? 'l' : 'L';  // further pairs are lines
            break;
        case 'L':
            if (!read(a) || !read(b) || contour == nullptr) {
                return false;
            }
            current = {base.x + a, base.y + b};
            contour->push_back(current);
            break;
        case 'H':
            if (!read(a) || contour == nullptr) {
                return false;
            }
            current.x = rel ? current.x + a : a;
            contour->push_back(current);
            break;
        case 'V':
            if (!read(a) || contour == nullptr) {
                return false;
            }
            current.y = rel ? current.y + a : a;
            contour->push_back(current);
            break;
        case 'C': {
            if (!read(a) || !read(b) || !read(c) || !read(e) || !read(f) || !read(g) || contour == nullptr) {
                return false;
            }
            const Point p1{base.x + a, base.y + b};
            const Point p2{base.x + c, base.y + e};
            const Point p3{base.x + f, base.y + g};
            cubic(*contour, current, p1, p2, p3);
            lastControl = p2;
            current = p3;
            break;
        }
        case 'S': {
            if (!read(a) || !read(b) || !read(c) || !read(e) || contour == nullptr) {
                return false;
            }
            const bool follows = last == 'C' || last == 'S';
            const Point p1 = follows ? Point{2 * current.x - lastControl.x, 2 * current.y - lastControl.y} : current;
            const Point p2{base.x + a, base.y + b};
            const Point p3{base.x + c, base.y + e};
            cubic(*contour, current, p1, p2, p3);
            lastControl = p2;
            current = p3;
            break;
        }
        case 'Q': {
            if (!read(a) || !read(b) || !read(c) || !read(e) || contour == nullptr) {
                return false;
            }
            const Point p1{base.x + a, base.y + b};
            const Point p2{base.x + c, base.y + e};
            quadratic(*contour, current, p1, p2);
            lastControl = p1;
            current = p2;
            break;
        }
        case 'T': {
            if (!read(a) || !read(b) || contour == nullptr) {
                return false;
            }
            const bool follows = last == 'Q' || last == 'T';
            const Point p1 = follows ? Point{2 * current.x - lastControl.x, 2 * current.y - lastControl.y} : current;
            const Point p2{base.x + a, base.y + b};
            quadratic(*contour, current, p1, p2);
            lastControl = p1;
            current = p2;
            break;
        }
        case 'Z':
            current = start;
            contour = nullptr;  // a new subpath needs an M
            break;
        default:
            return false;  // arcs and anything else
        }
        last = upper;
        if (++commands > kMaxPoints / kCurveSteps || contours.size() > kMaxShapes) {
            return false;  // each command adds at most kCurveSteps points
        }
    }
    for (const auto& k : contours) {
        points += k.size();
    }
    return points <= kMaxPoints;
}

// one shape over the picture, source-over (nonzero winding, exact horizontal coverage, kSubRows rows a pixel)
void fill(SvgPicture& picture, const Shape& shape, double scale, double offsetX, double offsetY) {
    struct Edge {
        double x0, y0, x1, y1;
        int dir;
    };
    std::vector<Edge> edges;
    for (const auto& contour : shape.contours) {
        for (std::size_t i = 0; i < contour.size(); ++i) {
            const Point a = contour[i];
            const Point b = contour[(i + 1) % contour.size()];
            const double ay = (a.y - offsetY) * scale, by = (b.y - offsetY) * scale;
            if (ay == by) {
                continue;
            }
            edges.push_back({(a.x - offsetX) * scale, ay, (b.x - offsetX) * scale, by, ay < by ? 1 : -1});
        }
    }
    std::vector<float> coverage(static_cast<std::size_t>(picture.width) + 2);
    std::vector<std::pair<double, int>> crossings;
    for (int y = 0; y < picture.height; ++y) {
        std::fill(coverage.begin(), coverage.end(), 0.0f);
        for (int s = 0; s < kSubRows; ++s) {
            const double sy = y + (s + 0.5) / kSubRows;
            crossings.clear();
            for (const Edge& e : edges) {
                const double lo = std::min(e.y0, e.y1), hi = std::max(e.y0, e.y1);
                if (sy < lo || sy >= hi) {
                    continue;
                }
                const double t = (sy - e.y0) / (e.y1 - e.y0);
                crossings.push_back({e.x0 + t * (e.x1 - e.x0), e.dir});
            }
            std::sort(crossings.begin(), crossings.end());
            int winding = 0;
            for (std::size_t i = 0; i + 1 < crossings.size(); ++i) {
                winding += crossings[i].second;
                if (winding == 0) {
                    continue;
                }
                const double from = std::max(0.0, crossings[i].first);
                const double to = std::min(static_cast<double>(picture.width), crossings[i + 1].first);
                for (int x = static_cast<int>(std::floor(from)); x < to && x < picture.width; ++x) {
                    const double covered = std::min(to, x + 1.0) - std::max(from, static_cast<double>(x));
                    if (covered > 0) {
                        coverage[static_cast<std::size_t>(x)] += static_cast<float>(covered / kSubRows);
                    }
                }
            }
        }
        for (int x = 0; x < picture.width; ++x) {
            const float a = std::min(1.0f, coverage[static_cast<std::size_t>(x)]);
            if (a <= 0) {
                continue;
            }
            std::uint8_t* p = picture.pixels.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(picture.width) + static_cast<std::size_t>(x)) * 4;
            const float keep = 1 - a;
            p[0] = static_cast<std::uint8_t>(std::lround(shape.r * a + p[0] * keep));
            p[1] = static_cast<std::uint8_t>(std::lround(shape.g * a + p[1] * keep));
            p[2] = static_cast<std::uint8_t>(std::lround(shape.b * a + p[2] * keep));
            p[3] = static_cast<std::uint8_t>(std::lround(255 * a + p[3] * keep));
        }
    }
}

}  // namespace

std::optional<SvgPicture> rasterizeSvg(std::string_view svg, int height) {
    if (svg.empty() || svg.size() > kMaxText || height <= 0 || height > kMaxHeight) {
        return std::nullopt;
    }
    std::vector<Shape> shapes;
    double vx = 0, vy = 0, vw = 0, vh = 0;
    std::size_t points = 0;
    std::size_t at = 0;
    while ((at = svg.find('<', at)) != std::string_view::npos) {
        const std::size_t end = svg.find('>', at);
        if (end == std::string_view::npos) {
            break;
        }
        const std::string_view tag = svg.substr(at, end - at);
        at = end + 1;
        const auto named = [&](std::string_view name) {
            return tag.size() > name.size() + 1 && tag.substr(1, name.size()) == name &&
                   (tag[name.size() + 1] == ' ' || tag[name.size() + 1] == '\t' || tag[name.size() + 1] == '\n' ||
                    tag[name.size() + 1] == '\r' || tag[name.size() + 1] == '/');
        };
        if (named("svg")) {
            const auto box = attribute(tag, "viewBox");
            if (!box) {
                return std::nullopt;
            }
            Numbers n(*box);
            const auto a = n.number(), b = n.number(), c = n.number(), d = n.number();
            if (!a || !b || !c || !d || *c <= 0 || *d <= 0) {
                return std::nullopt;
            }
            vx = *a, vy = *b, vw = *c, vh = *d;
            continue;
        }
        const bool path = named("path");
        const bool polygon = named("polygon");
        if (!path && !polygon) {
            continue;  // groups, comments, metadata
        }
        if (attribute(tag, "transform") || attribute(tag, "style") || attribute(tag, "fill-rule") ||
            attribute(tag, "clip-path")) {
            return std::nullopt;  // drawing it plainly would be wrong
        }
        const auto fillValue = attribute(tag, "fill");
        if (fillValue && *fillValue == "none") {
            continue;
        }
        auto shape = colour(fillValue ? *fillValue : "#000000");
        if (!shape || shapes.size() >= kMaxShapes) {
            return std::nullopt;
        }
        if (path) {
            const auto d = attribute(tag, "d");
            if (!d || !parsePath(*d, shape->contours, points)) {
                return std::nullopt;
            }
        } else {
            const auto list = attribute(tag, "points");
            if (!list) {
                return std::nullopt;
            }
            Numbers n(*list);
            std::vector<Point> contour;
            while (!n.atEnd()) {
                const auto x = n.number(), y = n.number();
                if (!x || !y || contour.size() >= kMaxPoints) {
                    return std::nullopt;
                }
                contour.push_back({*x, *y});
            }
            shape->contours.push_back(std::move(contour));
        }
        shapes.push_back(std::move(*shape));
    }
    if (vw <= 0 || shapes.empty()) {
        return std::nullopt;
    }
    const double scale = static_cast<double>(height) / vh;
    const double width = std::ceil(vw * scale);
    if (width < 1 || width > kMaxWidth) {
        return std::nullopt;
    }
    SvgPicture picture;
    picture.width = static_cast<int>(width);
    picture.height = height;
    picture.pixels.assign(static_cast<std::size_t>(picture.width) * static_cast<std::size_t>(picture.height) * 4, 0);
    for (const Shape& shape : shapes) {
        fill(picture, shape, scale, vx, vy);
    }
    return picture;
}

}  // namespace playtube::ui
