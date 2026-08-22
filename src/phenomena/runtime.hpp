#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace phenomena {

using EntityId = std::uint64_t;

enum class EntityState {
    Alive,
    Deceased,
    Transcended
};

struct Entity {
    EntityId id{};
    std::string name;
    EntityState state{EntityState::Alive};
};

class World final {
public:
    explicit World(std::string world_id);

    [[nodiscard]] const std::string& id() const noexcept;

    EntityId create_entity(std::string name);

    [[nodiscard]] Entity* find_entity(EntityId id) noexcept;
    [[nodiscard]] const Entity* find_entity(EntityId id) const noexcept;

    void set_entity_state(EntityId id, EntityState state) noexcept;

    [[nodiscard]] std::size_t entity_count() const noexcept;

    void tick(double delta_seconds) noexcept;

    [[nodiscard]] double simulation_time() const noexcept;

private:
    std::string world_id_;
    double simulation_time_{0.0};
    EntityId next_entity_id_{1};

    std::unordered_map<EntityId, Entity> entities_;
};

class Runtime final {
public:
    Runtime();

    void initialize();
    void update(double delta_seconds) noexcept;
    void shutdown() noexcept;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] World& world() noexcept;
    [[nodiscard]] const World& world() const noexcept;

private:
    bool initialized_{false};
    World world_;
};

} // namespace phenomena
