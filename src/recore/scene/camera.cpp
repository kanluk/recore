#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace recore::scene {

Camera::Camera() : Camera(Desc{}) {}

Camera::Camera(const Desc& desc)
    : mPosition{desc.position},
      mRotation{desc.rotation},
      mFov{desc.fov},
      mNear{desc.near},
      mFar{desc.far},
      mAspect{desc.aspect} {}

glm::mat4 Camera::getView() const {
  return glm::inverse(glm::translate(glm::mat4{1.f}, mPosition) *
                      glm::toMat4(glm::quat(mRotation)));
}

glm::mat4 Camera::getProjection() const {
  auto projection = glm::perspective(glm::radians(mFov), mAspect, mNear, mFar);
  projection[1][1] *= -1;
  return projection;
}

glm::mat4 Camera::getViewProjection() const {
  return getProjection() * getView();
}

void Camera::setAspect(float aspect) {
  mAspect = aspect;
}

void Camera::setAspect(uint32_t width, uint32_t height) {
  mAspect = static_cast<float>(width) / static_cast<float>(height);
  mWidth = width;
  mHeight = height;
}

bool Camera::hasMoved() {
  // TODO: create separate method to reset moved flag
  bool moved = mMoved;
  mMoved = false;
  return moved;
}

}  // namespace recore::scene
