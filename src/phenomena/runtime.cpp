#include "runtime.hpp"

#include <utility>

namespace phenomena {

World::World(std::string world_id)
    : world_id_(std::move(world_id))
{
}

const std::string& World::id() const noexcept
{
    return world_id_;
}

EntityId World::create_entity(std::string name)
{
    const EntityId id = next_entity_id_++;

    entities_.emplace(
        id,
        Entity{
            .id = id,
            .name = std::move(name),
            .state = EntityState::Alive
        }
    );

    return id;
}

Entity* World::find_entity(EntityId id) noexcept
{
    const auto it = entities_.find(id);

    if (it == entities_.end()) {
        return nullptr;
    }

    return &it->second;
}

const Entity* World::find_entity(EntityId id) const noexcept
{
    const auto it = entities_.find(id);

    if (it == entities_.end()) {
        return nullptr;
    }

    return &it->second;
}

void World::set_entity_state(
    EntityId id,
    EntityState state
) noexcept
{
    if (auto* entity = find_entity(id)) {
        entity->state = state;
    }
}

std::size_t World::entity_count() const noexcept
{
    return entities_.size();
}

void World::tick(double delta_seconds) noexcept
{
    if (delta_seconds > 0.0) {
        simulation_time_ += delta_seconds;
    }
}

double World::simulation_time() const noexcept
{
    return simulation_time_;
}

Runtime::Runtime()
    : world_("phenomena-primary")
{
}

void Runtime::initialize()
{
    if (initialized_) {
        return;
    }

    world_.create_entity("Phenomena World");
    initialized_ = true;
}

void Runtime::update(double delta_seconds) noexcept
{
    if (!initialized_) {
        return;
    }

    world_.tick(delta_seconds);
}

void Runtime::shutdown() noexcept
{
    initialized_ = false;
}

bool Runtime::initialized() const noexcept
{
    return initialized_;
}

World& Runtime::world() noexcept
{
    return world_;
}

const World& Runtime::world() const noexcept
{
    return world_;
}

} // namespace phenomena
