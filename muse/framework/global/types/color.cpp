#include "color.h"

#include <iomanip>
#include <sstream>

namespace muse {
static constexpr int from_hex(char c);
static int hex2int(const char *s, int n);
static bool get_hex_rgb(const char *name, size_t len, Rgba *rgba);
static std::string rgb2hex(int r, int g, int b, int a = Color::default_alpha);
static std::string rgb2hex(Rgba rgba);
static void insert_hex_component(int num, std::stringstream &ss);

Color::Color() { is_valid_ = false; }

Color::Color(const Color &other) : rgba_(other.rgba_), is_valid_(other.is_valid_) {}

Color::Color(const char *color) { set_named_color(color); }

Color &Color::operator=(const Color &other) {
    rgba_ = other.rgba_;
    is_valid_ = other.is_valid_;
    return *this;
}

bool Color::operator==(const Color &other) const { return rgba_ == other.rgba_ && is_valid_ == other.is_valid_; }

bool Color::operator!=(const Color &other) const { return !operator==(other); }

bool Color::operator<(const Color &other) const { return rgba_ < other.rgba_; }

std::string Color::to_string() const { return rgb2hex(rgba_); }

Color Color::from_string(const std::string &str) {
    Color c;
    c.set_named_color(str);
    return c;
}

Color Color::from_string(const char *str) {
    Color c;
    c.set_named_color(str);
    return c;
}

Color Color::from_string(const String &str) { return from_string(str.to_std_string()); }

bool Color::is_valid() const { return is_valid_; }

int Color::red() const { return get_red(rgba_); }

int Color::green() const { return get_green(rgba_); }

int Color::blue() const { return get_blue(rgba_); }

int Color::alpha() const { return get_alpha(rgba_); }

void Color::set_named_color(const std::string &color) {
    if (color.empty()) {
        is_valid_ = false;
        return;
    }

    Rgba rgba;
    if (color[0] == '#') {
        if (get_hex_rgb(color.data(), color.size(), &rgba)) {
            set_rgba(rgba);
            is_valid_ = true;
            return;
        }
    }

    is_valid_ = false;
}

void Color::set_named_color(const char *color) {
    if (!color) {
        return;
    }
    set_named_color(std::string(color));
}

void Color::set_red(int value) { set_rgba(value, green(), blue(), alpha()); }

void Color::set_green(int value) { set_rgba(red(), value, blue(), alpha()); }

void Color::set_blue(int value) { set_rgba(red(), green(), value, alpha()); }

void Color::set_alpha(int value) { set_rgba(red(), green(), blue(), value); }

void Color::apply_tint(double tint) {
    set_rgba(red() + (255 - red()) * tint, green() + (255 - green()) * tint, blue() + (255 - blue()) * tint, alpha());
}

void Color::set_rgba(int r, int g, int b, int a) {
    if (!is_rgba_valid(r, g, b, a)) {
        is_valid_ = false;
        return;
    }

    rgba_ = rgba(r, g, b, a);
    is_valid_ = true;
}

void Color::set_rgba(Rgba rgba) {
    if (!is_rgba_valid(get_red(rgba), get_green(rgba), get_blue(rgba), get_alpha(rgba))) {
        is_valid_ = false;
        return;
    }

    rgba_ = rgba;
    is_valid_ = true;
}

Color Color::inverted() const {
    int m = std::min(red() < green() ? red() : green(), blue());
    int M = std::max(red() > green() ? red() : green(), blue());
    int x = 255 - m - M;
    return Color(red() + x, green() + x, blue() + x, alpha());
}

static constexpr int from_hex(char c) {
    return ((c >= '0') && (c <= '9')) ? int(c - '0') :
           ((c >= 'A') && (c <= 'F')) ? int(c - 'A' + 10) :
           ((c >= 'a') && (c <= 'f')) ? int(c - 'a' + 10) :
                                        -1;
}

static int hex2int(const char *s, int n) {
    if (n < 0) {
        return -1;
    }
    int result = 0;
    for (; n > 0; --n) {
        result = result * 16;
        const int h = from_hex(*s++);
        if (h < 0) {
            return -1;
        }
        result += h;
    }
    return result;
}

static bool get_hex_rgb(const char *name, size_t len, Rgba *rgba) {
    if (name[0] != '#') {
        return false;
    }
    name++;
    --len;
    int a, r, g, b;
    a = 255;
    if (len == 8) {
        a = hex2int(name + 0, 2);
        r = hex2int(name + 2, 2);
        g = hex2int(name + 4, 2);
        b = hex2int(name + 6, 2);
    } else if (len == 6) {
        r = hex2int(name + 0, 2);
        g = hex2int(name + 2, 2);
        b = hex2int(name + 4, 2);
    } else if (len == 3) {
        r = hex2int(name + 0, 1) * 0x11;
        g = hex2int(name + 1, 1) * 0x11;
        b = hex2int(name + 2, 1) * 0x11;
    } else {
        r = g = b = -1;
    }

    if (!is_rgba_valid(r, g, b, a)) {
        *rgba = 0;
        return false;
    }

    *rgba = muse::rgba(r, g, b, a);
    return true;
}

static void insert_hex_component(int num, std::stringstream &ss) {
    ss << std::uppercase << std::setw(2) << std::setfill('0') << std::hex << num;
}

static std::string rgb2hex(int r, int g, int b, int a) {
    std::stringstream ss;

    ss << "#";
    insert_hex_component(r, ss);
    insert_hex_component(g, ss);
    insert_hex_component(b, ss);
    if (a != Color::default_alpha) {
        insert_hex_component(a, ss);
    }
    return ss.str();
}

static std::string rgb2hex(Rgba rgba) { return rgb2hex(get_red(rgba), get_green(rgba), get_blue(rgba), get_alpha(rgba)); }
} // namespace muse
