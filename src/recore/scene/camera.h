#pragma once

#include <glm/glm.hpp>

namespace recore::scene {

class Camera {
 public:
  struct Desc {
    glm::vec3 position{0.f, 0.f, 0.f};
    glm::vec3 rotation{0.f, 0.f, 0.f};
    float fov = 45.f;
    float near = 0.1f;
    float far = 100.f;
    float aspect = 1.f;
  };

  Camera();

  explicit Camera(const Desc& desc);

  void setPosition(const glm::vec3& position) {
    mPosition = position;
    mMoved = true;
  }

  void setRotation(const glm::vec3& rotation) {
    mRotation = rotation;
    mMoved = true;
  }

  [[nodiscard]] const glm::vec3& getPosition() const { return mPosition; }

  [[nodiscard]] const glm::vec3& getRotation() const { return mRotation; }

  [[nodiscard]] float getFov() const { return mFov; }

  [[nodiscard]] float getNear() const { return mNear; }

  [[nodiscard]] float getFar() const { return mFar; }

  [[nodiscard]] float getAspect() const { return mAspect; }

  [[nodiscard]] uint32_t getWidth() const { return mWidth; }

  [[nodiscard]] uint32_t getHeight() const { return mHeight; }

  [[nodiscard]] glm::mat4 getView() const;
  [[nodiscard]] glm::mat4 getProjection() const;
  [[nodiscard]] glm::mat4 getViewProjection() const;

  void setAspect(float aspect);
  void setAspect(uint32_t width, uint32_t height);

  bool hasMoved();

 private:
  glm::vec3 mPosition;
  glm::vec3 mRotation;
  float mFov;
  float mNear;
  float mFar;
  float mAspect;

  uint32_t mWidth;
  uint32_t mHeight;

  bool mMoved = true;
};

}  // namespace recore::scene
