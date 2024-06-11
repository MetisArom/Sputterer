#pragma once
#ifndef SPUTTERER_CAMERA_HPP
#define SPUTTERER_CAMERA_HPP

#include <iosfwd>

#include <glad/glad.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "vec3.hpp"

//! Camera Movement enum classes
enum class Direction {
  Forward, Backward, Left, Right, Up, Down
};

enum class CameraMovement {
  Pan, Orbit
};

//! Default camera values
constexpr auto default_yaw = -90.0f;
constexpr auto default_pitch = 0.0f;

constexpr auto default_pan_sensitivity = 0.05f;
constexpr auto default_orbit_sensitivity = 0.1f;

constexpr auto default_fov = 75.0f;
constexpr auto default_yaw_speed = 100.0f;
constexpr auto default_pitch_speed = 100.0f;
constexpr auto default_zoom_speed = 0.5f;

constexpr auto min_zoom_distance = 0.1f;
constexpr auto max_zoom_distance = 20.0f;

class Camera {
public:
  //! camera attributes
  vec3 orientation;
  vec3 front{0.0f, 0.0f, -1.0f};
  vec3 up{0.0f, 1.0f, 0.0f};
  vec3 right{1.0f, 0.0f, 0.0f};
  vec3 world_up{0.0f, 1.0f, 0.0f};
  vec3 center{0.0f, 0.0f, 0.0f};

  //! Euler angles
  float yaw{default_yaw};
  float pitch{default_pitch};
  float distance;

  //! camera options
  float pitch_speed{default_pitch_speed};
  float yaw_speed{default_yaw_speed};
  float orbit_sensitivity{default_orbit_sensitivity};
  float pan_sensitivity{default_pan_sensitivity};
  float zoom_speed{default_zoom_speed};
  float fov{default_fov};

  //! Returns the view matrix calculated using euler angles and the glfw look at matrix
  [[nodiscard]] glm::mat4 get_view_matrix () const;

  //! Returns the projection matrix, given an aspect ratio
  [[nodiscard]] glm::mat4 get_projection_matrix (float aspect_ratio, float min = 0.1f, float max = 100.0f) const;

  //! Processes input received from keyboard. Expects a movement direction and a timestep_s.
  void process_keyboard (Direction direction, float delta_time);

  //! Processes input received from a mouse input system. Expects the offset value in both the x and y direction.
  void process_mouse_movement (float xoffset, float yoffset, CameraMovement movement_type);

  //! processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
  void process_mouse_scroll (float yoffset);

  //! After a change, update camera orientation and position vectors
  void update_vectors (CameraMovement movement_type = CameraMovement::Orbit);

  //! Constructor with vectors
  explicit Camera (vec3 position = {0.0f, 0.0f, 0.0f})
    : orientation(glm::normalize(position)), distance(glm::length(position)) {
    update_vectors();
  }
};

#endif