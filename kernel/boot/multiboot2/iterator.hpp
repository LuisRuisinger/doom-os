#ifndef DOOM_OS_KERNEL_BOOT_MULTIBOOT2_ITERATOR_HPP_
#define DOOM_OS_KERNEL_BOOT_MULTIBOOT2_ITERATOR_HPP_

// =================================================================================================
// Standard library
// =================================================================================================

#include <cstddef>
#include <iterator>
#include <ranges>

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/multiboot2/spec.hpp"
#include "kernel/core/bits.hpp"
#include "kernel/core/cast.hpp"
#include "kernel/core/types.hpp"

namespace kernel::boot::multiboot2 {

using kernel::core::uptr;

[[nodiscard]] inline bool well_formed(const fixed_header &header)
{
    const auto *begin = (&header) as(const u8 *);
    if (header.total_size < sizeof(fixed_header))
        return false;

    if (header.total_size > (~uptr{0}) as(usize) - begin as(uptr))
        return false;

    const auto *end = begin + header.total_size;
    const auto *at = begin + sizeof(fixed_header);

    while ((end - at) as(usize) >= sizeof(tag_header)) {
        const auto *tag = at as(const tag_header *);
        const auto  remaining = (end - at) as(usize);

        if (tag->size < sizeof(tag_header) || tag->size > remaining)
            return false;

        if (tag->type as(tag_type) == tag_type::END)
            return tag->size == sizeof(tag_header);

        const auto step = kernel::core::utils::align_up<usize>(tag->size, TAG_ALIGNMENT);
        if (step > remaining)
            return false;

        at += step;
    }

    return false;
}

class tag_iterator {
public:
    using value_type = tag_header;
    using difference_type = std::ptrdiff_t;

    explicit tag_iterator(const tag_header *tag)
        : m_tag{tag}
    {
    }

    const tag_header &operator*() const
    {
        return *m_tag;
    }

    tag_iterator &operator++()
    {
        const auto step = kernel::core::utils::align_up<usize>(m_tag->size, TAG_ALIGNMENT);

        m_tag = (m_tag as(const u8 *) + step) as(const tag_header *);
        return *this;
    }

    tag_iterator operator++(int)
    {
        const tag_iterator previous = *this;

        ++*this;
        return previous;
    }

    bool operator==(std::default_sentinel_t /*unused*/) const
    {
        return m_tag->type as(tag_type) == tag_type::END;
    }

private:
    const tag_header *m_tag;
};

static_assert(std::input_iterator<tag_iterator>);
static_assert(std::sentinel_for<std::default_sentinel_t, tag_iterator>);

// The following code assumes the provided tag sequence is well formed
// This is checked as part of required functions during boot component initialization

inline std::ranges::subrange<tag_iterator, std::default_sentinel_t> tags(const fixed_header &header)
{
    const auto *first = ((&header) as(const u8 *) + sizeof(fixed_header)) as(const tag_header *);

    return {tag_iterator{first}, std::default_sentinel};
}

inline auto entries(const memory_map_tag &tag)
{
    const auto *fst = (&tag) as(const u8 *) + sizeof(memory_map_tag);
    const usize cnt = (tag.header.size - sizeof(memory_map_tag)) / tag.entry_size;
    const u32   stride = tag.entry_size;

    return std::views::iota(usize{0}, cnt) |
           std::views::transform([fst, stride](usize i) -> const memory_map_entry & {
               return *((fst + (i * stride)) as(const memory_map_entry *));
           });
}

}  // namespace kernel::boot::multiboot2

#endif  // DOOM_OS_KERNEL_BOOT_MULTIBOOT2_ITERATOR_HPP_
