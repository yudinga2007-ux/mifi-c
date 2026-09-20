#include <iostream>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <stdexcept>

using std::cin, std::cout, std::endl, std::ostream, std::string;

namespace ComplexNumbers {

constexpr double EPS = 1e-9;
constexpr double PI  = 3.14159265358979323846;

class IComplex {
public:
    virtual ~IComplex() = default;

    virtual double getRe() const = 0;
    virtual double getIm() const = 0;
    virtual double getModulus() const = 0;
    virtual double getArgument() const = 0;

    virtual std::unique_ptr<IComplex> clone() const = 0;

    virtual std::unique_ptr<IComplex> operator+(const IComplex& other) const = 0;
    virtual std::unique_ptr<IComplex> operator-(const IComplex& other) const = 0;
    virtual std::unique_ptr<IComplex> operator-() const = 0;
    virtual std::unique_ptr<IComplex> operator*(const IComplex& other) const = 0;
    virtual std::unique_ptr<IComplex> operator/(const IComplex& other) const = 0;
    virtual bool operator==(const IComplex& other) const = 0;
    bool operator!=(const IComplex& other) const { return !(*this == other); }

    virtual string serialize() const = 0;
    static std::unique_ptr<IComplex> deserialize(const string& s);
};

ostream& operator<<(ostream& os, const IComplex& c) {
    return os << c.serialize();
}

class AlgebraicComplex : public IComplex {
private:
    double re, im;
public:
    AlgebraicComplex(double r = 0.0, double i = 0.0) : re(r), im(i) {}

    double getRe() const override { return re; }
    double getIm() const override { return im; }
    double getModulus() const override { return std::sqrt(re * re + im * im); }
    double getArgument() const override { return std::atan2(im, re); }

    std::unique_ptr<IComplex> clone() const override {
        return std::make_unique<AlgebraicComplex>(re, im);
    }

    std::unique_ptr<IComplex> operator+(const IComplex& o) const override {
        return std::make_unique<AlgebraicComplex>(re + o.getRe(), im + o.getIm());
    }
    std::unique_ptr<IComplex> operator-(const IComplex& o) const override {
        return std::make_unique<AlgebraicComplex>(re - o.getRe(), im - o.getIm());
    }
    std::unique_ptr<IComplex> operator-() const override {
        return std::make_unique<AlgebraicComplex>(-re, -im);
    }
    std::unique_ptr<IComplex> operator*(const IComplex& o) const override {
        return std::make_unique<AlgebraicComplex>( //67
            re * o.getRe() - im * o.getIm(),
            re * o.getIm() + im * o.getRe());
    }
    std::unique_ptr<IComplex> operator/(const IComplex& o) const override {
        if (o.getModulus() < EPS)
            throw std::invalid_argument("Ошибность");
        double dr = o.getRe(), di = o.getIm();
        double denom = dr * dr + di * di;
        return std::make_unique<AlgebraicComplex>(
            (re * dr + im * di) / denom,
            (im * dr - re * di) / denom);
    }
    bool operator==(const IComplex& o) const override {
        return std::fabs(re - o.getRe()) < EPS && std::fabs(im - o.getIm()) < EPS;
    }

    string serialize() const override {
        return "alg(" + std::to_string(re) + ", " + std::to_string(im) + ")";
    }
};

class TrigonometricComplex : public IComplex {
private:
    double r, phi;
public:
    TrigonometricComplex(double modulus = 0.0, double argument = 0.0)
        : r(modulus), phi(argument) {}

    double getRe() const override { return r * std::cos(phi); }
    double getIm() const override { return r * std::sin(phi); }
    double getModulus() const override { return r; }
    double getArgument() const override { return phi; }

    std::unique_ptr<IComplex> clone() const override {
        return std::make_unique<TrigonometricComplex>(r, phi);
    }

    std::unique_ptr<IComplex> operator+(const IComplex& o) const override {
        double nr = getRe() + o.getRe();
        double ni = getIm() + o.getIm();
        return std::make_unique<TrigonometricComplex>(
            std::sqrt(nr * nr + ni * ni), std::atan2(ni, nr));
    }
    std::unique_ptr<IComplex> operator-(const IComplex& o) const override {
        auto neg = -o;
        return *this + *neg;
    }
    std::unique_ptr<IComplex> operator-() const override {
        return std::make_unique<TrigonometricComplex>(r, phi + PI);
    }
    std::unique_ptr<IComplex> operator*(const IComplex& o) const override {
        return std::make_unique<TrigonometricComplex>(
            r * o.getModulus(), phi + o.getArgument());
    }
    std::unique_ptr<IComplex> operator/(const IComplex& o) const override {
        if (o.getModulus() < EPS)
            throw std::invalid_argument("Ошибность");
        return std::make_unique<TrigonometricComplex>(
            r / o.getModulus(), phi - o.getArgument());
    }
    bool operator==(const IComplex& o) const override {
        return std::fabs(getRe() - o.getRe()) < EPS &&
               std::fabs(getIm() - o.getIm()) < EPS;
    }

    string serialize() const override {
        return "trig(" + std::to_string(r) + ", " + std::to_string(phi) + ")";
    }
};

std::unique_ptr<IComplex> IComplex::deserialize(const string& s) {
    size_t p = s.find('('), q = s.find(')');
    if (p == string::npos || q == string::npos || p > q)
        throw std::invalid_argument("Неверная сериализованная строка: " + s);
    string body = s.substr(p + 1, q - p - 1);
    size_t comma = body.find(',');
    if (comma == string::npos)
        throw std::invalid_argument("Неверная сериализованная строка: " + s);
    double x = std::stod(body.substr(0, comma));
    double y = std::stod(body.substr(comma + 1));
    if (s.compare(0, p, "alg") == 0)  return std::make_unique<AlgebraicComplex>(x, y);
    if (s.compare(0, p, "trig") == 0) return std::make_unique<TrigonometricComplex>(x, y);
    throw std::invalid_argument("Неизвестный тип тэга: " + s.substr(0, p));
}

std::pair<std::unique_ptr<IComplex>, std::unique_ptr<IComplex>>
complex_sqrt(const IComplex& z) {
    auto first = std::make_unique<TrigonometricComplex>(
        std::sqrt(z.getModulus()), z.getArgument() / 2.0);
    auto second = -*first;
    return { std::move(first), std::move(second) };
}

std::pair<std::unique_ptr<IComplex>, std::unique_ptr<IComplex>>
solveQuadratic(const IComplex& a, const IComplex& b, const IComplex& c) {
    auto b_sq = b * b;

    AlgebraicComplex four = 4;
    auto four_ac = four * a;
    four_ac = *four_ac * c;
    auto D = *b_sq - *four_ac;

    auto [sqrt1, sqrt2] = complex_sqrt(*D);

    auto neg_b = -b;
    AlgebraicComplex two = 2;
    auto two_a = two * a;

    auto num1 = *neg_b + *sqrt1;
    auto num2 = *neg_b + *sqrt2;

    return { *num1 / *two_a, *num2 / *two_a };
}

}

int main() {
    using namespace ComplexNumbers;
    cout << std::boolalpha;

    cout << "ДЗ-1 комплексные числа и квадратное уравнение" << endl;

    auto z1 = std::make_unique<AlgebraicComplex>(3.0, 4.0);
    auto z2 = std::make_unique<TrigonometricComplex>(2.0, PI / 4.0);

    cout << "z1      = " << *z1 << endl;
    cout << "z2      = " << *z2 << endl;

    auto sum  = *z1 + *z2;
    auto prod = *z1 * *z2;
    auto neg  = -*z1;
    auto quot = *z1 / *z2;
    cout << "z1 + z2 = " << *sum  << endl;
    cout << "z1 * z2 = " << *prod << endl;
    cout << "-z1     = " << *neg  << endl;
    cout << "z1 / z2 = " << *quot << endl;
    cout << "z1 == z1: " << (*z1 == *z1) << ", z1 != z2: " << (*z1 != *z2) << endl;

    string s = z1->serialize();
    cout << "serialize(z1)   = \"" << s << "\"" << endl;
    auto z1_copy = IComplex::deserialize(s);
    cout << "deserialize(s)  = " << *z1_copy
         << ", равно z1: " << (*z1_copy == *z1) << endl;

    cout << "\nx^2 - 2x + 5 = 0" << endl;
    AlgebraicComplex a = 1, b = -2, c = 5; 
    auto [r1, r2] = solveQuadratic(a, b, c);
    cout << "корень 1 = " << *r1 << endl;
    cout << "корень 2 = " << *r2 << endl;

    cout << "\nx^2 + (1+i)x + i = 0" << endl;
    AlgebraicComplex a2 = 1, b2(1, 1), c2(0, 1);
    auto [r3, r4] = solveQuadratic(a2, b2, c2);
    cout << "корень 1 = " << *r3 << endl;
    cout << "корень 2 = " << *r4 << endl;

    cout << "\nДеление на ноль" << endl;
    AlgebraicComplex zero = 0;
    try {
        auto bad = *z1 / zero;
        cout << *bad << endl;
    } catch (const std::exception& e) {
        cout << "Ошибка: " << e.what() << endl;
    }

    return 0;
}