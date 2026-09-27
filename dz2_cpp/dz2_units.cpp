#include <array>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdio>
#include <ctime>
#include <format>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace Velichiny {

struct AmperTag  { static constexpr const char* unit = "A"; };
struct VoltTag   { static constexpr const char* unit = "V"; };
struct OhmTag    { static constexpr const char* unit = "Ohm"; };
struct JouleTag  { static constexpr const char* unit = "J"; };
struct WattTag   { static constexpr const char* unit = "W"; };
struct SecondTag { static constexpr const char* unit = "s"; };

template <class Tag>
struct Velichina {
    double value{};
    static constexpr const char* unit() noexcept { return Tag::unit; }
};

using Amper = Velichina<AmperTag>;
using Volt = Velichina<VoltTag>;
using Ohm = Velichina<OhmTag>;
using Joule = Velichina<JouleTag>;
using Watt = Velichina<WattTag>;
using Second = Velichina<SecondTag>;

namespace details {

inline double safe_div(double a, double b) {
    if (b == 0.0) throw std::invalid_argument("Division by zero quantity");
    return a / b;
}

template <class T>
T make(double v) {
    if (!std::isfinite(v)) throw std::domain_error("Non finite quantity value");
    return T{v};
}

}

template <class Tag> Velichina<Tag> operator+(Velichina<Tag> l, Velichina<Tag> r) { return details::make<Velichina<Tag>>(l.value + r.value); }
template <class Tag> Velichina<Tag> operator-(Velichina<Tag> l, Velichina<Tag> r) { return details::make<Velichina<Tag>>(l.value - r.value); }
template <class Tag> Velichina<Tag> operator-(Velichina<Tag> x) { return Velichina<Tag>{-x.value}; }
template <class Tag> bool operator==(Velichina<Tag> l, Velichina<Tag> r) { return l.value == r.value; }
template <class Tag> bool operator!=(Velichina<Tag> l, Velichina<Tag> r) { return !(l == r); }
template <class Tag> bool operator<(Velichina<Tag> l, Velichina<Tag> r) { return l.value < r.value; }

inline Volt operator*(Amper i, Ohm r) { return details::make<Volt>(i.value * r.value); }
inline Volt operator*(Ohm r, Amper i) { return i * r; }
inline Ohm operator/(Volt u, Amper i) { return details::make<Ohm>(details::safe_div(u.value, i.value)); }
inline Amper operator/(Volt u, Ohm r) { return details::make<Amper>(details::safe_div(u.value, r.value)); }
inline Watt operator*(Volt u, Amper i) { return details::make<Watt>(u.value * i.value); }
inline Watt operator*(Amper i, Volt u) { return u * i; }
inline Amper operator/(Watt p, Volt u) { return details::make<Amper>(details::safe_div(p.value, u.value)); }
inline Volt operator/(Watt p, Amper i) { return details::make<Volt>(details::safe_div(p.value, i.value)); }
inline Joule operator*(Watt p, Second t) { return details::make<Joule>(p.value * t.value); }
inline Joule operator*(Second t, Watt p) { return p * t; }
inline Watt operator/(Joule e, Second t) { return details::make<Watt>(details::safe_div(e.value, t.value)); }
inline Second operator/(Joule e, Watt p) { return details::make<Second>(details::safe_div(e.value, p.value)); }

constexpr Amper operator""_mA(unsigned long long x) { return Amper{static_cast<double>(x) / 1000.0}; }
constexpr Amper operator""_mA(long double x) { return Amper{static_cast<double>(x) / 1000.0}; }
constexpr Volt operator""_V(unsigned long long x) { return Volt{static_cast<double>(x)}; }
constexpr Volt operator""_V(long double x) { return Volt{static_cast<double>(x)}; }
constexpr Ohm operator""_Ohm(unsigned long long x) { return Ohm{static_cast<double>(x)}; }
constexpr Ohm operator""_Ohm(long double x) { return Ohm{static_cast<double>(x)}; }
constexpr Joule operator""_J(unsigned long long x) { return Joule{static_cast<double>(x)}; }
constexpr Joule operator""_J(long double x) { return Joule{static_cast<double>(x)}; }
constexpr Watt operator""_W(unsigned long long x) { return Watt{static_cast<double>(x)}; }
constexpr Watt operator""_W(long double x) { return Watt{static_cast<double>(x)}; }
constexpr Second operator""_s(unsigned long long x) { return Second{static_cast<double>(x)}; }
constexpr Second operator""_s(long double x) { return Second{static_cast<double>(x)}; }

}

namespace Pribory {

template <class V>
class Izmerenie {
private:
    std::chrono::system_clock::time_point vremya_;
    V znachenie_;
    double pogreshnost_;
public:
    Izmerenie(std::chrono::system_clock::time_point t, V znachenie, double pogreshnost)
        : vremya_(t), znachenie_(znachenie), pogreshnost_(pogreshnost)
    {
        if (!std::isfinite(pogreshnost_) || pogreshnost_ < 0.0)
            throw std::invalid_argument("Izmerenie: pogreshnosti must be non negative finite");
        if (!std::isfinite(znachenie_.value))
            throw std::invalid_argument("Izmerenie: value must be finite");
    }
    auto vremya() const { return vremya_; }
    V znachenie() const { return znachenie_; }
    double pogreshnost() const { return pogreshnost_; }
};

inline std::string time_to_string(std::chrono::system_clock::time_point tp) {
    const std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    const std::tm* gmt = std::gmtime(&tt);
    if (gmt == nullptr) throw std::runtime_error("gmtime failed");
    char buf[80];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02dZ",
                  gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday,
                  gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
    return buf;
}

class Pribor {
public:
    Pribor(double min, double max, double klass, std::string id)
        : min_(min), max_(max), klass_(klass), id_(std::move(id))
    {
        if (!std::isfinite(min_) || !std::isfinite(max_) || !(min_ < max_))
            throw std::invalid_argument("Pribor: badboy diapazon (min < max expected)");
        bool ok = false;
        for (double k : klassy_gost())
            if (std::fabs(klass_ - k) < 1e-9) ok = true;
        if (!ok) throw std::invalid_argument("Pribor: klass tochnosti not in GOST row");
        if (id_.empty()) throw std::invalid_argument("Pribor: empty identifier");
    }
    virtual ~Pribor() = default;

protected:
    struct Otchet {
        std::chrono::system_clock::time_point vremya;
        double znachenie;
    };
    Otchet snyat() const {
        static std::mt19937 gen{std::random_device{}()};
        std::uniform_real_distribution<double> dist(min_, max_);
        return Otchet{std::chrono::system_clock::now(), dist(gen)};
    }
    double pogreshnost() const { return klass_ / 100.0 * (max_ - min_); }

private:
    double min_, max_, klass_;
    std::string id_;
    static const std::array<double, 8>& klassy_gost() {
        static const std::array<double, 8> k{0.05, 0.1, 0.2, 0.5, 1.0, 1.5, 2.5, 4.0};
        return k;
    }
};

class Voltmetr : public Pribor {
public:
    using Pribor::Pribor;
    Izmerenie<Velichiny::Volt> izmerit() const {
        auto [t, v] = snyat();
        return Izmerenie<Velichiny::Volt>{t, Velichiny::Volt{v}, pogreshnost()};
    }
};

class Ammetr : public Pribor {
public:
    using Pribor::Pribor;
    Izmerenie<Velichiny::Amper> izmerit() const {
        auto [t, v] = snyat();
        return Izmerenie<Velichiny::Amper>{t, Velichiny::Amper{v}, pogreshnost()};
    }
};

class Multimetr : public Pribor {
public:
    using Pribor::Pribor;
    template <class V>
        requires (std::same_as<V, Velichiny::Volt> ||
                  std::same_as<V, Velichiny::Amper> ||
                  std::same_as<V, Velichiny::Ohm>)
    Izmerenie<V> izmerit() const {
        auto [t, v] = snyat();
        return Izmerenie<V>{t, V{v}, pogreshnost()};
    }
};

}

template <class Tag>
struct std::formatter<Velichiny::Velichina<Tag>> {
    std::formatter<double> double_formatter;
    constexpr auto parse(std::format_parse_context& ctx) { return double_formatter.parse(ctx); }
    auto format(const Velichiny::Velichina<Tag>& z, std::format_context& ctx) const {
        auto out = double_formatter.format(z.value, ctx);
        return std::format_to(out, " {}", Velichiny::Velichina<Tag>::unit());
    }
};

template <class V>
struct std::formatter<Pribory::Izmerenie<V>> {
    std::formatter<double> double_formatter;
    constexpr auto parse(std::format_parse_context& ctx) { return double_formatter.parse(ctx); }
    auto format(const Pribory::Izmerenie<V>& m, std::format_context& ctx) const {
        auto out = double_formatter.format(m.znachenie().value, ctx);
        out = std::format_to(out, " +/- ");
        out = double_formatter.format(m.pogreshnost(), ctx);
        return std::format_to(out, " {} @ {}", V::unit(), Pribory::time_to_string(m.vremya()));
    }
};

void demo_velichiny() {
    using namespace Velichiny;
    std::cout << "--- Velichiny: (nado bilo setlocate delat a ne eto), operators, physical laws ---\n";
    auto i = 500_mA;
    auto r = 4_Ohm;
    auto u = i * r;
    auto p = u * i;
    auto t = 90_s;
    auto e = p * t;
    std::cout << std::format("i = {}, r = {}\n", i, r);
    std::cout << std::format("u = i*r = {}, p = u*i = {}, e = p*t = {}\n", u, p, e);
    std::cout << std::format("u/i = {}, u/r = {}, e/t = {}, e/p = {}\n", u / i, u / r, e / t, e / p);
    std::cout << std::format("i + i = {}, -i = {}\n", i + i, -i);
    std::cout << std::format("u == 2_V: {}, u != 3_V: {}, i < 1000_mA: {}\n",
                             u == 2_V, u != 3_V, i < 1000_mA);
    try {
        auto bad = 10_V / 0_mA;
        (void)bad;
    } catch (const std::exception& ex) {
        std::cout << "Caught: " << ex.what() << '\n';
    }
    try {
        auto huge = 1e308_V + 1e308_V;
        (void)huge;
    } catch (const std::exception& ex) {
        std::cout << "Caught: " << ex.what() << '\n';
    }
}

void demo_pribory() {
	using namespace Velichiny;
    std::cout << "\n--- Pribory: measurement, GOST accuracy class ---\n";
    Pribory::Voltmetr v{0.0, 300.0, 1.5, "V7-1"};
    Pribory::Ammetr a{0.0, 2.0, 0.5, "A5-2"};
    Pribory::Multimetr m{0.0, 200.0, 2.5, "MM-3"};
    std::cout << std::format("V7-1: {}\n", v.izmerit());
    std::cout << std::format("A5-2: {}\n", a.izmerit());
    std::cout << std::format("MM-3 (Ohm): {}\n", m.izmerit<Ohm>());
    std::cout << std::format("precision demo: {:.2}\n", v.izmerit());
    try {
        Pribory::Voltmetr bad{300.0, 0.0, 1.5, "X"};
        (void)bad;
    } catch (const std::exception& ex) {
        std::cout << "Caught: " << ex.what() << '\n';
    }
    try {
        Pribory::Voltmetr bad2{0.0, 10.0, 3.3, "Y"};
        (void)bad2;
    } catch (const std::exception& ex) {
        std::cout << "Caught: " << ex.what() << '\n';
    }
}

void demo_agregaty() {
    using namespace Velichiny;
    std::cout << "\n--- Aggregates ---\n";
    static_assert(std::is_aggregate_v<Amper>);
    static_assert(std::is_aggregate_v<Volt>);
    static_assert(std::is_aggregate_v<Ohm>);
    static_assert(std::is_aggregate_v<Joule>);
    static_assert(std::is_aggregate_v<Watt>);
    static_assert(std::is_aggregate_v<Second>);
    static_assert(!std::is_aggregate_v<Pribory::Voltmetr>);
    static_assert(!std::is_aggregate_v<Pribory::Ammetr>);
    static_assert(!std::is_aggregate_v<Pribory::Multimetr>);
    static_assert(!std::is_aggregate_v<Pribory::Izmerenie<Volt>>);
    Amper x{1.5};
    std::cout << std::format("aggregate init: x = {}\n", x);
    std::cout << "Velichiny are aggregates; Pribor and Izmerenie are not.\n";
}

int main() {
    demo_velichiny();
    demo_pribory();
    demo_agregaty();
    return 0;
}