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

    assign_index(g_allocator.free_2m, block, state.all_free);
    assign_index(g_allocator.partial_2m, block, state.any_free && !state.all_free);
}

void refresh_1g(usize block)
{
    assign_index(g_allocator.free_1g, block, all_2m_free_in_1g(block));
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

[[nodiscard]] paddr_t alloc_single_locked()
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
             free_kib, g_allocator.free_frames, popcount(g_allocator.free_2m, L1_WORDS),
             popcount(g_allocator.free_1g, L2_WORDS));
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
constexpr usize SELF_TEST_ODD_RUN = 11;
constexpr usize SELF_TEST_ODD_ALIGN = 8;

paddr_t g_self_test_frames[SELF_TEST_SAMPLES]{};

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

// Allocates single frames, releases them, and checks the totals come back. Distinctness is
// checked by the release: handing the same frame out twice would trip the double-free panic.
void check_single_frames()
{
    const stats before = current_stats();

    for (usize i = 0; i < SELF_TEST_SAMPLES; ++i) {
        g_self_test_frames[i] = alloc_frame();

        expect(g_self_test_frames[i] != INVALID_PHYSICAL_ADDRESS, "ran out of single frames");
        expect(is_aligned<paddr_t>(g_self_test_frames[i], FRAME_SIZE),
               "single frame is not aligned");
        expect(!is_free(g_self_test_frames[i]), "allocated frame still reads as free");
    }

    expect(current_stats().free_frames == before.free_frames - SELF_TEST_SAMPLES,
           "free count did not drop by the number of frames allocated");

    expect_invariants("single frame allocation");

    for (usize i = 0; i < SELF_TEST_SAMPLES; ++i)
        free_frame(g_self_test_frames[i]);

    expect(current_stats().free_frames == before.free_frames,
           "free count did not return after releasing every frame");

    expect_invariants("single frame release");
}

void check_large_block(usize frames_per_block, const char *what)
{
    const stats before = current_stats();

    const paddr_t block = alloc_frames(frames_per_block, frames_per_block);

    if (block == INVALID_PHYSICAL_ADDRESS) {
        KPRINTLN("[pmm] self-test: no {} block available, skipping", what);
        return;
    }

    expect(is_aligned<paddr_t>(block, frames_per_block * FRAME_SIZE), "large block is not aligned");
    expect(current_stats().free_frames == before.free_frames - frames_per_block,
           "free count did not drop by the size of the block");

    expect_invariants("large block allocation");

    free_frames(block, frames_per_block);

    expect(current_stats().free_frames == before.free_frames,
           "free count did not return after releasing the block");

    expect_invariants("large block release");
}

// The case a buddy allocator cannot serve exactly: a run that is neither a power of two nor
// naturally aligned to its own size.
void check_odd_run()
{
    const stats before = current_stats();

    const paddr_t run = alloc_frames(SELF_TEST_ODD_RUN, SELF_TEST_ODD_ALIGN);

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

    free_frames(run, SELF_TEST_ODD_RUN);

    expect(current_stats().free_frames == before.free_frames,
           "free count did not return after releasing the run");

    expect_invariants("odd length run");
}

void run_self_test()
{
    KPRINTLN("[pmm] self-test running");

    expect_invariants("initialisation");

    check_single_frames();
    check_large_block(FRAMES_PER_2M, "2 MiB");
    check_large_block(FRAMES_PER_1G, "1 GiB");
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
        .free_2m_blocks = popcount(g_allocator.free_2m, L1_WORDS),
        .free_1g_blocks = popcount(g_allocator.free_1g, L2_WORDS),
    };
}

paddr_t alloc_frames(usize count, usize alignment_frames)
{
    if (count == 0 || !is_power_of_two<usize>(alignment_frames))
        return INVALID_PHYSICAL_ADDRESS;

    kernel::sync::spinlock_guard guard(g_allocator.lock);

    if (!g_allocator.initialized || count > g_allocator.free_frames)
        return INVALID_PHYSICAL_ADDRESS;

    if (count == 1 && alignment_frames == 1)
        return alloc_single_locked();

    if (count == FRAMES_PER_1G && alignment_frames == FRAMES_PER_1G)
        return take_block_locked(g_allocator.free_1g, L2_WORDS, BLOCK_1G_COUNT, FRAMES_PER_1G);

    if (count == FRAMES_PER_2M && alignment_frames == FRAMES_PER_2M)
        return take_block_locked(g_allocator.free_2m, L1_WORDS, BLOCK_2M_COUNT, FRAMES_PER_2M);

    return alloc_run_locked(count, alignment_frames);
}

paddr_t alloc_frame()
{
    return alloc_frames(1, 1);
}

void free_frames(paddr_t base, usize count)
{
    if (count == 0)
        return;

    kernel::sync::spinlock_guard guard(g_allocator.lock);

    if (!g_allocator.initialized)
        KPANIC("pmm: free of {:#018X} before the allocator is initialised", base);

    if (!is_aligned<paddr_t>(base, FRAME_SIZE))
        KPANIC("pmm: free of unaligned address {:#018X}", base);

    const usize first = static_cast<usize>(base >> FRAME_SHIFT);

    if (first >= g_allocator.managed_frames || count > g_allocator.managed_frames - first)
        KPANIC("pmm: free of {} frames at {:#018X} leaves the managed range", count, base);

    for (usize i = 0; i < count; ++i)
        if (test_index(g_allocator.frames, first + i))
            KPANIC("pmm: double free of frame {:#018X} in a {} frame release at {:#018X}",
                   static_cast<paddr_t>(first + i) << FRAME_SHIFT, count, base);

    apply_range(g_allocator.frames, first, count, true);
    g_allocator.free_frames += count;
    refresh_frame_range(first, count);
}

void free_frame(paddr_t base)
{
    free_frames(base, 1);
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
