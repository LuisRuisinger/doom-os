// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/memory/pmm/pmm.hpp"

#include "kernel/core/bits.hpp"
#include "kernel/debug/kprint.hpp"
#include "kernel/sync/spinlock.hpp"

namespace kernel::core::memory::pmm {

namespace {

using kernel::boot::info;
using kernel::boot::memory_kind;
using kernel::core::u8;
using kernel::core::uptr;
using kernel::sync::spinlock;
using kernel::sync::spinlock_guard;

inline constexpr u32     INVALID_PFN = ~u32{0};
inline constexpr u8      INVALID_ORDER = 0xff;
inline constexpr u8      INVALID_ZONE = 0xff;
inline constexpr u64     LOW_MEMORY_SIZE = 1024 * 1024;
inline constexpr paddr_t EARLY_METADATA_ADDRESS_LIMIT = 1024ull * 1024 * 1024;

enum class page_state : u8 {
    RESERVED,
    FREE,
    ALLOCATED,
};

struct page {
    u32        next{INVALID_PFN};
    u32        prev{INVALID_PFN};
    u8         order{INVALID_ORDER};
    u8         zone{INVALID_ZONE};
    page_state state{page_state::RESERVED};
    bool       managed{};
};

struct free_area {
    u32 head{INVALID_PFN};
    u64 block_count{};
};

struct zone {
    zone_kind kind{};
    paddr_t   base{};
    paddr_t   limit{};
    u64       managed_pages{};
    u64       free_pages{};
    u64       allocated_pages{};
    free_area areas[MAX_ORDER + 1]{};
    spinlock  lock{};
};

struct allocator_state {
    bool    initialized{};
    page   *pages{};
    u64     page_count{};
    paddr_t managed_limit{};
    paddr_t metadata_base{};
    u64     metadata_length{};
    zone    zones[ZONE_COUNT]{};
};

allocator_state g_allocator{};

extern "C" char boot_start[];
extern "C" char kernel_physical_end[];

[[nodiscard]] constexpr usize zone_index(zone_kind kind)
{
    return static_cast<usize>(kind);
}

[[nodiscard]] constexpr u64 order_pages(u32 order)
{
    return u64{1} << order;
}

[[nodiscard]] constexpr u64 order_bytes(u32 order)
{
    return order_pages(order) * PAGE_SIZE;
}

void lock_all_zones()
{
    for (usize i = 0; i < ZONE_COUNT; ++i)
        g_allocator.zones[i].lock.lock();
}

void unlock_all_zones()
{
    for (usize i = ZONE_COUNT; i > 0; --i)
        g_allocator.zones[i - 1].lock.unlock();
}

class all_zone_locks_guard {
public:
    all_zone_locks_guard()
    {
        lock_all_zones();
    }

    ~all_zone_locks_guard()
    {
        unlock_all_zones();
    }

    all_zone_locks_guard(const all_zone_locks_guard &) = delete;
    all_zone_locks_guard &operator=(const all_zone_locks_guard &) = delete;

    all_zone_locks_guard(all_zone_locks_guard &&) = delete;
    all_zone_locks_guard &operator=(all_zone_locks_guard &&) = delete;
};

[[nodiscard]] constexpr paddr_t range_end_saturating(paddr_t base, u64 length)
{
    if (length > ~paddr_t{0} - base)
        return ~paddr_t{0};

    return base + length;
}

[[nodiscard]] constexpr paddr_t align_up_saturating(paddr_t value, u64 alignment)
{
    if (value > ~paddr_t{0} - (alignment - 1))
        return ~paddr_t{0} & ~(alignment - 1);

    return kernel::core::utils::align_up<paddr_t>(value, alignment);
}

[[nodiscard]] constexpr u32 pfn_from_address(paddr_t address)
{
    return static_cast<u32>(address >> PAGE_SHIFT);
}

[[nodiscard]] constexpr paddr_t address_from_pfn(u32 pfn)
{
    return static_cast<paddr_t>(pfn) << PAGE_SHIFT;
}

[[nodiscard]] bool pfn_in_range(u32 pfn)
{
    return pfn < g_allocator.page_count;
}

[[nodiscard]] bool block_in_range(u32 pfn, u32 order)
{
    return pfn_in_range(pfn) && order <= MAX_ORDER &&
           order_pages(order) <= g_allocator.page_count - pfn;
}

[[nodiscard]] bool pfn_aligned_to_order(u32 pfn, u32 order)
{
    return (pfn & static_cast<u32>(order_pages(order) - 1)) == 0;
}

[[nodiscard]] paddr_t symbol_address(const char *symbol)
{
    return reinterpret_cast<paddr_t>(symbol);
}

[[nodiscard]] constexpr bool ranges_overlap(paddr_t first_base, paddr_t first_end,
                                            paddr_t second_base, paddr_t second_end)
{
    return first_base < second_end && second_base < first_end;
}

[[nodiscard]] constexpr paddr_t min_address(paddr_t left, paddr_t right)
{
    return left < right ? left : right;
}

[[nodiscard]] constexpr paddr_t max_address(paddr_t left, paddr_t right)
{
    return left > right ? left : right;
}

[[nodiscard]] paddr_t managed_limit_from_boot_memory(const info &boot)
{
    paddr_t limit = 0;

    for (usize i = 0; i < boot.memory_map.count; ++i) {
        const auto &region = boot.memory_map[i];

        if (region.kind != memory_kind::USABLE)
            continue;

        const paddr_t end =
            align_up_saturating(range_end_saturating(region.base, region.length), PAGE_SIZE);
        limit = max_address(limit, min_address(end, MAX_MANAGED_MEMORY_BYTES));
    }

    return limit;
}

[[nodiscard]] constexpr u64 page_metadata_length(u64 page_count)
{
    const u64 bytes = page_count * sizeof(page);

    return kernel::core::utils::align_up<u64>(bytes, PAGE_SIZE);
}

[[nodiscard]] bool protected_range_overlaps(paddr_t base, paddr_t end, paddr_t protected_base,
                                            u64 protected_length, paddr_t &overlap_base)
{
    const paddr_t protected_end = range_end_saturating(protected_base, protected_length);

    if (!ranges_overlap(base, end, protected_base, protected_end))
        return false;

    overlap_base = protected_base;
    return true;
}

[[nodiscard]] bool boot_protected_range_overlap(paddr_t base, paddr_t end, const info &boot,
                                                paddr_t &overlap_base)
{
    const paddr_t kernel_base = symbol_address(boot_start);
    const paddr_t kernel_end = symbol_address(kernel_physical_end);

    if (ranges_overlap(base, end, kernel_base, kernel_end)) {
        overlap_base = kernel_base;
        return true;
    }

    if (protected_range_overlaps(base, end, 0, LOW_MEMORY_SIZE, overlap_base))
        return true;

    for (usize i = 0; i < boot.reserved.count; ++i) {
        const auto &range = boot.reserved[i];

        if (range.length != 0 &&
            protected_range_overlaps(base, end, range.base, range.length, overlap_base))
            return true;
    }

    for (usize i = 0; i < boot.modules.count; ++i) {
        const auto &range = boot.modules[i].range;

        if (range.length == 0)
            continue;

        if (protected_range_overlaps(base, end, range.base, range.length, overlap_base))
            return true;
    }

    if (boot.framebuffer.present) {
        const u64 pitch = boot.framebuffer.pitch;
        const u64 height = boot.framebuffer.height;

        if (height != 0 && pitch <= ~u64{0} / height) {
            const u64 byte_count = pitch * height;

            if (byte_count != 0 && protected_range_overlaps(base, end, boot.framebuffer.address,
                                                            byte_count, overlap_base))
                return true;
        }
    }

    return false;
}

[[nodiscard]] bool find_metadata_in_available_range(paddr_t range_base, paddr_t range_end,
                                                    u64 length, const info &boot,
                                                    paddr_t &metadata_base)
{
    paddr_t cursor = kernel::core::utils::align_down<paddr_t>(range_end, PAGE_SIZE);

    while (cursor >= range_base && cursor - range_base >= length) {
        const paddr_t candidate_end = cursor;
        const paddr_t candidate_base = candidate_end - length;
        paddr_t       overlap_base = 0;

        if (!boot_protected_range_overlap(candidate_base, candidate_end, boot, overlap_base)) {
            metadata_base = candidate_base;
            return true;
        }

        if (overlap_base <= range_base)
            return false;

        cursor = kernel::core::utils::align_down<paddr_t>(overlap_base, PAGE_SIZE);
    }

    return false;
}

[[nodiscard]] bool choose_metadata_storage(const info &boot, paddr_t managed_limit, u64 page_count,
                                           paddr_t &metadata_base, u64 &metadata_length)
{
    metadata_length = page_metadata_length(page_count);

    if (managed_limit == 0 || page_count == 0 || metadata_length == 0)
        return false;

    paddr_t best_base = 0;
    bool    found = false;

    for (usize i = 0; i < boot.memory_map.count; ++i) {
        const auto &region = boot.memory_map[i];

        if (region.kind != memory_kind::USABLE)
            continue;

        paddr_t start = align_up_saturating(region.base, PAGE_SIZE);
        paddr_t end = kernel::core::utils::align_down<paddr_t>(
            range_end_saturating(region.base, region.length), PAGE_SIZE);

        end = min_address(end, managed_limit);
        end = min_address(end, EARLY_METADATA_ADDRESS_LIMIT);

        if (end <= start || end - start < metadata_length)
            continue;

        paddr_t candidate_base = 0;
        if (!find_metadata_in_available_range(start, end, metadata_length, boot, candidate_base))
            continue;

        if (!found || candidate_base > best_base) {
            best_base = candidate_base;
            found = true;
        }
    }

    if (!found)
        return false;

    metadata_base = best_base;
    return true;
}

[[nodiscard]] zone_kind zone_for_address(paddr_t address)
{
    if (address < DMA_LIMIT)
        return zone_kind::DMA;

    if (address < DMA32_LIMIT)
        return zone_kind::DMA32;

    return zone_kind::NORMAL;
}

[[nodiscard]] zone *zone_for_pfn(u32 pfn)
{
    if (!pfn_in_range(pfn))
        return nullptr;

    auto &page = g_allocator.pages[pfn];

    if (!page.managed || page.zone >= ZONE_COUNT)
        return nullptr;

    return &g_allocator.zones[page.zone];
}

[[nodiscard]] paddr_t zone_limit_for_address(paddr_t address)
{
    paddr_t limit = MAX_MANAGED_MEMORY_BYTES;

    switch (zone_for_address(address)) {
        case zone_kind::DMA:
            limit = DMA_LIMIT;
            break;
        case zone_kind::DMA32:
            limit = DMA32_LIMIT;
            break;
        case zone_kind::NORMAL:
        default:
            break;
    }

    return min_address(limit, g_allocator.managed_limit);
}

[[nodiscard]] bool block_fits_zone(const zone &zone, u32 pfn, u32 order)
{
    const paddr_t base = address_from_pfn(pfn);
    const paddr_t end = base + order_bytes(order);

    return base >= zone.base && end <= zone.limit && block_in_range(pfn, order);
}

void reset_zone(zone &zone, zone_kind kind, paddr_t base, paddr_t limit)
{
    zone.kind = kind;
    zone.base = base;
    zone.limit = limit;
    zone.managed_pages = 0;
    zone.free_pages = 0;
    zone.allocated_pages = 0;

    for (u32 order = 0; order <= MAX_ORDER; ++order)
        zone.areas[order] = {};
}

[[nodiscard]] constexpr paddr_t clamp_zone_base(paddr_t base, paddr_t limit)
{
    return base < limit ? base : limit;
}

void reset_allocator(paddr_t managed_limit, page *pages, u64 page_count, paddr_t metadata_base,
                     u64 metadata_length)
{
    g_allocator.initialized = false;
    g_allocator.pages = pages;
    g_allocator.page_count = page_count;
    g_allocator.managed_limit = managed_limit;
    g_allocator.metadata_base = metadata_base;
    g_allocator.metadata_length = metadata_length;

    const paddr_t dma_limit = min_address(DMA_LIMIT, managed_limit);
    const paddr_t dma32_limit = min_address(DMA32_LIMIT, managed_limit);

    reset_zone(g_allocator.zones[zone_index(zone_kind::DMA)], zone_kind::DMA, 0, dma_limit);
    reset_zone(g_allocator.zones[zone_index(zone_kind::DMA32)], zone_kind::DMA32,
               clamp_zone_base(DMA_LIMIT, dma32_limit), dma32_limit);
    reset_zone(g_allocator.zones[zone_index(zone_kind::NORMAL)], zone_kind::NORMAL,
               clamp_zone_base(DMA32_LIMIT, managed_limit), managed_limit);

    for (u64 i = 0; i < page_count; ++i)
        g_allocator.pages[i] = {};
}

[[nodiscard]] bool clamp_range_to_managed_memory(paddr_t &start, paddr_t &end)
{
    if (end <= start)
        return false;

    if (end > g_allocator.managed_limit)
        end = g_allocator.managed_limit;

    return end > start;
}

void mark_block(u32 pfn, u32 order, page_state state, bool managed, zone_kind zone_kind)
{
    for (u64 i = 0; i < order_pages(order); ++i) {
        auto &page = g_allocator.pages[pfn + i];

        page.next = INVALID_PFN;
        page.prev = INVALID_PFN;
        page.order = INVALID_ORDER;
        page.zone = static_cast<u8>(zone_kind);
        page.state = state;
        page.managed = managed;
    }

    g_allocator.pages[pfn].order = static_cast<u8>(order);
}

[[nodiscard]] bool block_has_state(u32 pfn, u32 order, page_state state, const zone &zone)
{
    if (!block_fits_zone(zone, pfn, order))
        return false;

    const auto &page = g_allocator.pages[pfn];
    return page.state == state && page.order == order && page.zone == static_cast<u8>(zone.kind);
}

[[nodiscard]] bool block_is_unmanaged_reserved(u32 pfn, u32 order, const zone &zone)
{
    if (!block_fits_zone(zone, pfn, order))
        return false;

    for (u64 i = 0; i < order_pages(order); ++i) {
        const auto &page = g_allocator.pages[pfn + i];

        if (page.managed || page.state != page_state::RESERVED)
            return false;
    }

    return true;
}

void list_push(zone &zone, u32 pfn, u32 order)
{
    mark_block(pfn, order, page_state::FREE, true, zone.kind);

    auto &area = zone.areas[order];
    auto &node = g_allocator.pages[pfn];

    node.prev = INVALID_PFN;
    node.next = area.head;

    if (area.head != INVALID_PFN)
        g_allocator.pages[area.head].prev = pfn;

    area.head = pfn;
    ++area.block_count;
    zone.free_pages += order_pages(order);
}

void list_remove(zone &zone, u32 pfn, u32 order)
{
    auto &area = zone.areas[order];
    auto &node = g_allocator.pages[pfn];

    if (node.prev != INVALID_PFN)
        g_allocator.pages[node.prev].next = node.next;
    else
        area.head = node.next;

    if (node.next != INVALID_PFN)
        g_allocator.pages[node.next].prev = node.prev;

    node.next = INVALID_PFN;
    node.prev = INVALID_PFN;
    --area.block_count;

    zone.free_pages -= order_pages(order);
}

[[nodiscard]] u32 list_pop(zone &zone, u32 order)
{
    const u32 pfn = zone.areas[order].head;
    list_remove(zone, pfn, order);
    return pfn;
}

void split_to_order(zone &zone, u32 pfn, u32 source_order, u32 target_order)
{
    while (source_order > target_order) {
        --source_order;

        const u32 buddy = pfn + static_cast<u32>(order_pages(source_order));
        list_push(zone, buddy, source_order);
    }
}

[[nodiscard]] u32 largest_fit_order(const zone &zone, u32 pfn, u64 remaining_pages)
{
    for (u32 order = MAX_ORDER; order > 0; --order) {
        if (order_pages(order) <= remaining_pages && pfn_aligned_to_order(pfn, order) &&
            block_fits_zone(zone, pfn, order))
            return order;
    }

    return 0;
}

[[nodiscard]] bool add_usable_range_locked(paddr_t base, u64 length)
{
    paddr_t start = align_up_saturating(base, PAGE_SIZE);
    paddr_t end =
        kernel::core::utils::align_down<paddr_t>(range_end_saturating(base, length), PAGE_SIZE);

    if (!clamp_range_to_managed_memory(start, end))
        return true;

    while (start < end) {
        auto         &zone = g_allocator.zones[zone_index(zone_for_address(start))];
        const paddr_t zone_end =
            zone_limit_for_address(start) < end ? zone_limit_for_address(start) : end;
        u32       pfn = pfn_from_address(start);
        const u32 end_pfn = pfn_from_address(zone_end);

        while (pfn < end_pfn) {
            const u64 remaining_pages = end_pfn - pfn;
            const u32 order = largest_fit_order(zone, pfn, remaining_pages);

            if (!block_is_unmanaged_reserved(pfn, order, zone))
                return false;

            list_push(zone, pfn, order);
            zone.managed_pages += order_pages(order);
            pfn += static_cast<u32>(order_pages(order));
        }

        start = zone_end;
    }

    return true;
}

void reserve_from_free_block(zone &zone, u32 block_pfn, u32 order, u32 target_pfn)
{
    while (order > 0) {
        --order;

        const u32 right = block_pfn + static_cast<u32>(order_pages(order));

        if (target_pfn < right) {
            list_push(zone, right, order);
        } else {
            list_push(zone, block_pfn, order);
            block_pfn = right;
        }
    }

    mark_block(block_pfn, 0, page_state::RESERVED, true, zone.kind);
}

[[nodiscard]] bool reserve_pfn_locked(u32 pfn)
{
    if (!pfn_in_range(pfn))
        return true;

    auto &target = g_allocator.pages[pfn];

    if (target.state == page_state::RESERVED)
        return true;

    if (target.state == page_state::ALLOCATED)
        return false;

    auto *zone = zone_for_pfn(pfn);
    if (!zone)
        return true;

    for (u32 order = 0; order <= MAX_ORDER; ++order) {
        const u32 block_pfn = pfn & ~static_cast<u32>(order_pages(order) - 1);

        if (!block_has_state(block_pfn, order, page_state::FREE, *zone))
            continue;

        list_remove(*zone, block_pfn, order);
        reserve_from_free_block(*zone, block_pfn, order, pfn);
        return true;
    }

    return false;
}

[[nodiscard]] bool reserve_range_locked(paddr_t base, u64 length)
{
    paddr_t start = kernel::core::utils::align_down<paddr_t>(base, PAGE_SIZE);
    paddr_t end = align_up_saturating(range_end_saturating(base, length), PAGE_SIZE);

    if (!clamp_range_to_managed_memory(start, end))
        return true;

    for (u32 pfn = pfn_from_address(start); pfn < pfn_from_address(end); ++pfn) {
        if (!reserve_pfn_locked(pfn))
            return false;
    }

    return true;
}

[[nodiscard]] paddr_t allocate_from_zone_locked(zone &zone, u32 order)
{
    if (!g_allocator.initialized || order > MAX_ORDER)
        return INVALID_PHYSICAL_ADDRESS;

    u32 source_order = order;
    while (source_order <= MAX_ORDER && zone.areas[source_order].head == INVALID_PFN)
        ++source_order;

    if (source_order > MAX_ORDER)
        return INVALID_PHYSICAL_ADDRESS;

    const u32 pfn = list_pop(zone, source_order);
    split_to_order(zone, pfn, source_order, order);
    mark_block(pfn, order, page_state::ALLOCATED, true, zone.kind);
    zone.allocated_pages += order_pages(order);
    return address_from_pfn(pfn);
}

[[nodiscard]] bool free_locked(zone &zone, paddr_t address, u32 order)
{
    if (!g_allocator.initialized || order > MAX_ORDER)
        return false;

    if (!kernel::core::utils::is_aligned<paddr_t>(address, order_bytes(order)))
        return false;

    if (!contains(address))
        return false;

    const u32 pfn = pfn_from_address(address);

    if (!block_has_state(pfn, order, page_state::ALLOCATED, zone))
        return false;

    zone.allocated_pages -= order_pages(order);
    mark_block(pfn, order, page_state::RESERVED, true, zone.kind);

    u32 block_pfn = pfn;
    u32 block_order = order;

    while (block_order < MAX_ORDER) {
        const u32 buddy = block_pfn ^ static_cast<u32>(order_pages(block_order));

        if (!block_has_state(buddy, block_order, page_state::FREE, zone))
            break;

        list_remove(zone, buddy, block_order);

        if (buddy < block_pfn)
            block_pfn = buddy;

        ++block_order;
    }

    list_push(zone, block_pfn, block_order);
    return true;
}

[[nodiscard]] bool add_boot_usable_memory(const info &boot)
{
    for (usize i = 0; i < boot.memory_map.count; ++i) {
        const auto &region = boot.memory_map[i];

        if (region.kind != memory_kind::USABLE)
            continue;

        if (!add_usable_range_locked(region.base, region.length))
            return false;
    }

    return true;
}

void log_allocatable_memory()
{
    const auto &dma = g_allocator.zones[zone_index(zone_kind::DMA)];
    const auto &dma32 = g_allocator.zones[zone_index(zone_kind::DMA32)];
    const auto &normal = g_allocator.zones[zone_index(zone_kind::NORMAL)];
    const u64   free_pages = dma.free_pages + dma32.free_pages + normal.free_pages;

    KPRINTLN("[init] allocatable={} KiB (DMA={} KiB DMA32={} KiB NORMAL={} KiB)",
             free_pages * (PAGE_SIZE / 1024), dma.free_pages * (PAGE_SIZE / 1024),
             dma32.free_pages * (PAGE_SIZE / 1024), normal.free_pages * (PAGE_SIZE / 1024));
}

[[nodiscard]] bool reserve_kernel_image()
{
    const paddr_t start = symbol_address(boot_start);
    const paddr_t end = symbol_address(kernel_physical_end);

    if (end <= start)
        return false;

    const u64 length = end - start;
    if (!reserve_range_locked(start, length))
        return false;

    return true;
}

[[nodiscard]] bool reserve_boot_info(const info &boot)
{
    for (usize i = 0; i < boot.reserved.count; ++i) {
        const auto &range = boot.reserved[i];

        if (range.length == 0)
            continue;

        if (!reserve_range_locked(range.base, range.length))
            return false;
    }

    return true;
}

[[nodiscard]] bool reserve_modules(const info &boot)
{
    for (usize i = 0; i < boot.modules.count; ++i) {
        const auto &range = boot.modules[i].range;

        if (range.length == 0)
            continue;

        if (!reserve_range_locked(range.base, range.length))
            return false;
    }

    return true;
}

[[nodiscard]] bool reserve_framebuffer(const info &boot)
{
    if (!boot.framebuffer.present)
        return true;

    const u64 pitch = boot.framebuffer.pitch;
    const u64 height = boot.framebuffer.height;

    if (height != 0 && pitch > ~u64{0} / height)
        return false;

    const u64 byte_count = pitch * height;

    if (byte_count == 0)
        return true;

    if (!reserve_range_locked(boot.framebuffer.address, byte_count))
        return false;

    return true;
}

[[nodiscard]] bool reserve_low_memory()
{
    if (!reserve_range_locked(0, LOW_MEMORY_SIZE))
        return false;

    return true;
}

[[nodiscard]] bool reserve_metadata_storage()
{
    if (g_allocator.metadata_base == 0 || g_allocator.metadata_length == 0)
        return false;

    return reserve_range_locked(g_allocator.metadata_base, g_allocator.metadata_length);
}

[[nodiscard]] bool reserve_boot_ranges(const info &boot)
{
    return reserve_kernel_image() && reserve_boot_info(boot) && reserve_modules(boot) &&
           reserve_framebuffer(boot) && reserve_low_memory() && reserve_metadata_storage();
}

}  // namespace

bool initialized()
{
    return g_allocator.initialized;
}

stats current_stats()
{
    all_zone_locks_guard guard{};

    stats result{
        .initialized = g_allocator.initialized,
        .managed_pages = 0,
        .free_pages = 0,
        .allocated_pages = 0,
        .zones = {},
    };

    for (usize i = 0; i < ZONE_COUNT; ++i) {
        const auto &zone = g_allocator.zones[i];

        result.zones[i] = zone_stats{
            .base = zone.base,
            .limit = zone.limit,
            .managed_pages = zone.managed_pages,
            .free_pages = zone.free_pages,
            .allocated_pages = zone.allocated_pages,
        };

        result.managed_pages += zone.managed_pages;
        result.free_pages += zone.free_pages;
        result.allocated_pages += zone.allocated_pages;
    }

    return result;
}

paddr_t alloc_pages(zone_kind zone_kind, u32 order)
{
    if (zone_index(zone_kind) >= ZONE_COUNT)
        return INVALID_PHYSICAL_ADDRESS;

    auto &zone = g_allocator.zones[zone_index(zone_kind)];

    spinlock_guard guard{zone.lock};
    return allocate_from_zone_locked(zone, order);
}

paddr_t alloc_pages(u32 order)
{
    constexpr zone_kind ORDER[] = {
        zone_kind::NORMAL,
        zone_kind::DMA32,
        zone_kind::DMA,
    };

    for (auto zone_kind : ORDER) {
        auto          &zone = g_allocator.zones[zone_index(zone_kind)];
        spinlock_guard guard{zone.lock};

        const paddr_t page = allocate_from_zone_locked(zone, order);
        if (page != INVALID_PHYSICAL_ADDRESS)
            return page;
    }

    return INVALID_PHYSICAL_ADDRESS;
}

paddr_t alloc_page()
{
    return alloc_pages(0);
}

paddr_t alloc_page(zone_kind zone)
{
    return alloc_pages(zone, 0);
}

bool free_pages(paddr_t address, u32 order)
{
    if (!contains(address))
        return false;

    auto &zone = g_allocator.zones[zone_index(zone_for_address(address))];

    spinlock_guard guard{zone.lock};
    return free_locked(zone, address, order);
}

bool free_page(paddr_t address)
{
    return free_pages(address, 0);
}

bool reserve_range(paddr_t base, u64 length)
{
    if (!g_allocator.initialized)
        return false;

    all_zone_locks_guard guard{};
    return reserve_range_locked(base, length);
}

bool contains(paddr_t address)
{
    return address < g_allocator.managed_limit;
}

kernel::core::init_result component::init_allocator()
{
    using kernel::core::init_error;

    const auto &boot = kernel::boot::boot_info::current();

    if (!boot.valid)
        return kernel::core::Err(init_error::DEPENDENCY_UNAVAILABLE);

    const paddr_t managed_limit = managed_limit_from_boot_memory(boot);
    const u64     page_count = managed_limit / PAGE_SIZE;
    paddr_t       metadata_base = 0;
    u64           metadata_length = 0;

    if (page_count > MAX_MANAGED_PAGES)
        return kernel::core::Err(init_error::CAPACITY_EXCEEDED);

    if (!choose_metadata_storage(boot, managed_limit, page_count, metadata_base, metadata_length))
        return kernel::core::Err(init_error::METADATA_STORAGE_UNAVAILABLE);

    reset_allocator(managed_limit, reinterpret_cast<page *>(metadata_base), page_count,
                    metadata_base, metadata_length);

    if (!add_boot_usable_memory(boot))
        return kernel::core::Err(init_error::NO_USABLE_MEMORY);

    if (!reserve_boot_ranges(boot))
        return kernel::core::Err(init_error::RESERVATION_FAILED);

    g_allocator.initialized = true;
    log_allocatable_memory();

    return kernel::core::Ok();
}

}  // namespace kernel::core::memory::pmm
