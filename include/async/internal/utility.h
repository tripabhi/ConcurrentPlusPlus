#pragma once

#include <cstddef>

namespace async {
namespace internal {
inline constexpr std::size_t ALIGNMENT = 2 * sizeof(std::max_align_t);
}
} // namespace async