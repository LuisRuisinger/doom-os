#include "kernel/mm/pmm.hpp"

#include <concepts>

#include "kernel/core/bitmap.hpp"
#include "kernel/core/bits.hpp"
#include "kernel/core/cast.hpp"
#include "kernel/core/reflect.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/sync/spinlock.hpp"

namespace kernel::mm::pmm {

namespace {

using kernel::core::Err;
using kernel::core::Ok;
using kernel::core::u16;
using kernel::core::u8;
using kernel::core::uptr;
using kernel::core::utils::align_down;
using kernel::core::utils::align_up;
using kernel::core::utils::bitmap;
using kernel::core::utils::is_aligned;

using frame_map = bitmap<MAX_FRAMES>;

constexpr usize FRAMES_PER_BLOCK = frames_in(page_size::SIZE_2M);
constexpr usize WORDS_PER_BLOCK = FRAMES_PER_BLOCK / frame_map::BITS_PER_STORAGE_TYPE_INSTANCE;
constexpr usize BLOCK_COUNT = MAX_FRAMES / FRAMES_PER_BLOCK;
constexpr u16   NIL = ~u16{0};

static_assert(BLOCK_COUNT < NIL && FRAMES_PER_BLOCK <= NIL);

struct block {
    u16 free;
    u16 next;
    u16 prev;
};

frame_map m_frames{};
block     m_blocks[BLOCK_COUNT]{};
u16       m_whole{NIL};
u16       m_broken{NIL};

// TODO
// finer locking approach for more performance
kernel::sync::spinlock m_lock{};

extern "C" char boot_start[];
extern "C" char kernel_physical_end[];

[[nodiscard]] paddr_t address_of(usize frame)
{
    return frame as(paddr_t) << FRAME_SHIFT;
}

[[nodiscard]] u16 *list_of(usize free)
{
    if (free == 0)
        return nullptr;

    if (free == FRAMES_PER_BLOCK)
        return &m_whole;

    return &m_broken;
}

void push(u16 &head, usize index)
{
    m_blocks[index].next = head;
    m_blocks[index].prev = NIL;

    if (head != NIL)
        m_blocks[head].prev = index as(u16);

    head = index as(u16);
}

void unlink(u16 &head, usize index)
{
    const block &b = m_blocks[index];

    if (b.prev == NIL)
        head = b.next;
    else
        m_blocks[b.prev].next = b.next;

    if (b.next != NIL)
        m_blocks[b.next].prev = b.prev;
}

void set_free(usize index, usize free)
{
    u16 *from = list_of(m_blocks[index].free);
    u16 *to = list_of(free);

    if (from != to) {
        if (from != nullptr)
            unlink(*from, index);

        if (to != nullptr)
            push(*to, index);
    }

    m_blocks[index].free = free as(u16);
}

[[nodiscard]] paddr_t take_whole(usize first, usize count)
{
    m_frames.clear(first * FRAMES_PER_BLOCK, count * FRAMES_PER_BLOCK);

    for (usize i = 0; i < count; ++i)
        set_free(first + i, 0);

    return address_of(first * FRAMES_PER_BLOCK);
}

template <usize Blocks>
[[nodiscard]] usize find_whole_run()
{
    if constexpr (Blocks == 1)
        return m_whole;

    for (usize first = 0; first < BLOCK_COUNT; first += Blocks) {
        usize whole = 0;

        while (whole < Blocks && m_blocks[first + whole].free == FRAMES_PER_BLOCK)
            ++whole;

        if (whole == Blocks)
            return first;
    }

    return NIL;
}

template <page_size S>
[[nodiscard]] Result<paddr_t, mm_error> take()
{
    if constexpr (S == page_size::SIZE_4K) {
        const u16 index = m_broken != NIL ? m_broken : m_whole;

        if (index == NIL)
            return Err(mm_error::OUT_OF_MEMORY);

        const usize frame = m_frames.find_set(index * WORDS_PER_BLOCK, WORDS_PER_BLOCK);

        if (frame == frame_map::NPOS)
            KPANIC("pmm: block {} is listed as free but holds nothing", index);

        m_frames.clear(frame);
        set_free(index, m_blocks[index].free - 1);

        return Ok(address_of(frame));
    } else {
        constexpr usize BLOCKS = frames_in(S) / FRAMES_PER_BLOCK;
        const usize     first = find_whole_run<BLOCKS>();

        if (first == NIL)
            return Err(mm_error::OUT_OF_MEMORY);

        return Ok(take_whole(first, BLOCKS));
    }
}

void mark(paddr_t base, u64 length, bool usable)
{
    if (length == 0 || base >= MAX_PHYSICAL_MEMORY)
        return;

    const paddr_t end = length > MAX_PHYSICAL_MEMORY - base ? MAX_PHYSICAL_MEMORY : base + length;
    const paddr_t first =
        usable ? align_up<paddr_t>(base, FRAME_SIZE) : align_down<paddr_t>(base, FRAME_SIZE);
    const paddr_t last =
        usable ? align_down<paddr_t>(end, FRAME_SIZE) : align_up<paddr_t>(end, FRAME_SIZE);

    if (first >= last)
        return;

    if (usable)
        m_frames.set(first >> FRAME_SHIFT, (last - first) >> FRAME_SHIFT);
    else
        m_frames.clear(first >> FRAME_SHIFT, (last - first) >> FRAME_SHIFT);
}

enum class phase : u8 {
    FREE,
    RESERVE,
};

void claim(std::same_as<bool> auto, phase)
{
}

template <usize N>
void claim(const kernel::boot::bounded_string<N> &, phase)
{
}

void claim(const kernel::boot::address_range &range, phase p)
{
    if (p == phase::RESERVE)
        mark(range.base, range.length, false);
}

void claim(const kernel::boot::memory_region &region, phase p)
{
    const bool usable = region.kind == kernel::boot::memory_kind::USABLE;

    if ((p == phase::FREE) == usable)
        mark(region.base, region.length, usable);
}

void claim(const kernel::boot::framebuffer_info &framebuffer, phase p)
{
    if (p == phase::RESERVE && framebuffer.present)
        mark(framebuffer.address, framebuffer.pitch as(u64) * framebuffer.height, false);
}

void claim(const kernel::boot::acpi_info &acpi, phase p)
{
    if (p == phase::RESERVE && acpi.present)
        mark(acpi.rsdp, 36, false);
}

template <typename E, usize N>
void claim(const kernel::boot::fixed_table<E, N> &table, phase p)
{
    for (const auto &entry : table)
        claim(entry, p);
}

template <reflect::reflectable T>
void claim(const T &aggregate, phase p)
{
    reflect::apply_fields(aggregate, [&](const auto &...fields) { (claim(fields, p), ...); });
}

void populate(const kernel::boot::info &boot)
{
    claim(boot, phase::FREE);

    mark(0, 1024 * 1024, false);
    mark(boot_start as(uptr), kernel_physical_end as(uptr) - boot_start as(uptr), false);

    claim(boot, phase::RESERVE);

    for (usize index = 0; index < BLOCK_COUNT; ++index)
        set_free(index, m_frames.count_set(index * WORDS_PER_BLOCK, WORDS_PER_BLOCK));
}

}  // namespace

Result<paddr_t, mm_error> alloc(page_size size)
{
    kernel::sync::spinlock_guard guard(m_lock);

    switch (size) {
        case page_size::SIZE_4K:
            return take<page_size::SIZE_4K>();
        case page_size::SIZE_2M:
            return take<page_size::SIZE_2M>();
        case page_size::SIZE_1G:
            return take<page_size::SIZE_1G>();
    }

    return Err(mm_error::UNSUPPORTED_SIZE);
}

void free(page_size size, paddr_t base)
{
    const usize count = frames_in(size);
    const usize first = base >> FRAME_SHIFT;

    if (!is_aligned<paddr_t>(base, bytes_in(size)) || first >= MAX_FRAMES ||
        count > MAX_FRAMES - first)
        KPANIC("pmm: invalid release of {} frames at {:#018X}", count, base);

    kernel::sync::spinlock_guard guard(m_lock);

    if (m_frames.find_set_in(first, count) != frame_map::NPOS)
        KPANIC("pmm: double free of {} frames at {:#018X}", count, base);

    m_frames.set(first, count);

    const usize index = first / FRAMES_PER_BLOCK;

    if (count < FRAMES_PER_BLOCK) {
        set_free(index, m_blocks[index].free + count);
        return;
    }

    for (usize i = 0; i < count / FRAMES_PER_BLOCK; ++i)
        set_free(index + i, FRAMES_PER_BLOCK);
}

kernel::init::init_result component::init_allocator()
{
    using kernel::init::init_error;

    const auto &boot = kernel::boot::boot_info::current();

    if (!boot.valid)
        return Err(init_error::DEPENDENCY_UNAVAILABLE);

    populate(boot);

    if (m_whole == NIL && m_broken == NIL)
        return Err(init_error::NO_USABLE_MEMORY);

    return Ok();
}

}  // namespace kernel::mm::pmm
