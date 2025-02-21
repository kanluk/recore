#include "ecs.h"

#include "ecs_components.h"

namespace recore::scene {

Entity ECS::createEntity(const std::string& name) {
  Entity entity{*this, mRegistry.create()};
  auto& tag = entity.addComponent<TagComponent>(name);
  return entity;
}

void ECS::destroyEntity(Entity entity) {
  mRegistry.destroy(static_cast<entt::entity>(entity));
}

std::optional<Entity> ECS::getByName(const std::string& name) {
  auto view = mRegistry.view<TagComponent>();
  for (auto&& [entity, tag] : view.each()) {
    if (tag.tag == name) {
      return Entity{*this, entity};
    }
  }
  return {};
}

Entity::operator bool() const {
  return mHandle != entt::null;
}

Entity::operator entt::entity() const {
  return mHandle;
}

Entity::operator uint32_t() const {
  return (uint32_t)mHandle;
}

bool Entity::operator==(const Entity& other) const {
  return mHandle == other.mHandle && &mECS == &other.mECS;
}

bool Entity::operator!=(const Entity& other) const {
  return !(*this == other);
}

}  // namespace recore::scene
