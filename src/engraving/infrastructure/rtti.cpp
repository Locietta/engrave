#include "rtti.h"

#include <string>

#include "global/asserts.h"

namespace mu::engraving {
void report_bad_cast(const char *from, const char *to, const char *file, int line) {
    muse::diagnostics::assertion_failure(std::string("[BAD CAST] from: ") + from + ", to: " + to, file, line);
}
} // namespace mu::engraving
