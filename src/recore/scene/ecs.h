#pragma once

#include <recore/core/base.h>

#include <entt/entt.hpp>

namespace recore::scene {

// Simple EnTT ECS object wrapper...

class Entity;

class ECS : public NoCopyMove {
 public:
  ECS() = default;
  ~ECS() = default;

  [[nodiscard]] Entity createEntity(const std::string& name);
  void destroyEntity(Entity entity);

  [[nodiscard]] std::optional<Entity> getByName(const std::string& name);

  [[nodiscard]] entt::registry& getRegistry() { return mRegistry; }

 private:
  entt::registry mRegistry;

  friend class Entity;
};

class Entity {
 public:
  Entity(ECS& ecs, entt::entity handle) : mECS{ecs}, mHandle{handle} {}

  template <typename T, typename... Args>
  T& addComponent(Args&&... args);

  template <typename T>
  T& getComponent();

  template <typename T>
  bool hasComponent();

  template <typename T>
  void removeComponent();

  explicit operator bool() const;

  explicit operator entt::entity() const;

  explicit operator uint32_t() const;

  bool operator==(const Entity& other) const;

  bool operator!=(const Entity& other) const;

 private:
  ECS& mECS;
  entt::entity mHandle;
};

template <typename T, typename... Args>
T& Entity::addComponent(Args&&... args) {
  return mECS.mRegistry.emplace<T>(mHandle, std::forward<Args>(args)...);
}

template <typename T>
T& Entity::getComponent() {
  return mECS.mRegistry.get<T>(mHandle);
}

template <typename T>
bool Entity::hasComponent() {
  return mECS.mRegistry.all_of<T>(mHandle);
}

template <typename T>
void Entity::removeComponent() {
  mECS.mRegistry.remove<T>(mHandle);
}

}  // namespace recore::scene
