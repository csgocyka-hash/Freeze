#include "Check.h"
#include <cstring>

// Runs every registered test, or only those whose name contains argv[1].
int main (int argc, char** argv)
{
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failed = 0;

    for (auto& test : check::registry())
    {
        if (filter != nullptr && std::strstr (test.name, filter) == nullptr)
            continue;

        const int before = check::failures();
        std::printf ("[ RUN  ] %s\n", test.name);
        test.fn();
        ++run;
        const bool ok = check::failures() == before;
        failed += ok ? 0 : 1;
        std::printf ("[ %s ] %s\n", ok ? " OK " : "FAIL", test.name);
    }

    std::printf ("\n%d/%d tests passed\n", run - failed, run);
    return failed == 0 && run > 0 ? 0 : 1;
}
