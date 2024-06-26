#include <iostream>
#include <cmath>

#include "Camera.hpp"

// Returns the view matrix calculated using euler angles and the lookat matrix
glm::mat4 Camera::get_view_matrix () const {
  return glm::lookAt(center + orientation*distance, center, up);
}

// Returns the projection matrix, given an aspect ratio
glm::mat4 Camera::get_projection_matrix (float aspect_ratio, float min, float max) const {
  return glm::perspective(glm::radians(fov), aspect_ratio, min, max);
}

void Camera::process_keyboard (Direction direction, float delta_time) {
  switch (direction) {
    case Direction::Forward:
      pitch += pitch_speed*delta_time;
      break;
    case Direction::Backward:
      pitch -= pitch_speed*delta_time;
      break;
    case Direction::Left:
      yaw += yaw_speed*delta_time;
      break;
    case Direction::Right:
      yaw -= yaw_speed*delta_time;
      break;
    case Direction::Up:
    case Direction::Down:
      break;
  }
  update_vectors();
}

// Processes input received from a mouse input system. Expects the offset value in both the x and y direction.
void Camera::process_mouse_movement (float xoffset, float yoffset, CameraMovement movement_type) {
  switch (movement_type) {
    case CameraMovement::Pan:
      xoffset *= pan_sensitivity;
      yaw -= xoffset;
      break;
    case CameraMovement::Orbit:
      xoffset *= orbit_sensitivity;
      yoffset *= orbit_sensitivity;
      yaw += xoffset;
      pitch += yoffset;
      break;
  }

  // update Front, Right and Up Vectors using the updated Euler angles
  update_vectors(movement_type);
}

// processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
void Camera::process_mouse_scroll (float yoffset) {
  distance -= zoom_speed*yoffset;
  distance = std::min(max_zoom_distance, std::max(min_zoom_distance, distance));
  update_vectors();
}

// After a change, update camera orientation and position vectors
void Camera::update_vectors (CameraMovement movement_type) {
  // make sure that when pitch is out of bounds, screen doesn't get flipped
  pitch = std::min(89.0f, std::max(-89.0f, pitch));

  // Constrain yaw to [0, 360) for to avoid floating point issues at high angles
  yaw = fmodf(yaw, 360.0f);

  switch (movement_type) {
    case CameraMovement::Orbit: {
      yaw = fmodf(yaw, 360.0f);
      // calculates the front vector from the camera's (updated) Euler Angles
      orientation.x = cosf(glm::radians(yaw))*cosf(glm::radians(pitch));
      orientation.y = sinf(glm::radians(pitch));
      orientation.z = sinf(glm::radians(yaw))*cosf(glm::radians(pitch));
      orientation = glm::normalize(orientation);

      break;
    }
    case CameraMovement::Pan: {
      auto pos = center + distance*orientation;

      orientation.x = cosf(glm::radians(yaw))*cosf(glm::radians(pitch));
      orientation.y = sinf(glm::radians(pitch));
      orientation.z = sinf(glm::radians(yaw))*cosf(glm::radians(pitch));
      orientation = glm::normalize(orientation);

      center = pos - distance*orientation;

      break;
    }
  }

  front = center - distance*orientation;

  // Also calculate right and up vector
  right = glm::normalize(glm::cross(front, world_up));
  up = glm::normalize(glm::cross(right, front));
}