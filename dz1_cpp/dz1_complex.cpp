#include <iostream>
#include <cmath>
#include <memory>
#include <vector>

using std::cin, std::cout, std::endl, std::ostream;

namespace ComplexNumbers {

    class IComplex {
    public:

        virtual ~IComplex() = default;

        virtual double getRe() const = 0;
        virtual double getIm() const = 0;

        virtual double getModulus() const = 0;
        virtual double getArgument() const = 0;

        virtual std::unique_ptr<IComplex> clone() const = 0;

        virtual std::unique_ptr<IComplex> add(const IComplex& other) const = 0;
        virtual std::unique_ptr<IComplex> sub(const IComplex& other) const = 0;
        virtual std::unique_ptr<IComplex> mul(const IComplex& other) const = 0;
        
        virtual void print(ostream& os) const = 0;
    };

    ostream& operator<<(ostream& os, const IComplex& c) {
        c.print(os);
        return os;
    }

    class AlgebraicComplex : public IComplex {
    private:
        double re;
        double im;

    public:

        AlgebraicComplex(double r = 0.0, double i = 0.0) : re(r), im(i) {}

        double getRe() const override { return re; }
        double getIm() const override { return im; }

        double getModulus() const override { return std::sqrt(re*re + im*im); }
        double getArgument() const override { return std::atan2(im, re); }

        std::unique_ptr<IComplex> clone() const override {
            return std::make_unique<AlgebraicComplex>(re, im);
        }

        std::unique_ptr<IComplex> add(const IComplex& other) const override {
            return std::make_unique<AlgebraicComplex>(this->re + other.getRe(), this->im + other.getIm());
        }

        std::unique_ptr<IComplex> sub(const IComplex& other) const override {
            return std::make_unique<AlgebraicComplex>(this->re - other.getRe(), this->im - other.getIm());
        }

        std::unique_ptr<IComplex> mul(const IComplex& other) const override {
            double newRe = this->re * other.getRe() - this->im * other.getIm();
            double newIm = this->re * other.getIm() + this->im * other.getRe();
            return std::make_unique<AlgebraicComplex>(newRe, newIm);
        }

        void print(ostream& os) const override {
            os << "(" << re << " + " << im << "i) [Алгебраическая]";
        }
    };

    class TrigonometricComplex : public IComplex {
    private:
        double r;
        double phi;

    public:
        TrigonometricComplex(double modulus = 0.0, double argument = 0.0) : r(modulus), phi(argument) {}

        double getRe() const override { return r * std::cos(phi); }
        double getIm() const override { return r * std::sin(phi); }
        
        double getModulus() const override { return r; }
        double getArgument() const override { return phi; }

        std::unique_ptr<IComplex> clone() const override {
            return std::make_unique<TrigonometricComplex>(r, phi);
        }

        std::unique_ptr<IComplex> add(const IComplex& other) const override {
            double newRe = this->getRe() + other.getRe();
            double newIm = this->getIm() + other.getIm();
            return std::make_unique<AlgebraicComplex>(newRe, newIm);
        }

        std::unique_ptr<IComplex> sub(const IComplex& other) const override {
            double newRe = this->getRe() - other.getRe();
            double newIm = this->getIm() - other.getIm();
            return std::make_unique<AlgebraicComplex>(newRe, newIm);
        }

        std::unique_ptr<IComplex> mul(const IComplex& other) const override {
            double newR = this->r * other.getModulus();
            double newPhi = this->phi + other.getArgument();
            return std::make_unique<TrigonometricComplex>(newR, newPhi);
        }

        void print(ostream& os) const override {
            os << r << " * (cos(" << phi << ") + i*sin(" << phi << ")) [Тригонометрическая]";
        }
    };

    std::vector<std::unique_ptr<IComplex>> solveQuadratic(const IComplex& a, const IComplex& b, const IComplex& c) {
        std::vector<std::unique_ptr<IComplex>> roots;

        auto b_sq = b.mul(b); 

        AlgebraicComplex four(4.0, 0.0);
        auto four_a = four.mul(a); 

        auto four_ac = four_a->mul(c);

        auto D = b_sq->sub(*four_ac);

        double d_re = D->getRe();
        double d_im = D->getIm();

        double mod_D = std::sqrt(d_re*d_re + d_im*d_im);
        double arg_D = std::atan2(d_im, d_re);

        double sqrt_mod = std::sqrt(mod_D);
        double sqrt_arg = arg_D / 2.0;

        auto sqrt_D_1 = std::make_unique<TrigonometricComplex>(sqrt_mod, sqrt_arg);
        auto sqrt_D_2 = std::make_unique<TrigonometricComplex>(sqrt_mod, sqrt_arg + M_PI);

        AlgebraicComplex zero(0.0, 0.0);
        auto minus_b = zero.sub(b);

        AlgebraicComplex two(2.0, 0.0);
        auto two_a = two.mul(a);

        auto divide = [](const IComplex& n, const IComplex& d) -> std::unique_ptr<IComplex> {
            double nr = n.getRe(), ni = n.getIm();
            double dr = d.getRe(), di = d.getIm();
            double denom = dr*dr + di*di;
            if (denom == 0) return std::make_unique<AlgebraicComplex>(0,0); // Защита от деления на 0
            double resRe = (nr*dr + ni*di) / denom;
            double resIm = (ni*dr - nr*di) / denom;
            return std::make_unique<AlgebraicComplex>(resRe, resIm);
        };

        auto num1 = minus_b->add(*sqrt_D_1);
        roots.push_back(divide(*num1, *two_a));

        auto num2 = minus_b->add(*sqrt_D_2);
        roots.push_back(divide(*num2, *two_a));

        return roots;
    }
}

int main() {
    using namespace ComplexNumbers;

    cout << "--- ДЗ-1: Комплексные числа и Квадратное уравнение ---" << endl;

    auto z1 = std::make_unique<AlgebraicComplex>(3.0, 4.0);
    
    auto z2 = std::make_unique<TrigonometricComplex>(2.0, M_PI / 4.0);

    cout << "z1: " << *z1 << endl;
    cout << "z2: " << *z2 << endl;

    auto sum = z1->add(*z2);
    cout << "z1 + z2 = " << *sum << endl;

    auto prod = z1->mul(*z2);
    cout << "z1 * z2 = " << *prod << endl;

    cout << "\n--- Решение x^2 - 2x + 5 = 0 ---" << endl;
    
    AlgebraicComplex a(1.0, 0.0);
    AlgebraicComplex b(-2.0, 0.0);
    AlgebraicComplex c(5.0, 0.0);

    auto roots = solveQuadratic(a, b, c);

    cout << "Корень 1: " << *roots[0] << endl;
    cout << "Корень 2: " << *roots[1] << endl;

    cout << "\n--- Решение x^2 + (1+i)x + i = 0 ---" << endl;
    AlgebraicComplex a2(1.0, 0.0);
    AlgebraicComplex b2(1.0, 1.0);
    AlgebraicComplex c2(0.0, 1.0);

    auto roots2 = solveQuadratic(a2, b2, c2);
    cout << "Корень 1: " << *roots2[0] << endl;
    cout << "Корень 2: " << *roots2[1] << endl;

    return 0;
}