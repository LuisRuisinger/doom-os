// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/memory/pmm/pmm.hpp"

#include "kernel/core/bitmap.hpp"
#include "kernel/core/bits.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/sync/spinlock.hpp"

namespace kernel::core::memory::pmm {

namespace {

using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::uptr;
using kernel::core::usize;
using kernel::core::utils::align_down;
using kernel::core::utils::align_up;
using kernel::core::utils::all_bits;
using kernel::core::utils::bit_width;
using kernel::core::utils::bitmap;
using kernel::core::utils::clear_bit;
using kernel::core::utils::is_aligned;

// =================================================================================================
// Layout
//
// A frame bitmap, and two lists over the 2 MiB blocks it describes: whole blocks, and blocks
// with some frames free but not all. Allocation is a list head - never a search - and single
// frames come off a broken block before a whole one is opened, so a 4 KiB request does not
// shatter a block a 2 MiB request could still use.
//
// The only thing tracked per block is how many of its frames are free, which is what makes
// "did that release just complete a block" a comparison rather than a scan of its bitmap.
// =================================================================================================

using frame_map = bitmap<MAX_FRAMES>;

constexpr usize BITS_PER_WORD = frame_map::BITS_PER_STORAGE_TYPE_INSTANCE;
constexpr usize NPOS = frame_map::NPOS;

constexpr usize WORDS_PER_BLOCK = FRAMES_PER_2M / BITS_PER_WORD;
constexpr usize BLOCK_COUNT = MAX_FRAMES / FRAMES_PER_2M;
constexpr usize BLOCKS_PER_1G = FRAMES_PER_1G / FRAMES_PER_2M;
constexpr usize GIB_COUNT = MAX_FRAMES / FRAMES_PER_1G;

constexpr u16 NIL = ~u16{0};

static_assert(FRAMES_PER_2M % BITS_PER_WORD == 0);
static_assert(BLOCK_COUNT < NIL, "a block index has to fit in a link");
static_assert(FRAMES_PER_2M <= NIL, "a per block free count has to fit in a u16");

// =================================================================================================
// Indices
//
// Frames and blocks are both counts of things and would convert into each other without a
// murmur. They do not: the only way across is a named conversion.
// =================================================================================================

template <typename Tag>
struct index {
    usize raw{};

    constexpr index() = default;
    explicit constexpr index(usize value)
        : raw(value)
    {
    }

    [[nodiscard]] constexpr index operator+(usize count) const
    {
        return index{raw + count};
    }

    constexpr index &operator++()
    {
        ++raw;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const index &) const = default;
    [[nodiscard]] constexpr auto operator<=>(const index &) const = default;
};

using frame_id = index<struct frame_tag>;
using block_id = index<struct block_tag>;

[[nodiscard]] constexpr block_id block_of(frame_id frame)
{
    return block_id{frame.raw / FRAMES_PER_2M};
}

[[nodiscard]] constexpr frame_id first_frame_of(block_id block)
{
    return frame_id{block.raw * FRAMES_PER_2M};
}

[[nodiscard]] constexpr frame_id frame_at(paddr_t base)
{
    return frame_id{static_cast<usize>(base >> FRAME_SHIFT)};
}

[[nodiscard]] constexpr paddr_t address_of(frame_id frame)
{
    return static_cast<paddr_t>(frame.raw) << FRAME_SHIFT;
}

// =================================================================================================
// State
// =================================================================================================

struct allocator_state {
    frame_map frames{};  // 1 = free. Ground truth.

    u16 free_in[BLOCK_COUNT]{};  // free frames per 2 MiB block
    u16 next[BLOCK_COUNT]{};     // intrusive links; a block is on at most one list
    u16 prev[BLOCK_COUNT]{};

    u16 whole_head{NIL};
    u16 broken_head{NIL};

    usize managed_frames{};
    usize free_frames{};

    bool initialized{};

    kernel::sync::spinlock lock{};
};

allocator_state g_allocator{};

// Both symbols carry physical addresses: the image spans the identity mapped boot sections
// through the end of the higher-half image.
extern "C" char boot_start[];
extern "C" char kernel_physical_end[];

// =================================================================================================
// Block lists
//
// Doubly linked, because a block leaves the middle of a list as soon as one frame is taken out
// of it or put back into it.
// =================================================================================================

[[nodiscard]] u16 *list_for(usize free_count)
{
    if (free_count == 0)
        return nullptr;

    return free_count == FRAMES_PER_2M ? &g_allocator.whole_head : &g_allocator.broken_head;
}

void list_insert(u16 &head, block_id block)
{
    g_allocator.prev[block.raw] = NIL;
    g_allocator.next[block.raw] = head;

    if (head != NIL)
        g_allocator.prev[head] = static_cast<u16>(block.raw);

    head = static_cast<u16>(block.raw);
}

void list_remove(u16 &head, block_id block)
{
    const u16 next = g_allocator.next[block.raw];
    const u16 prev = g_allocator.prev[block.raw];

    if (prev == NIL)
        head = next;
    else
        g_allocator.next[prev] = next;

    if (next != NIL)
        g_allocator.prev[next] = prev;

    g_allocator.next[block.raw] = NIL;
    g_allocator.prev[block.raw] = NIL;
}

// The one place a block's free count changes, and therefore the one place it moves between
// lists. Everything else calls this and stays out of the links.
void set_free_count(block_id block, usize now)
{
    const usize before = g_allocator.free_in[block.raw];

    if (before == now)
        return;

    u16 *from = list_for(before);
    u16 *to = list_for(now);

    if (from != to) {
        if (from != nullptr)
            list_remove(*from, block);

        if (to != nullptr)
            list_insert(*to, block);
    }

    g_allocator.free_in[block.raw] = static_cast<u16>(now);
}

// =================================================================================================
// Allocation
// =================================================================================================

// Single frames, drained a whole word at a time so the word is loaded once, cleared in a
// register and stored once however many of its bits the request wanted. Broken blocks first,
// which is the whole of the anti-fragmentation policy.
[[nodiscard]] usize take_frames_locked(usize count, paddr_t *out)
{
    usize taken = 0;

    while (taken < count) {
        const u16 head =
            g_allocator.broken_head != NIL ? g_allocator.broken_head : g_allocator.whole_head;

        if (head == NIL)
            break;

        const block_id block{head};
        const usize    base_word = block.raw * WORDS_PER_BLOCK;

        usize from_block = 0;

        for (usize i = 0; i < WORDS_PER_BLOCK && taken < count; ++i) {
            u64 &word = g_allocator.frames.word(base_word + i);
            u64  bits = word;

            if (bits == 0)
                continue;

            do {
                const u32 offset = static_cast<u32>(__builtin_ctzll(bits));

                bits = clear_bit(bits, offset);
                out[taken] = address_of(frame_id{(base_word + i) * BITS_PER_WORD + offset});

                ++taken;
                ++from_block;
            } while (bits != 0 && taken < count);

            word = bits;
        }

        // The block was chosen off a list that says it holds a free frame.
        if (from_block == 0)
            KPANIC("pmm: block {} is listed as free but holds nothing", block.raw);

        g_allocator.free_frames -= from_block;
        set_free_count(block, g_allocator.free_in[block.raw] - from_block);
    }

    return taken;
}

[[nodiscard]] paddr_t take_2m_locked()
{
    if (g_allocator.whole_head == NIL)
        return INVALID_PHYSICAL_ADDRESS;

    const block_id block{g_allocator.whole_head};
    const frame_id first = first_frame_of(block);

    g_allocator.frames.clear(first.raw, FRAMES_PER_2M);
    g_allocator.free_frames -= FRAMES_PER_2M;
    set_free_count(block, 0);

    return address_of(first);
}

// A gigabyte is 512 whole blocks in a row, which the lists cannot express, so it is looked for.
// Sixteen candidates that give up on their first used block is cheap enough for a request that
// hands out a gigabyte.
[[nodiscard]] paddr_t take_1g_locked()
{
    for (usize gib = 0; gib < GIB_COUNT; ++gib) {
        const block_id first_block{gib * BLOCKS_PER_1G};

        usize whole = 0;

        while (whole < BLOCKS_PER_1G &&
               g_allocator.free_in[first_block.raw + whole] == FRAMES_PER_2M)
            ++whole;

        if (whole != BLOCKS_PER_1G)
            continue;

        const frame_id first = first_frame_of(first_block);

        g_allocator.frames.clear(first.raw, FRAMES_PER_1G);
        g_allocator.free_frames -= FRAMES_PER_1G;

        for (usize i = 0; i < BLOCKS_PER_1G; ++i)
            set_free_count(first_block + i, 0);

        return address_of(first);
    }

    return INVALID_PHYSICAL_ADDRESS;
}

// Shared by every release path. Everything it rejects is a caller bug rather than a runtime
// condition, so it panics instead of reporting.
void release_locked(page_size size, paddr_t base)
{
    if (!g_allocator.initialized)
        KPANIC("pmm: release of {:#018X} before the allocator is initialised", base);

    if (!is_aligned<paddr_t>(base, bytes_in(size)))
        KPANIC("pmm: release of {:#018X}, which is not aligned to {} bytes", base, bytes_in(size));

    const usize    count = frames_in(size);
    const frame_id first = frame_at(base);

    if (first.raw >= g_allocator.managed_frames || count > g_allocator.managed_frames - first.raw)
        KPANIC("pmm: release of {} frames at {:#018X} leaves the managed range", count, base);

    // A frame in the range that is already free means the caller is handing back something it
    // does not hold.
    const usize already_free = g_allocator.frames.find_set_in(first.raw, count);

    if (already_free != NPOS)
        KPANIC("pmm: double free of frame {:#018X} in a {} frame release at {:#018X}",
               address_of(frame_id{already_free}), count, base);

    g_allocator.frames.set(first.raw, count);
    g_allocator.free_frames += count;

    const block_id block = block_of(first);

    if (count < FRAMES_PER_2M) {
        set_free_count(block, g_allocator.free_in[block.raw] + count);
        return;
    }

    for (usize i = 0; i < count / FRAMES_PER_2M; ++i)
        set_free_count(block + i, FRAMES_PER_2M);
}

// =================================================================================================
// Ingestion
//
// Passes over the boot description that depend neither on the order regions arrive in nor on
// their being disjoint. Marking is idempotent, so an overlap costs nothing.
// =================================================================================================

void reset_state()
{
    g_allocator.frames.reset();

    for (usize block = 0; block < BLOCK_COUNT; ++block) {
        g_allocator.free_in[block] = 0;
        g_allocator.next[block] = NIL;
        g_allocator.prev[block] = NIL;
    }

    g_allocator.whole_head = NIL;
    g_allocator.broken_head = NIL;
    g_allocator.free_frames = 0;
}

[[nodiscard]] paddr_t range_end(paddr_t base, u64 length)
{
    return length > all_bits<paddr_t>() - base ? all_bits<paddr_t>() : base + length;
}

// Usable memory rounds inward and everything else rounds outward, so a frame that is only
// partly usable is never handed out and a frame that is partly something else is never taken.
// The saturating align_up is because the boot description promises nothing about its own edges.
void mark_range(paddr_t base, u64 length, paddr_t limit, bool usable)
{
    if (length == 0)
        return;

    const auto align_up_saturating = [](paddr_t value) {
        return value > all_bits<paddr_t>() - (FRAME_SIZE - 1)
                   ? align_down<paddr_t>(all_bits<paddr_t>(), FRAME_SIZE)
                   : align_up<paddr_t>(value, FRAME_SIZE);
    };

    const paddr_t raw_end = range_end(base, length);
    const paddr_t first =
        usable ? align_up_saturating(base) : align_down<paddr_t>(base, FRAME_SIZE);

    paddr_t end = usable ? align_down<paddr_t>(raw_end, FRAME_SIZE) : align_up_saturating(raw_end);

    if (end > limit)
        end = limit;

    if (first >= end)
        return;

    const usize count = static_cast<usize>((end - first) >> FRAME_SHIFT);

    if (usable)
        g_allocator.frames.set(frame_at(first).raw, count);
    else
        g_allocator.frames.clear(frame_at(first).raw, count);
}

void ingest(const kernel::boot::info &boot, paddr_t limit)
{
    // Everything is occupied until a usable region says otherwise. Deriving free memory as the
    // complement of what was reserved would hand out any region the description lost.
    for (usize i = 0; i < boot.memory_map.count; ++i)
        if (boot.memory_map[i].kind == kernel::boot::memory_kind::USABLE)
            mark_range(boot.memory_map[i].base, boot.memory_map[i].length, limit, true);

    // Real mode IVT, BDA, EBDA, and whatever else the firmware still believes it owns.
    mark_range(0, 1024 * 1024, limit, false);

    const paddr_t image_first = static_cast<paddr_t>(reinterpret_cast<uptr>(boot_start));
    const paddr_t image_end = static_cast<paddr_t>(reinterpret_cast<uptr>(kernel_physical_end));

    if (image_end > image_first)
        mark_range(image_first, image_end - image_first, limit, false);

    for (usize i = 0; i < boot.reserved.count; ++i)
        mark_range(boot.reserved[i].base, boot.reserved[i].length, limit, false);

    // Module bytes were never copied out of where the bootloader put them.
    for (usize i = 0; i < boot.modules.count; ++i)
        mark_range(boot.modules[i].range.base, boot.modules[i].range.length, limit, false);

    // An MMIO aperture rather than RAM, and commonly absent from the memory map entirely.
    if (boot.framebuffer.present)
        mark_range(boot.framebuffer.address,
                   static_cast<u64>(boot.framebuffer.pitch) * boot.framebuffer.height, limit,
                   false);
}

// Counts and lists are a fold of the finished bitmap, so they are built once from it rather
// than maintained across ingestion.
void build_lists()
{
    for (usize i = 0; i < BLOCK_COUNT; ++i) {
        const block_id block{i};
        const usize    free = g_allocator.frames.count_set(i * WORDS_PER_BLOCK, WORDS_PER_BLOCK);

        set_free_count(block, free);
        g_allocator.free_frames += free;
    }
}

[[nodiscard]] paddr_t usable_ceiling(const kernel::boot::info &boot)
{
    paddr_t highest = 0;

    for (usize i = 0; i < boot.memory_map.count; ++i) {
        const auto &region = boot.memory_map[i];

        if (region.kind != kernel::boot::memory_kind::USABLE || region.length == 0)
            continue;

        const paddr_t end = range_end(region.base, region.length);

        if (end > highest)
            highest = end;
    }

    return highest > MAX_PHYSICAL_MEMORY ? MAX_PHYSICAL_MEMORY : highest;
}

}  // namespace

// =================================================================================================
// PMM API
// =================================================================================================

bool alloc_pages(page_size size, usize count, paddr_t *out)
{
    if (count == 0)
        return true;

    if (out == nullptr)
        return false;

    kernel::sync::spinlock_guard guard(g_allocator.lock);

    if (!g_allocator.initialized || count * frames_in(size) > g_allocator.free_frames)
        return false;

    if (size == page_size::SMALL_4K) {
        // Frames are counted, so the check above is exact and nothing below comes up short.
        const usize taken = take_frames_locked(count, out);

        if (taken != count)
            KPANIC("pmm: {} frames were free but only {} could be taken", count, taken);

        return true;
    }

    // Whole blocks are not counted - finding out how many exist costs the same as taking them -
    // so a large request is attempted and handed straight back if the machine came up short.
    for (usize taken = 0; taken < count; ++taken) {
        out[taken] = size == page_size::LARGE_2M ? take_2m_locked() : take_1g_locked();

        if (out[taken] != INVALID_PHYSICAL_ADDRESS)
            continue;

        for (usize i = 0; i < taken; ++i)
            release_locked(size, out[i]);

        return false;
    }

    return true;
}

void free_pages(page_size size, usize count, const paddr_t *pages)
{
    if (count == 0)
        return;

    if (pages == nullptr)
        KPANIC("pmm: release of {} pages of {} bytes from a null array", count, bytes_in(size));

    kernel::sync::spinlock_guard guard(g_allocator.lock);

    for (usize i = 0; i < count; ++i)
        release_locked(size, pages[i]);
}

paddr_t alloc_page(page_size size)
{
    paddr_t page = INVALID_PHYSICAL_ADDRESS;

    return alloc_pages(size, 1, &page) ? page : INVALID_PHYSICAL_ADDRESS;
}

void free_page(page_size size, paddr_t base)
{
    free_pages(size, 1, &base);
}

// =================================================================================================
// Component
// =================================================================================================

kernel::core::init_result component::init_allocator()
{
    using kernel::core::init_error;

    const auto &boot = kernel::boot::boot_info::current();

    if (!boot.valid)
        return kernel::core::Err(init_error::DEPENDENCY_UNAVAILABLE);

    const paddr_t limit = align_down<paddr_t>(usable_ceiling(boot), FRAME_SIZE);

    if (limit == 0)
        return kernel::core::Err(init_error::NO_USABLE_MEMORY);

    kernel::sync::spinlock_guard guard(g_allocator.lock);

    g_allocator.initialized = false;
    g_allocator.managed_frames = static_cast<usize>(limit >> FRAME_SHIFT);

    reset_state();
    ingest(boot, limit);
    build_lists();

    if (g_allocator.free_frames == 0)
        return kernel::core::Err(init_error::NO_USABLE_MEMORY);

    g_allocator.initialized = true;

    return kernel::core::Ok();
}

}  // namespace kernel::core::memory::pmm
