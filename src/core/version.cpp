#include "atpl/core/version.hpp"

#define ATPL_STRINGIFY_IMPL(x) #x
#define ATPL_STRINGIFY(x) ATPL_STRINGIFY_IMPL(x)
#define ATPL_VERSION_STRING                                                                                            \
    ATPL_STRINGIFY(ATPL_VERSION_MAJOR) "." ATPL_STRINGIFY(ATPL_VERSION_MINOR) "." ATPL_STRINGIFY(ATPL_VERSION_PATCH)

namespace atpl {

Version version() {
    return { ATPL_VERSION_MAJOR, ATPL_VERSION_MINOR, ATPL_VERSION_PATCH };
}

std::string_view versionString() {
    return ATPL_VERSION_STRING;
}

} // namespace atpl
