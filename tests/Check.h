#pragma once
// Minimal test harness: TEST registers a function, CHECK / CHECK_NEAR record failures.
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace check
{
struct TestCase
{
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failures()
{
    static int count = 0;
    return count;
}

struct Registrar
{
    Registrar (const char* name, std::function<void()> fn) { registry().push_back ({ name, std::move (fn) }); }
};

inline void fail (const char* file, int line, const std::string& message)
{
    std::printf ("    FAIL %s:%d: %s\n", file, line, message.c_str());
    ++failures();
}
} // namespace check

#define CHECK_CAT2(a, b) a##b
#define CHECK_CAT(a, b) CHECK_CAT2 (a, b)

#define TEST(name)                                                      \
    static void name();                                                 \
    static check::Registrar CHECK_CAT (registrar_, name) (#name, name); \
    static void name()

#define CHECK(cond) \
    do { if (! (cond)) check::fail (__FILE__, __LINE__, #cond); } while (0)

#define CHECK_NEAR(a, b, tol)                                                            \
    do {                                                                                 \
        const double va_ = (a), vb_ = (b);                                               \
        if (! (std::abs (va_ - vb_) <= (tol)))                                           \
            check::fail (__FILE__, __LINE__, std::string (#a " ~= " #b "  (")            \
                                                 + std::to_string (va_) + " vs "         \
                                                 + std::to_string (vb_) + ")");          \
    } while (0)
