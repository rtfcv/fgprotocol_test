/**
 * @file check.h
 * @brief Minimal assertion macros for CTest-registered executables.
 *
 * No framework dependency: a failed CHECK prints the failing expression
 * and location and exits non-zero, which is all CTest needs to mark the
 * test failed.
 */
#ifndef JSBSIM_TESTER_TESTS_CHECK_H
#define JSBSIM_TESTER_TESTS_CHECK_H

#include <cmath>
#include <cstdlib>
#include <iostream>

/**
 * @def CHECK(cond)
 * @brief Fails the test (prints and exits 1) unless `cond` is true.
 * @param cond Boolean expression to assert.
 */
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "CHECK failed: " #cond "\n  at " << __FILE__      \
                       << ":" << __LINE__ << "\n";                         \
            std::exit(1);                                                  \
        }                                                                  \
    } while (0)

/**
 * @def CHECK_EQ(a, b)
 * @brief Fails the test unless `a == b`, printing both values on failure.
 * @param a Left-hand value.
 * @param b Right-hand value.
 */
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

/**
 * @def CHECK_NEAR(a, b, eps)
 * @brief Fails the test unless `a` and `b` are within `eps` of each other.
 * @param a Left-hand value (converted to `double`).
 * @param b Right-hand value (converted to `double`).
 * @param eps Maximum allowed absolute difference.
 */
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
