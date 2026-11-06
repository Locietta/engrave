#include "version.h"

#include "global/appinfo.h"

namespace music {
const char *engine_version() { return muse::appinfo::version_string(); }
} // namespace music
