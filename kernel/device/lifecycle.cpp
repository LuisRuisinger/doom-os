// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/device/lifecycle.hpp"

#include <uk/capability.hpp>

#include "arch/x86_64/cpu/cpu.hpp"
#include "kernel/debug/kprint.hpp"

namespace uk {

namespace detail {

[[noreturn]] void result_error(const char *message)
{
    KPANIC(message);
}

[[noreturn]] void panic_missing_capability(const capability_key &key)
{
    KPANIC("requested driver capability is not registered: {}", key.name);
}

}  // namespace detail

namespace {

using kernel::core::u8;
using kernel::core::usize;

static constexpr usize MAX_DRIVERS = 128;
static constexpr usize MAX_REGISTERED_CAPABILITIES = 256;

enum class visit_state : u8 {
    NOT_VISITED,
    VISITING,
    VISITED,
};

struct driver_node {
    const driver_id *id{};
    init_fn          init{};
    exit_fn          exit{};
    visit_state      visit{};
    bool             initialized{};
};

struct registered_capability {
    const capability_key *key;
    const driver_id      *driver;
    const void           *api;
};

struct lifecycle_state {
    driver_node nodes[MAX_DRIVERS];
    usize       node_count;

    driver_node *initialized[MAX_DRIVERS];
    usize        initialized_count;

    registered_capability capabilities[MAX_REGISTERED_CAPABILITIES];
    usize                 capability_count;

    bool built;
    bool initialized_all;
};

static lifecycle_state m_state{};

bool string_equal(const char *lhs, const char *rhs)
{
    if (lhs == nullptr || rhs == nullptr) {
        return lhs == rhs;
    }

    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) {
            return false;
        }

        ++lhs;
        ++rhs;
    }

    return *lhs == *rhs;
}

bool same_capability(const capability_key &lhs, const capability_key &rhs)
{
    return string_equal(lhs.name, rhs.name);
}

bool is_singleton(const capability_key &key)
{
    return key.multiplicity == capability_multiplicity::SINGLE;
}

driver_node *find_node(const driver_id *id)
{
    if (id == nullptr) {
        return nullptr;
    }

    for (usize i = 0; i < m_state.node_count; ++i) {
        if (m_state.nodes[i].id == id || string_equal(m_state.nodes[i].id->name, id->name)) {
            return &m_state.nodes[i];
        }
    }

    return nullptr;
}

driver_node *find_node(const char *name)
{
    if (name == nullptr) {
        return nullptr;
    }

    for (usize i = 0; i < m_state.node_count; ++i) {
        if (string_equal(m_state.nodes[i].id->name, name)) {
            return &m_state.nodes[i];
        }
    }

    return nullptr;
}

init_result add_driver(const driver_id &id)
{
    if (id.name == nullptr) {
        return kernel::core::Err(error::INVALID_RECORD);
    }

    if (find_node(id.name) != nullptr) {
        return kernel::core::Err(error::DUPLICATE_DRIVER);
    }

    if (m_state.node_count == MAX_DRIVERS) {
        return kernel::core::Err(error::CAPACITY_EXCEEDED);
    }

    m_state.nodes[m_state.node_count] = driver_node{.id = &id};
    ++m_state.node_count;

    return kernel::core::Ok();
}

usize provider_record_count(const capability_key &key)
{
    usize count = 0;

    for (const provide_record &record : registry::provide_records()) {
        if (record.capability != nullptr && same_capability(*record.capability, key)) {
            ++count;
        }
    }

    return count;
}

init_result validate_provider_count(const capability_key &key)
{
    if (is_singleton(key) && provider_record_count(key) > 1) {
        return kernel::core::Err(error::MULTIPLE_CAPABILITY_PROVIDERS);
    }

    return kernel::core::Ok();
}

init_result attach_init_records()
{
    for (const init_record &record : registry::init_records()) {
        if (record.driver == nullptr || record.init == nullptr) {
            return kernel::core::Err(error::INVALID_RECORD);
        }

        driver_node *node = find_node(record.driver);

        if (node == nullptr) {
            return kernel::core::Err(error::UNKNOWN_DRIVER);
        }

        if (node->init != nullptr) {
            return kernel::core::Err(error::DUPLICATE_DRIVER_ENTRY);
        }

        node->init = record.init;
    }

    return kernel::core::Ok();
}

init_result attach_exit_records()
{
    for (const exit_record &record : registry::exit_records()) {
        if (record.driver == nullptr || record.exit == nullptr) {
            return kernel::core::Err(error::INVALID_RECORD);
        }

        driver_node *node = find_node(record.driver);

        if (node == nullptr) {
            return kernel::core::Err(error::UNKNOWN_DRIVER);
        }

        if (node->exit != nullptr) {
            return kernel::core::Err(error::DUPLICATE_DRIVER_EXIT);
        }

        node->exit = record.exit;
    }

    return kernel::core::Ok();
}

init_result validate_dependency_records()
{
    for (const require_driver_record &record : registry::require_driver_records()) {
        if (record.driver == nullptr || record.dependency_name == nullptr) {
            return kernel::core::Err(error::INVALID_RECORD);
        }

        if (find_node(record.driver) == nullptr) {
            return kernel::core::Err(error::UNKNOWN_DRIVER);
        }

        if (find_node(record.dependency_name) == nullptr) {
            return kernel::core::Err(error::UNKNOWN_DRIVER_DEPENDENCY);
        }
    }

    for (const require_capability_record &record : registry::require_capability_records()) {
        if (record.driver == nullptr || record.capability == nullptr ||
            record.capability->name == nullptr) {
            return kernel::core::Err(error::INVALID_RECORD);
        }

        if (find_node(record.driver) == nullptr) {
            return kernel::core::Err(error::UNKNOWN_DRIVER);
        }

        init_result provider_count = validate_provider_count(*record.capability);

        if (provider_count.is_err()) {
            return provider_count;
        }

        if (provider_record_count(*record.capability) == 0) {
            return kernel::core::Err(error::MISSING_CAPABILITY_PROVIDER);
        }
    }

    return kernel::core::Ok();
}

init_result validate_provider_records()
{
    for (const provide_record &record : registry::provide_records()) {
        if (record.driver == nullptr || record.capability == nullptr ||
            record.capability->name == nullptr || record.resolve == nullptr) {
            return kernel::core::Err(error::INVALID_RECORD);
        }

        if (find_node(record.driver) == nullptr) {
            return kernel::core::Err(error::UNKNOWN_DRIVER);
        }

        init_result provider_count = validate_provider_count(*record.capability);

        if (provider_count.is_err()) {
            return provider_count;
        }
    }

    return kernel::core::Ok();
}

init_result build_registry()
{
    if (m_state.built) {
        return kernel::core::Ok();
    }

    m_state = lifecycle_state{};

    for (const driver_id &id : registry::drivers()) {
        init_result outcome = add_driver(id);

        if (outcome.is_err()) {
            return outcome;
        }
    }

    init_result init_records = attach_init_records();

    if (init_records.is_err()) {
        return init_records;
    }

    init_result exit_records = attach_exit_records();

    if (exit_records.is_err()) {
        return exit_records;
    }

    init_result providers = validate_provider_records();

    if (providers.is_err()) {
        return providers;
    }

    init_result dependencies = validate_dependency_records();

    if (dependencies.is_err()) {
        return dependencies;
    }

    m_state.built = true;

    return kernel::core::Ok();
}

init_result register_capabilities(driver_node &node)
{
    for (const provide_record &record : registry::provide_records()) {
        if (!string_equal(record.driver->name, node.id->name)) {
            continue;
        }

        const void *api = record.resolve();

        if (api == nullptr) {
            return kernel::core::Err(error::CAPABILITY_RESOLVE_FAILED);
        }

        if (m_state.capability_count == MAX_REGISTERED_CAPABILITIES) {
            return kernel::core::Err(error::CAPACITY_EXCEEDED);
        }

        m_state.capabilities[m_state.capability_count] = registered_capability{
            .key = record.capability,
            .driver = node.id,
            .api = api,
        };
        ++m_state.capability_count;
    }

    return kernel::core::Ok();
}

init_result visit(driver_node &node);

init_result visit_capability_providers(const capability_key &key)
{
    init_result provider_count = validate_provider_count(key);

    if (provider_count.is_err()) {
        return provider_count;
    }

    bool found = false;

    for (const provide_record &record : registry::provide_records()) {
        if (record.capability == nullptr || !same_capability(*record.capability, key)) {
            continue;
        }

        driver_node *provider = find_node(record.driver);

        if (provider == nullptr) {
            return kernel::core::Err(error::UNKNOWN_DRIVER);
        }

        found = true;

        init_result outcome = visit(*provider);

        if (outcome.is_err()) {
            return outcome;
        }
    }

    if (!found) {
        return kernel::core::Err(error::MISSING_CAPABILITY_PROVIDER);
    }

    return kernel::core::Ok();
}

init_result visit_driver_dependencies(driver_node &node)
{
    for (const require_driver_record &record : registry::require_driver_records()) {
        if (!string_equal(record.driver->name, node.id->name)) {
            continue;
        }

        driver_node *dependency = find_node(record.dependency_name);

        if (dependency == nullptr) {
            return kernel::core::Err(error::UNKNOWN_DRIVER_DEPENDENCY);
        }

        init_result outcome = visit(*dependency);

        if (outcome.is_err()) {
            return outcome;
        }
    }

    return kernel::core::Ok();
}

init_result visit_capability_dependencies(driver_node &node)
{
    for (const require_capability_record &record : registry::require_capability_records()) {
        if (!string_equal(record.driver->name, node.id->name)) {
            continue;
        }

        init_result outcome = visit_capability_providers(*record.capability);

        if (outcome.is_err()) {
            return outcome;
        }
    }

    return kernel::core::Ok();
}

init_result run_driver(driver_node &node)
{
    KPRINTLN("[driver] {}", node.id->name);

    if (node.init != nullptr) {
        init_result outcome = node.init();

        if (outcome.is_err()) {
            KPRINTLN("[driver] {}: FAIL ({})", node.id->name, describe(outcome.unwrap_err_ref()));
            return outcome;
        }
    }

    init_result capabilities = register_capabilities(node);

    if (capabilities.is_err()) {
        KPRINTLN("[driver] {}: FAIL ({})", node.id->name, describe(capabilities.unwrap_err_ref()));
        return capabilities;
    }

    node.initialized = true;
    m_state.initialized[m_state.initialized_count] = &node;
    ++m_state.initialized_count;

    KPRINTLN("[driver] {}: OK", node.id->name);

    return kernel::core::Ok();
}

init_result visit(driver_node &node)
{
    if (node.visit == visit_state::VISITED) {
        return kernel::core::Ok();
    }

    if (node.visit == visit_state::VISITING) {
        return kernel::core::Err(error::DEPENDENCY_CYCLE);
    }

    node.visit = visit_state::VISITING;

    init_result driver_dependencies = visit_driver_dependencies(node);

    if (driver_dependencies.is_err()) {
        node.visit = visit_state::NOT_VISITED;
        return driver_dependencies;
    }

    init_result capability_dependencies = visit_capability_dependencies(node);

    if (capability_dependencies.is_err()) {
        node.visit = visit_state::NOT_VISITED;
        return capability_dependencies;
    }

    init_result outcome = run_driver(node);

    if (outcome.is_err()) {
        node.visit = visit_state::NOT_VISITED;
        return outcome;
    }

    node.visit = visit_state::VISITED;

    return kernel::core::Ok();
}

}  // namespace

const char *describe(error value)
{
    switch (value) {
        case error::UNSPECIFIED:
            return "unspecified failure";
        case error::CAPACITY_EXCEEDED:
            return "driver registry capacity exceeded";
        case error::INVALID_RECORD:
            return "invalid driver metadata record";
        case error::DUPLICATE_DRIVER:
            return "duplicate driver name";
        case error::DUPLICATE_DRIVER_ENTRY:
            return "driver has more than one entry point";
        case error::DUPLICATE_DRIVER_EXIT:
            return "driver has more than one exit point";
        case error::UNKNOWN_DRIVER:
            return "metadata references an unknown driver";
        case error::UNKNOWN_DRIVER_DEPENDENCY:
            return "driver depends on an unknown driver";
        case error::MISSING_CAPABILITY_PROVIDER:
            return "driver capability has no provider";
        case error::MULTIPLE_CAPABILITY_PROVIDERS:
            return "singleton driver capability has multiple providers";
        case error::DEPENDENCY_CYCLE:
            return "driver dependency cycle detected";
        case error::CAPABILITY_RESOLVE_FAILED:
            return "driver capability resolver returned null";
    }

    return "unknown driver error";
}

init_result init_all()
{
    if (m_state.initialized_all) {
        return kernel::core::Ok();
    }

    init_result registry = build_registry();

    if (registry.is_err()) {
        KPRINTLN("[driver] registry: FAIL ({})", describe(registry.unwrap_err_ref()));
        return registry;
    }

    for (usize i = 0; i < m_state.node_count; ++i) {
        init_result outcome = visit(m_state.nodes[i]);

        if (outcome.is_err()) {
            return outcome;
        }
    }

    m_state.initialized_all = true;

    return kernel::core::Ok();
}

void exit_all()
{
    for (usize i = m_state.initialized_count; i > 0; --i) {
        driver_node *node = m_state.initialized[i - 1];

        if (node == nullptr || !node->initialized) {
            continue;
        }

        if (node->exit != nullptr) {
            KPRINTLN("[driver] {}: exit", node->id->name);
            node->exit();
        }

        node->initialized = false;
    }

    m_state.initialized_count = 0;
    m_state.capability_count = 0;
    m_state.initialized_all = false;
}

void init_all_or_halt()
{
    init_result outcome = init_all();

    if (outcome.is_ok()) {
        return;
    }

    kernel::arch::x86_64::cpu::halt();
}

const void *find_capability(const capability_key &key)
{
    for (usize i = 0; i < m_state.capability_count; ++i) {
        if (m_state.capabilities[i].key != nullptr &&
            same_capability(*m_state.capabilities[i].key, key)) {
            return m_state.capabilities[i].api;
        }
    }

    return nullptr;
}

}  // namespace uk
