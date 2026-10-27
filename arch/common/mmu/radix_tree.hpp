#ifndef DOOM_OS_ARCH_COMMON_MMU_RADIX_TREE_HPP_
#define DOOM_OS_ARCH_COMMON_MMU_RADIX_TREE_HPP_

#include "arch/common/mmu/traits.hpp"
#include "kernel/core/array.hpp"
#include "kernel/core/result.hpp"
#include "kernel/core/types.hpp"
#include "kernel/mm/page.hpp"

namespace kernel::arch::common::mmu {

using kernel::core::Err;
using kernel::core::Ok;
using kernel::mm::bytes_in;
using kernel::mm::mapping;

template <page_format F, page_env E>
class radix_tree {
    paddr_t m_root{};

    struct ignore_removed {
        void operator()(const mapping &) const
        {
        }
    };

    struct walk_path {
        kernel::core::utils::array<u64 *, F::LEVELS + 1>   slots;
        kernel::core::utils::array<paddr_t, F::LEVELS + 1> tables;
        usize                                              reached_level;
    };

    [[nodiscard]] static page_size leaf_size(page_size size)
    {
        usize level = F::level_of(size);

        while (level > 1 && !F::supports(F::size_at(level)))
            --level;

        return F::size_at(level);
    }

    [[nodiscard]] static Result<usize, mm_error> validate(vaddr_t va, page_size size,
                                                          paddr_t pa = 0)
    {
        if (!F::valid(va) || ((va | pa) & (bytes_in(size) - 1)) != 0)
            return Err(mm_error::INVALID_ADDRESS);

        return Ok(F::level_of(size));
    }

    [[nodiscard]] static bool is_table_empty(const u64 *table)
    {
        for (usize i = 0; i < F::ENTRIES; ++i)
            if (table[i] != 0)
                return false;

        return true;
    }

    [[nodiscard]] Result<walk_path, mm_error> walk(vaddr_t va, usize target, bool allocate) const
    {
        walk_path path{};
        paddr_t   table = m_root;

        for (usize level = F::LEVELS;; --level) {
            u64 &slot = E::at(table)[F::index(level, va)];

            path.slots[level] = &slot;
            path.tables[level] = table;
            path.reached_level = level;

            if (level == target)
                return Ok(path);

            if (!F::present(slot)) {
                if (!allocate)
                    return Err(mm_error::NOT_MAPPED);

                E::replace(slot, F::table_entry(KTRY(E::alloc_table())), va);
            } else if (F::leaf(slot, level)) {
                if (!allocate)
                    return Ok(path);

                const paddr_t child = KTRY(E::alloc_table());

                F::split(E::at(child), slot, level);
                E::replace(slot, F::table_entry(child), va);
            }

            table = F::address(slot);
        }
    }

    [[nodiscard]] Result<walk_path, mm_error> walk_to_leaf(vaddr_t va, usize target) const
    {
        walk_path path = KTRY(walk(va, target, false));

        if (path.reached_level > target)
            path = KTRY(walk(va, target, true));

        if (!F::present(*path.slots[target]))
            return Err(mm_error::NOT_MAPPED);

        if (!F::leaf(*path.slots[target], target))
            return Err(mm_error::CONFLICT);

        return Ok(path);
    }

    void prune(const walk_path &path, usize from_level, vaddr_t va) const
    {
        for (usize level = from_level; level < F::LEVELS; ++level) {
            if (!is_table_empty(E::at(path.tables[level])))
                return;

            E::replace(*path.slots[level + 1], 0, va);
            E::free_table(path.tables[level]);
        }
    }

    [[nodiscard]] Result<void, mm_error> map_leaf(vaddr_t va, paddr_t pa, page_size size,
                                                  page_prot prot) const
    {
        const usize     target = KTRY(validate(va, size, pa));
        const walk_path path = KTRY(walk(va, target, true));
        const u64       entry = F::leaf_entry(pa, size, prot);
        u64            &slot = *path.slots[target];

        if (F::present(slot) && slot != entry)
            return Err(mm_error::CONFLICT);

        if (!F::present(slot))
            E::replace(slot, entry, va);

        return Ok();
    }

    [[nodiscard]] Result<void, mm_error> protect_leaf(vaddr_t va, page_size size,
                                                      page_prot prot) const
    {
        const usize     target = KTRY(validate(va, size));
        const walk_path path = KTRY(walk_to_leaf(va, target));
        u64            &slot = *path.slots[target];

        E::replace(slot, F::leaf_entry(F::address(slot), size, prot), va);

        return Ok();
    }

    [[nodiscard]] Result<void, mm_error> split_at(vaddr_t boundary, usize target) const
    {
        const auto path = walk(boundary, target, false);

        if (path.is_err())
            return Ok();

        const usize level = path.unwrap_ref().reached_level;

        if (level > target && (boundary & (bytes_in(F::size_at(level)) - 1)) != 0)
            KTRY(walk(boundary, target, true));

        return Ok();
    }

    [[nodiscard]] Result<mapping, mm_error> remove_leaf(vaddr_t va) const
    {
        if (!F::valid(va))
            return Err(mm_error::INVALID_ADDRESS);

        const walk_path path = KTRY(walk(va, 1, false));
        const usize     level = path.reached_level;
        u64            &slot = *path.slots[level];

        if (!F::present(slot))
            return Err(mm_error::NOT_MAPPED);

        const mapping removed{F::address(slot), 0, F::size_at(level), F::prot(slot)};

        E::replace(slot, 0, va);
        prune(path, level, va);

        return Ok(removed);
    }

    template <class Fn>
    [[nodiscard]] Result<void, mm_error> for_each_leaf(vaddr_t va, u64 length, page_size size,
                                                       Fn fn) const
    {
        size = leaf_size(size);

        const u64 step = bytes_in(size);

        if (length == 0 || length % step != 0)
            return Err(mm_error::INVALID_ADDRESS);

        for (u64 offset = 0; offset < length; offset += step)
            KTRY(fn(va + offset, offset, size));

        return Ok();
    }

public:
    constexpr radix_tree() = default;

    explicit constexpr radix_tree(paddr_t root)
        : m_root(root)
    {
    }

    [[nodiscard]] constexpr paddr_t root() const
    {
        return m_root;
    }

    [[nodiscard]] Result<void, mm_error> map_range(vaddr_t va, paddr_t pa, u64 length,
                                                   page_size size, page_prot prot) const
    {
        return for_each_leaf(va, length, size, [&](vaddr_t leaf, u64 offset, page_size leaf_sz) {
            return map_leaf(leaf, pa + offset, leaf_sz, prot);
        });
    }

    [[nodiscard]] Result<void, mm_error> protect_range(vaddr_t va, u64 length, page_size size,
                                                       page_prot prot) const
    {
        return for_each_leaf(va, length, size, [&](vaddr_t leaf, u64, page_size leaf_sz) {
            return protect_leaf(leaf, leaf_sz, prot);
        });
    }

    template <class Fn = ignore_removed>
    [[nodiscard]] Result<void, mm_error> unmap_range(vaddr_t va, u64 length, page_size size,
                                                     Fn on_removed = {}) const
    {
        size = leaf_size(size);

        const usize   target = KTRY(validate(va, size));
        const vaddr_t end = va + length;

        if (length == 0 || length % bytes_in(size) != 0 || end - 1 < va || !F::valid(end - 1))
            return Err(mm_error::INVALID_ADDRESS);

        KTRY(split_at(va, target));

        if (end > va && F::valid(end))
            KTRY(split_at(end, target));

        for (u64 offset = 0; offset < length;) {
            const mapping removed = KTRY(remove_leaf(va + offset));

            on_removed(removed);
            offset += bytes_in(removed.size);
        }

        return Ok();
    }

    [[nodiscard]] Result<mapping, mm_error> translate(vaddr_t va) const
    {
        if (!F::valid(va))
            return Err(mm_error::INVALID_ADDRESS);

        const walk_path path = KTRY(walk(va, 1, false));
        const usize     level = path.reached_level;
        const u64       slot = *path.slots[level];

        if (!F::present(slot))
            return Err(mm_error::NOT_MAPPED);

        const page_size size = F::size_at(level);

        return Ok<mapping>(F::address(slot), va & (bytes_in(size) - 1), size, F::prot(slot));
    }
};

}  // namespace kernel::arch::common::mmu

#endif  // DOOM_OS_ARCH_COMMON_MMU_RADIX_TREE_HPP_
