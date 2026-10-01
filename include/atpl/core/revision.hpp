#pragma once

#include <cstdint>

namespace atpl {

/// A change counter. Shared values (`Param`, `Series`, `Snapshot`) carry one; it only ever grows.
///
/// A reader remembers the revision it saw last and compares it later to learn whether there is
/// anything new, without reading or comparing the value itself. This is what lets the UI skip
/// all work for values that did not change.
using Revision = std::uint64_t;

} // namespace atpl
