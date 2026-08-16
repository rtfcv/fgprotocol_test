#ifndef JSBSIM_TESTER_TESTS_CHECK_H
#define JSBSIM_TESTER_TESTS_CHECK_H

#include <cmath>
#include <cstdlib>
#include <iostream>

// Minimal assertion macros for CTest-registered executables. No framework
// dependency: a failed CHECK prints the failing expression and location and
// exits non-zero, which is all CTest needs to mark the test failed.
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "CHECK failed: " #cond "\n  at " << __FILE__      \
                       << ":" << __LINE__ << "\n";                         \
            std::exit(1);                                                  \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                     \
    do {                                                                   \
        auto a_ = (a);                                                    \
        auto b_ = (b);                                                    \
        if (!(a_ == b_)) {                                                 \
            std::cerr << "CHECK_EQ failed: " #a " == " #b "\n  " << a_     \
                       << " != " << b_ << "\n  at " << __FILE__ << ":"     \
                       << __LINE__ << "\n";                                \
            std::exit(1);                                                  \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                              \
    do {                                                                   \
        double a_ = (a);                                                  \
        double b_ = (b);                                                  \
        double d_ = std::fabs(a_ - b_);                                    \
        if (!(d_ <= (eps))) {                                              \
            std::cerr << "CHECK_NEAR failed: " #a " ~= " #b "\n  " << a_   \
                       << " vs " << b_ << " (diff " << d_ << " > " << (eps) \
                       << ")\n  at " << __FILE__ << ":" << __LINE__        \
                       << "\n";                                            \
            std::exit(1);                                                  \
        }                                                                  \
    } while (0)

#endif // JSBSIM_TESTER_TESTS_CHECK_H
