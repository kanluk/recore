#pragma once

#include <string>

#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace recore::scene {

struct TagComponent {
  std::string tag{"Unnamed Entity"};

  TagComponent() = default;

  explicit TagComponent(std::string tag) : tag{std::move(tag)} {}
};

struct TransformComponent {
  glm::vec3 translation{0.f, 0.f, 0.f};
  glm::vec3 rotation{0.f, 0.f, 0.f};
  glm::vec3 scale{1.f, 1.f, 1.f};

  TransformComponent() = default;

  explicit TransformComponent(const glm::mat4& transform) {
    glm::quat quatRotation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(
        transform, scale, quatRotation, translation, skew, perspective);
    quatRotation = glm::conjugate(quatRotation);
    rotation = glm::eulerAngles(quatRotation);
  }

  explicit TransformComponent(const glm::vec3& translation)
      : translation{translation} {}

  [[nodiscard]] glm::mat4 getMat4() const {
    return glm::translate(glm::mat4(1.f), translation) *
           glm::toMat4(glm::quat(rotation)) * glm::scale(glm::mat4(1.f), scale);
  }
};

struct SceneNodeComponent {
  uint32_t modelMatrixID;
};

}  // namespace recore::scene
