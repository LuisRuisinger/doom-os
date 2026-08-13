// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/memory/pmm/pmm.hpp"

#include "kernel/core/bits.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"
#include "kernel/sync/spinlock.hpp"

namespace kernel::core::memory::pmm {

namespace {

using kernel::core::u64;
using kernel::core::uptr;
using kernel::core::usize;
using kernel::core::utils::align_down;
using kernel::core::utils::align_up;
using kernel::core::utils::all_bits;
using kernel::core::utils::bit_width;
using kernel::core::utils::clear_bit;
using kernel::core::utils::is_aligned;
using kernel::core::utils::is_power_of_two;
using kernel::core::utils::mask;
using kernel::core::utils::set_bit;
using kernel::core::utils::test_bit;

// =================================================================================================
// Layout
//
// The frame bitmap is the allocator. Everything below it is a summary of that bitmap, kept
// current so that the questions asked most often - is there a free frame, is there an aligned
// 2 MiB block - are answered without scanning half a megabyte.
//
// The summaries are redundant by construction, which is the point: verify_invariants()
// recomputes all of them and compares, so a bookkeeping mistake is caught by a rule rather
// than by whatever symptom it eventually produces.
// =================================================================================================

constexpr usize BITS_PER_WORD = bit_width<u64>();

constexpr usize L0_WORDS = MAX_FRAMES / BITS_PER_WORD;
constexpr usize BLOCK_2M_COUNT = MAX_FRAMES / FRAMES_PER_2M;
constexpr usize BLOCK_1G_COUNT = MAX_FRAMES / FRAMES_PER_1G;
constexpr usize L1_WORDS = BLOCK_2M_COUNT / BITS_PER_WORD;
constexpr usize L2_WORDS = align_up<usize>(BLOCK_1G_COUNT, BITS_PER_WORD) / BITS_PER_WORD;

constexpr usize L0_WORDS_PER_2M = FRAMES_PER_2M / BITS_PER_WORD;
constexpr usize BLOCKS_2M_PER_1G = FRAMES_PER_1G / FRAMES_PER_2M;
constexpr usize L1_WORDS_PER_1G = BLOCKS_2M_PER_1G / BITS_PER_WORD;

constexpr usize NPOS = all_bits<usize>();

static_assert(MAX_FRAMES % BITS_PER_WORD == 0);
static_assert(BLOCK_2M_COUNT % BITS_PER_WORD == 0);
static_assert(FRAMES_PER_2M % BITS_PER_WORD == 0);
static_assert(BLOCKS_2M_PER_1G % BITS_PER_WORD == 0);

// =================================================================================================
// State
//
// Statically sized and zero initialised, so it costs nothing at boot beyond the .bss it
// occupies and there is no metadata to place before the allocator can run.
// =================================================================================================

struct allocator_state {
    // 1 = free. Ground truth; everything below is derived from it.
    u64 frames[L0_WORDS]{};

    u64 free_2m[L1_WORDS]{};     // 1 = every frame in the 2 MiB block is free
    u64 partial_2m[L1_WORDS]{};  // 1 = some frames free, but not all
    u64 free_1g[L2_WORDS]{};     // 1 = every 2 MiB block in the 1 GiB block is free

    usize managed_frames{};
    usize free_frames{};

    // Maintained as the summaries change, so "can this request be met" is answered before
    // anything is handed out rather than discovered partway through.
    usize free_2m_count{};
    usize free_1g_count{};

    // Which 2 MiB block the last single-frame allocation came from. A hint only - it is
    // tested against partial_2m before use, so a stale value costs a scan and never a wrong
    // answer.
    usize partial_hint{};

    bool initialized{};

    kernel::sync::spinlock lock{};
};

allocator_state g_allocator{};

// The kernel image spans the boot sections, which are identity mapped low, through the end of
// the higher-half image. Both symbols carry physical addresses.
extern "C" char boot_start[];
extern "C" char kernel_physical_end[];

// =================================================================================================
// Saturating arithmetic
//
// The boot description is a transcription of what the bootloader said and promises nothing
// about it, so a region may claim to end past the top of the address space.
// =================================================================================================

[[nodiscard]] paddr_t range_end_saturating(paddr_t base, u64 length)
{
    return length > all_bits<paddr_t>() - base ? all_bits<paddr_t>() : base + length;
}

[[nodiscard]] paddr_t align_up_saturating(paddr_t value, u64 alignment)
{
    if (value > all_bits<paddr_t>() - (alignment - 1))
        return align_down<paddr_t>(all_bits<paddr_t>(), alignment);

    return align_up<paddr_t>(value, alignment);
}

// =================================================================================================
// Bitmap primitives
// =================================================================================================

[[nodiscard]] constexpr u32 word_offset(usize index)
{
    return static_cast<u32>(index % BITS_PER_WORD);
}

[[nodiscard]] bool test_index(const u64 *map, usize index)
{
    return test_bit(map[index / BITS_PER_WORD], word_offset(index));
}

void assign_index(u64 *map, usize index, bool value)
{
    u64 &word = map[index / BITS_PER_WORD];

    word = value ? set_bit(word, word_offset(index)) : clear_bit(word, word_offset(index));
}

// Bits [first_bit, 63] and bits [0, last_bit]. mask() already answers with every bit when
// asked for more than the word holds, which is what makes last_bit == 63 fall out rather than
// needing a guard against shifting by the width.
[[nodiscard]] constexpr u64 mask_from(usize first_bit)
{
    return ~mask<u64>(word_offset(first_bit));
}

[[nodiscard]] constexpr u64 mask_upto(usize last_bit)
{
    return mask<u64>(word_offset(last_bit) + 1);
}

// Sets or clears a run of bits a word at a time, so ingestion touching every frame on the
// machine is a few thousand stores rather than a few million.
void apply_range(u64 *map, usize first, usize count, bool value)
{
    if (count == 0)
        return;

    const usize last = first + count - 1;
    const usize first_word = first / BITS_PER_WORD;
    const usize last_word = last / BITS_PER_WORD;

    if (first_word == last_word) {
        const u64 selected = mask_from(first) & mask_upto(last);

        if (value)
            map[first_word] |= selected;
        else
            map[first_word] &= ~selected;

        return;
    }

    const u64 head = mask_from(first);

    if (value)
        map[first_word] |= head;
    else
        map[first_word] &= ~head;

    for (usize word = first_word + 1; word < last_word; ++word)
        map[word] = value ? all_bits<u64>() : 0;

    const u64 tail = mask_upto(last);

    if (value)
        map[last_word] |= tail;
    else
        map[last_word] &= ~tail;
}

[[nodiscard]] usize find_first_set(const u64 *map, usize words, usize limit)
{
    for (usize word = 0; word < words; ++word) {
        if (map[word] == 0)
            continue;

        const usize index = word * BITS_PER_WORD + static_cast<usize>(__builtin_ctzll(map[word]));

        return index < limit ? index : NPOS;
    }

    return NPOS;
}

[[nodiscard]] usize popcount(const u64 *map, usize words)
{
    usize total = 0;

    for (usize word = 0; word < words; ++word)
        total += static_cast<usize>(__builtin_popcountll(map[word]));

    return total;
}

// =================================================================================================
// Derived levels
// =================================================================================================

struct block_state {
    bool all_free;
    bool any_free;
};

[[nodiscard]] block_state inspect_2m(usize block)
{
    const u64 *words = &g_allocator.frames[block * L0_WORDS_PER_2M];

    u64 intersection = all_bits<u64>();
    u64 united = 0;

    for (usize i = 0; i < L0_WORDS_PER_2M; ++i) {
        intersection &= words[i];
        united |= words[i];
    }

    return block_state{intersection == all_bits<u64>(), united != 0};
}

[[nodiscard]] bool all_2m_free_in_1g(usize block)
{
    const u64 *words = &g_allocator.free_2m[block * L1_WORDS_PER_1G];

    for (usize i = 0; i < L1_WORDS_PER_1G; ++i)
        if (words[i] != all_bits<u64>())
            return false;

    return true;
}

void refresh_2m(usize block)
{
    const block_state state = inspect_2m(block);

    if (test_index(g_allocator.free_2m, block) != state.all_free) {
        if (state.all_free)
            ++g_allocator.free_2m_count;
        else
            --g_allocator.free_2m_count;
    }

    assign_index(g_allocator.free_2m, block, state.all_free);
    assign_index(g_allocator.partial_2m, block, state.any_free && !state.all_free);
}

void refresh_1g(usize block)
{
    const bool all_free = all_2m_free_in_1g(block);

    if (test_index(g_allocator.free_1g, block) != all_free) {
        if (all_free)
            ++g_allocator.free_1g_count;
        else
            --g_allocator.free_1g_count;
    }

    assign_index(g_allocator.free_1g, block, all_free);
}

void refresh_frame_range(usize first, usize count)
{
    if (count == 0)
        return;

    const usize last = first + count - 1;
    const usize first_block = first / FRAMES_PER_2M;
    const usize last_block = last / FRAMES_PER_2M;

    for (usize block = first_block; block <= last_block; ++block)
        refresh_2m(block);

    for (usize block = first_block / BLOCKS_2M_PER_1G; block <= last_block / BLOCKS_2M_PER_1G;
         ++block)
        refresh_1g(block);
}

// =================================================================================================
// Allocation
// =================================================================================================

// Prefers a block that is already broken up. Serving single frames from pristine blocks
// instead would shatter one 2 MiB block per allocation and leave a nearly empty machine unable
// to satisfy an aligned large request.
[[nodiscard]] usize pick_block_for_single_frame()
{
    const usize hint = g_allocator.partial_hint;

    if (hint < BLOCK_2M_COUNT && test_index(g_allocator.partial_2m, hint))
        return hint;

    const usize partial = find_first_set(g_allocator.partial_2m, L1_WORDS, BLOCK_2M_COUNT);

    if (partial != NPOS)
        return partial;

    return find_first_set(g_allocator.free_2m, L1_WORDS, BLOCK_2M_COUNT);
}

[[nodiscard]] paddr_t take_frame_locked()
{
    const usize block = pick_block_for_single_frame();

    if (block == NPOS)
        return INVALID_PHYSICAL_ADDRESS;

    const usize base_word = block * L0_WORDS_PER_2M;

    for (usize i = 0; i < L0_WORDS_PER_2M; ++i) {
        const u64 word = g_allocator.frames[base_word + i];

        if (word == 0)
            continue;

        const u32   offset = static_cast<u32>(__builtin_ctzll(word));
        const usize frame = (base_word + i) * BITS_PER_WORD + offset;

        g_allocator.frames[base_word + i] = clear_bit(word, offset);
        --g_allocator.free_frames;

        refresh_2m(block);
        refresh_1g(block / BLOCKS_2M_PER_1G);

        g_allocator.partial_hint = block;

        return static_cast<paddr_t>(frame) << FRAME_SHIFT;
    }

    // The block was chosen because a summary said it held a free frame.
    KPANIC("pmm: 2 MiB block {} claims a free frame it does not have", block);
}

[[nodiscard]] paddr_t take_block_locked(const u64 *level, usize level_words, usize level_count,
                                        usize frames_per_block)
{
    const usize block = find_first_set(level, level_words, level_count);

    if (block == NPOS)
        return INVALID_PHYSICAL_ADDRESS;

    const usize first = block * frames_per_block;

    apply_range(g_allocator.frames, first, frames_per_block, false);
    g_allocator.free_frames -= frames_per_block;
    refresh_frame_range(first, frames_per_block);

    return static_cast<paddr_t>(first) << FRAME_SHIFT;
}

// One page of the named size, naturally aligned. Each case is an index lookup rather than a
// search, which is what lets a batch be served without scanning anything.
[[nodiscard]] paddr_t take_page_locked(page_size size)
{
    switch (size) {
        case page_size::SMALL_4K:
            return take_frame_locked();
        case page_size::LARGE_2M:
            return take_block_locked(g_allocator.free_2m, L1_WORDS, BLOCK_2M_COUNT, FRAMES_PER_2M);
        case page_size::HUGE_1G:
            return take_block_locked(g_allocator.free_1g, L2_WORDS, BLOCK_1G_COUNT, FRAMES_PER_1G);
    }

    return INVALID_PHYSICAL_ADDRESS;
}

// How many pages of this size could be handed out right now. Exact rather than an estimate:
// taking one 4 KiB page costs one frame, one 2 MiB page costs one free_2m bit, and one 1 GiB
// page costs one free_1g bit - so a batch that passes this check cannot fail partway.
[[nodiscard]] usize available_pages_locked(page_size size)
{
    switch (size) {
        case page_size::SMALL_4K:
            return g_allocator.free_frames;
        case page_size::LARGE_2M:
            return g_allocator.free_2m_count;
        case page_size::HUGE_1G:
            return g_allocator.free_1g_count;
    }

    return 0;
}

// Shared by every release path. Everything it rejects is a caller bug rather than a runtime
// condition, so it panics instead of reporting.
void release_locked(paddr_t base, usize frame_count, u64 alignment)
{
    if (!g_allocator.initialized)
        KPANIC("pmm: release of {:#018X} before the allocator is initialised", base);

    if (!is_aligned<paddr_t>(base, alignment))
        KPANIC("pmm: release of {:#018X}, which is not aligned to {} bytes", base, alignment);

    const usize first = static_cast<usize>(base >> FRAME_SHIFT);

    if (first >= g_allocator.managed_frames || frame_count > g_allocator.managed_frames - first)
        KPANIC("pmm: release of {} frames at {:#018X} leaves the managed range", frame_count, base);

    for (usize i = 0; i < frame_count; ++i)
        if (test_index(g_allocator.frames, first + i))
            KPANIC("pmm: double free of frame {:#018X} in a {} frame release at {:#018X}",
                   static_cast<paddr_t>(first + i) << FRAME_SHIFT, frame_count, base);

    apply_range(g_allocator.frames, first, frame_count, true);
    g_allocator.free_frames += frame_count;
    refresh_frame_range(first, frame_count);
}

// Counts how many frames from `first` are free, stopping at `limit`.
[[nodiscard]] usize free_run_length(usize first, usize limit)
{
    usize length = 0;

    while (length < limit && first + length < g_allocator.managed_frames &&
           test_index(g_allocator.frames, first + length))
        ++length;

    return length;
}

[[nodiscard]] paddr_t alloc_run_locked(usize count, usize alignment_frames)
{
    usize start = 0;

    for (;;) {
        start = align_up<usize>(start, alignment_frames);

        if (start >= g_allocator.managed_frames || count > g_allocator.managed_frames - start)
            return INVALID_PHYSICAL_ADDRESS;

        const usize run = free_run_length(start, count);

        if (run == count) {
            apply_range(g_allocator.frames, start, count, false);
            g_allocator.free_frames -= count;
            refresh_frame_range(start, count);

            return static_cast<paddr_t>(start) << FRAME_SHIFT;
        }

        // The frame just past the run is taken, so no candidate start inside it can work.
        start += run + 1;
    }
}

// =================================================================================================
// Ingestion
//
// Four passes over the boot description, none of which depends on the order the regions
// arrive in or on their being disjoint. Marking is idempotent, so an overlap costs nothing and
// a region described twice is described once.
// =================================================================================================

void reset_bitmaps()
{
    for (usize word = 0; word < L0_WORDS; ++word)
        g_allocator.frames[word] = 0;

    for (usize word = 0; word < L1_WORDS; ++word) {
        g_allocator.free_2m[word] = 0;
        g_allocator.partial_2m[word] = 0;
    }

    for (usize word = 0; word < L2_WORDS; ++word)
        g_allocator.free_1g[word] = 0;

    g_allocator.free_2m_count = 0;
    g_allocator.free_1g_count = 0;
}

// Rounds inward: a frame only partly covered by usable memory is not usable.
void mark_usable(paddr_t base, u64 length, paddr_t limit)
{
    const paddr_t first = align_up_saturating(base, FRAME_SIZE);

    paddr_t end = align_down<paddr_t>(range_end_saturating(base, length), FRAME_SIZE);

    if (end > limit)
        end = limit;

    if (first >= end)
        return;

    apply_range(g_allocator.frames, first >> FRAME_SHIFT, (end - first) >> FRAME_SHIFT, true);
}

// Rounds outward: a frame partly covered by something else is not ours to hand out.
void mark_occupied(paddr_t base, u64 length, paddr_t limit)
{
    if (length == 0)
        return;

    const paddr_t first = align_down<paddr_t>(base, FRAME_SIZE);

    if (first >= limit)
        return;

    paddr_t end = align_up_saturating(range_end_saturating(base, length), FRAME_SIZE);

    if (end > limit)
        end = limit;

    if (first >= end)
        return;

    apply_range(g_allocator.frames, first >> FRAME_SHIFT, (end - first) >> FRAME_SHIFT, false);
}

[[nodiscard]] paddr_t highest_usable_end(const kernel::boot::info &boot)
{
    paddr_t highest = 0;

    for (usize i = 0; i < boot.memory_map.count; ++i) {
        const auto &region = boot.memory_map[i];

        if (region.kind != kernel::boot::memory_kind::USABLE || region.length == 0)
            continue;

        const paddr_t end = range_end_saturating(region.base, region.length);

        if (end > highest)
            highest = end;
    }

    return highest;
}

void add_usable_memory(const kernel::boot::info &boot, paddr_t limit)
{
    for (usize i = 0; i < boot.memory_map.count; ++i) {
        const auto &region = boot.memory_map[i];

        if (region.kind != kernel::boot::memory_kind::USABLE)
            continue;

        mark_usable(region.base, region.length, limit);
    }
}

// Logged as it goes, because "how much memory did the kernel keep for itself" is otherwise a
// subtraction nobody can check against anything.
void reserve_and_log(const char *what, paddr_t base, u64 length, paddr_t limit)
{
    if (length == 0)
        return;

    mark_occupied(base, length, limit);

    KPRINTLN("[pmm]   reserve {:#018X}..{:#018X} {}", base, range_end_saturating(base, length),
             what);
}

void reserve_occupied_ranges(const kernel::boot::info &boot, paddr_t limit)
{
    // Real mode IVT, BDA, EBDA, and whatever else the firmware still believes it owns.
    reserve_and_log("low memory", 0, 1024 * 1024, limit);

    const paddr_t image_first = static_cast<paddr_t>(reinterpret_cast<uptr>(boot_start));
    const paddr_t image_end = static_cast<paddr_t>(reinterpret_cast<uptr>(kernel_physical_end));

    if (image_end > image_first)
        reserve_and_log("kernel image", image_first, image_end - image_first, limit);

    for (usize i = 0; i < boot.reserved.count; ++i)
        reserve_and_log("boot protocol", boot.reserved[i].base, boot.reserved[i].length, limit);

    // Module bytes were never copied out of where the bootloader put them.
    for (usize i = 0; i < boot.modules.count; ++i)
        reserve_and_log("module", boot.modules[i].range.base, boot.modules[i].range.length, limit);

    // An MMIO aperture rather than RAM, and commonly absent from the memory map entirely, so
    // it cannot be inferred from the regions above.
    if (boot.framebuffer.present)
        reserve_and_log("framebuffer", boot.framebuffer.address,
                        static_cast<u64>(boot.framebuffer.pitch) * boot.framebuffer.height, limit);
}

void rebuild_derived_levels()
{
    for (usize block = 0; block < BLOCK_2M_COUNT; ++block)
        refresh_2m(block);

    for (usize block = 0; block < BLOCK_1G_COUNT; ++block)
        refresh_1g(block);
}

// =================================================================================================
// Reporting
// =================================================================================================

void log_memory_map(const kernel::boot::info &boot)
{
    KPRINTLN("[pmm] boot memory map ({} regions)", boot.memory_map.count);

    for (usize i = 0; i < boot.memory_map.count; ++i) {
        const auto &region = boot.memory_map[i];

        KPRINTLN("[pmm]   {:#018X}..{:#018X} {}", region.base,
                 range_end_saturating(region.base, region.length),
                 kernel::boot::describe(region.kind));
    }
}

void log_totals()
{
    const usize free_kib = g_allocator.free_frames * (FRAME_SIZE / 1024);
    const usize managed_kib = g_allocator.managed_frames * (FRAME_SIZE / 1024);

    KPRINTLN("[pmm] managed={} KiB free={} KiB ({} frames, {} free 2M, {} free 1G)", managed_kib,
             free_kib, g_allocator.free_frames, g_allocator.free_2m_count,
             g_allocator.free_1g_count);
}

}  // namespace

// =================================================================================================
// Self-test
//
// Built only when DOOM_OS_PMM_SELF_TEST is defined. Runs after init has published the
// allocator and uses the public API, so it exercises the locking path as well as the
// bookkeeping. A failure is a bug in the allocator rather than a condition to report, so it
// panics rather than returning.
// =================================================================================================

#ifdef DOOM_OS_PMM_SELF_TEST

namespace {

constexpr usize SELF_TEST_SAMPLES = 4096;
constexpr usize SELF_TEST_BATCH = 51;
constexpr usize SELF_TEST_ODD_RUN = 11;
constexpr usize SELF_TEST_ODD_ALIGN = 8;
constexpr usize SELF_TEST_LARGE_BATCH = 3;
constexpr usize SELF_TEST_OVERSHOOT_ROOM = 32;

paddr_t g_self_test_pages[SELF_TEST_SAMPLES]{};

void expect(bool condition, const char *what)
{
    if (!condition)
        KPANIC("pmm self-test: {}", what);
}

void expect_invariants(const char *stage)
{
    if (!verify_invariants())
        KPANIC("pmm self-test: derived levels disagree with the frame bitmap after {}", stage);
}

// Distinctness is checked by the release rather than by comparing every pair: handing the same
// page out twice would trip the double-free panic.
void check_batch(page_size size, usize count, const char *stage)
{
    const stats before = current_stats();

    if (!alloc_pages(size, count, g_self_test_pages)) {
        KPRINTLN("[pmm] self-test: {} pages of {} unavailable, skipping", count, describe(size));
        return;
    }

    for (usize i = 0; i < count; ++i) {
        expect(g_self_test_pages[i] != INVALID_PHYSICAL_ADDRESS, "batch returned an invalid page");
        expect(is_aligned<paddr_t>(g_self_test_pages[i], bytes_in(size)),
               "batch page is not naturally aligned");
        expect(!is_free(g_self_test_pages[i]), "batch page still reads as free");
    }

    expect(current_stats().free_frames == before.free_frames - count * frames_in(size),
           "free count did not drop by the size of the batch");

    expect_invariants(stage);

    free_pages(size, count, g_self_test_pages);

    expect(current_stats().free_frames == before.free_frames,
           "free count did not return after releasing the batch");

    expect_invariants(stage);
}

// A batch that cannot be met must allocate nothing at all, so the caller never has to unwind a
// partial result.
void check_all_or_nothing()
{
    const stats before = current_stats();

    paddr_t pages[SELF_TEST_OVERSHOOT_ROOM]{};

    const usize request = before.free_1g_pages + 1;

    if (request > SELF_TEST_OVERSHOOT_ROOM)
        return;

    expect(!alloc_pages(page_size::HUGE_1G, request, pages),
           "a batch larger than the supply reported success");

    expect(current_stats().free_frames == before.free_frames,
           "a batch that failed still consumed memory");

    expect_invariants("a rejected batch");
}

// The case a buddy allocator cannot serve exactly: a run that is neither a power of two nor
// naturally aligned to its own size.
void check_odd_run()
{
    const stats before = current_stats();

    const paddr_t run = alloc_contiguous(SELF_TEST_ODD_RUN, SELF_TEST_ODD_ALIGN);

    expect(run != INVALID_PHYSICAL_ADDRESS, "could not allocate an odd length run");
    expect(is_aligned<paddr_t>(run, SELF_TEST_ODD_ALIGN * FRAME_SIZE),
           "odd length run ignored its alignment");

    for (usize i = 0; i < SELF_TEST_ODD_RUN; ++i)
        expect(!is_free(run + i * FRAME_SIZE), "odd length run is not contiguously allocated");

    expect(is_free(run + SELF_TEST_ODD_RUN * FRAME_SIZE) ||
               !contains(run + SELF_TEST_ODD_RUN * FRAME_SIZE),
           "odd length run took more frames than it was asked for");

    expect(current_stats().free_frames == before.free_frames - SELF_TEST_ODD_RUN,
           "free count did not drop by the length of the run");

    free_contiguous(run, SELF_TEST_ODD_RUN);

    expect(current_stats().free_frames == before.free_frames,
           "free count did not return after releasing the run");

    expect_invariants("odd length run");
}

void run_self_test()
{
    KPRINTLN("[pmm] self-test running");

    expect_invariants("initialisation");

    check_batch(page_size::SMALL_4K, SELF_TEST_BATCH, "a small batch");
    check_batch(page_size::SMALL_4K, SELF_TEST_SAMPLES, "many single pages");
    check_batch(page_size::LARGE_2M, SELF_TEST_LARGE_BATCH, "a 2 MiB batch");
    check_batch(page_size::HUGE_1G, SELF_TEST_LARGE_BATCH, "a 1 GiB batch");
    check_all_or_nothing();
    check_odd_run();

    KPRINTLN("[pmm] self-test passed");
}

}  // namespace

#endif  // DOOM_OS_PMM_SELF_TEST

// =================================================================================================
// PMM API
// =================================================================================================

bool initialized()
{
    kernel::sync::spinlock_guard guard(g_allocator.lock);

    return g_allocator.initialized;
}

stats current_stats()
{
    kernel::sync::spinlock_guard guard(g_allocator.lock);

    return stats{
        .initialized = g_allocator.initialized,
        .managed_frames = g_allocator.managed_frames,
        .free_frames = g_allocator.free_frames,
        .allocated_frames = g_allocator.managed_frames - g_allocator.free_frames,
        .free_2m_pages = g_allocator.free_2m_count,
        .free_1g_pages = g_allocator.free_1g_count,
    };
}

const char *describe(page_size size)
{
    switch (size) {
        case page_size::SMALL_4K:
            return "4 KiB";
        case page_size::LARGE_2M:
            return "2 MiB";
        case page_size::HUGE_1G:
            return "1 GiB";
    }

    return "unknown";
}

bool alloc_pages(page_size size, usize count, paddr_t *out)
{
    if (count == 0)
        return true;

    if (out == nullptr)
        return false;

    kernel::sync::spinlock_guard guard(g_allocator.lock);

    if (!g_allocator.initialized || count > available_pages_locked(size))
        return false;

    // The check above is exact, so nothing below can come up short. Every page is an index
    // lookup, so a batch costs one lock acquisition and no search - and consecutive 4 KiB
    // pages come out of the same partially used block until it is empty, which keeps them
    // together and confines the damage to one block.
    for (usize i = 0; i < count; ++i) {
        out[i] = take_page_locked(size);

        if (out[i] == INVALID_PHYSICAL_ADDRESS)
            KPANIC("pmm: {} pages of {} were available but page {} could not be taken", count,
                   describe(size), i);
    }

    return true;
}

void free_pages(page_size size, usize count, const paddr_t *pages)
{
    if (count == 0)
        return;

    if (pages == nullptr)
        KPANIC("pmm: release of {} pages of {} from a null array", count, describe(size));

    kernel::sync::spinlock_guard guard(g_allocator.lock);

    for (usize i = 0; i < count; ++i)
        release_locked(pages[i], frames_in(size), bytes_in(size));
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

paddr_t alloc_contiguous(usize frame_count, usize alignment_frames)
{
    if (frame_count == 0 || !is_power_of_two<usize>(alignment_frames))
        return INVALID_PHYSICAL_ADDRESS;

    kernel::sync::spinlock_guard guard(g_allocator.lock);

    if (!g_allocator.initialized || frame_count > g_allocator.free_frames)
        return INVALID_PHYSICAL_ADDRESS;

    // Not a guess at what the caller meant - for a request that is explicitly contiguous, a
    // naturally aligned run of exactly one block is the same thing the level already indexes,
    // so the search is skipped rather than reinterpreted.
    if (frame_count == FRAMES_PER_1G && alignment_frames == FRAMES_PER_1G)
        return take_page_locked(page_size::HUGE_1G);

    if (frame_count == FRAMES_PER_2M && alignment_frames == FRAMES_PER_2M)
        return take_page_locked(page_size::LARGE_2M);

    if (frame_count == 1 && alignment_frames == 1)
        return take_page_locked(page_size::SMALL_4K);

    return alloc_run_locked(frame_count, alignment_frames);
}

void free_contiguous(paddr_t base, usize frame_count)
{
    if (frame_count == 0)
        return;

    kernel::sync::spinlock_guard guard(g_allocator.lock);

    release_locked(base, frame_count, FRAME_SIZE);
}

bool is_free(paddr_t address)
{
    kernel::sync::spinlock_guard guard(g_allocator.lock);

    const usize frame = static_cast<usize>(address >> FRAME_SHIFT);

    if (!g_allocator.initialized || frame >= g_allocator.managed_frames)
        return false;

    return test_index(g_allocator.frames, frame);
}

bool contains(paddr_t address)
{
    kernel::sync::spinlock_guard guard(g_allocator.lock);

    return g_allocator.initialized &&
           static_cast<usize>(address >> FRAME_SHIFT) < g_allocator.managed_frames;
}

bool verify_invariants()
{
    kernel::sync::spinlock_guard guard(g_allocator.lock);

    if (!g_allocator.initialized)
        return false;

    for (usize block = 0; block < BLOCK_2M_COUNT; ++block) {
        const block_state state = inspect_2m(block);

        if (test_index(g_allocator.free_2m, block) != state.all_free)
            return false;

        if (test_index(g_allocator.partial_2m, block) != (state.any_free && !state.all_free))
            return false;
    }

    for (usize block = 0; block < BLOCK_1G_COUNT; ++block)
        if (test_index(g_allocator.free_1g, block) != all_2m_free_in_1g(block))
            return false;

    return popcount(g_allocator.frames, L0_WORDS) == g_allocator.free_frames;
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

    log_memory_map(boot);

    paddr_t highest = highest_usable_end(boot);

    if (highest == 0)
        return kernel::core::Err(init_error::NO_USABLE_MEMORY);

    if (highest > MAX_PHYSICAL_MEMORY) {
        KPRINTLN(
            "[pmm] usable memory reaches {:#018X}; managing the first {} KiB and ignoring "
            "the rest",
            highest, MAX_PHYSICAL_MEMORY / 1024);

        highest = MAX_PHYSICAL_MEMORY;
    }

    highest = align_down<paddr_t>(highest, FRAME_SIZE);

    if (highest == 0)
        return kernel::core::Err(init_error::NO_USABLE_MEMORY);

    {
        kernel::sync::spinlock_guard guard(g_allocator.lock);

        g_allocator.initialized = false;
        g_allocator.managed_frames = static_cast<usize>(highest >> FRAME_SHIFT);
        g_allocator.partial_hint = 0;

        // Everything is occupied until a usable region says otherwise. Deriving free memory
        // as the complement of what was reserved would hand out any region the description
        // lost.
        reset_bitmaps();

        add_usable_memory(boot, highest);
        reserve_occupied_ranges(boot, highest);

        rebuild_derived_levels();

        g_allocator.free_frames = popcount(g_allocator.frames, L0_WORDS);

        if (g_allocator.free_frames == 0)
            return kernel::core::Err(init_error::NO_USABLE_MEMORY);

        g_allocator.initialized = true;

        log_totals();
    }

#ifdef DOOM_OS_PMM_SELF_TEST
    // Outside the guard: the self-test goes through the public API, so it takes the lock
    // itself and would otherwise deadlock on a non-recursive spinlock.
    run_self_test();
#endif

    return kernel::core::Ok();
}

}  // namespace kernel::core::memory::pmm
