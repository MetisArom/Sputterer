#ifndef SPUTTERER_APP_HPP
#define SPUTTERER_APP_HPP

#include "Camera.hpp"

namespace app {
  // settings
  constexpr unsigned int screen_width = 1360;
  constexpr unsigned int screen_height = 768;
  float aspect_ratio = static_cast<float>(screen_width)/static_cast<float>(screen_height);

  Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

  float delta_time = 0.0f;
  float last_x = 0.0;
  float last_y = 0.0;

  //! Function for processing key inputs to window
  void process_input (GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, true);
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
      camera.process_keyboard(Direction::Forward, delta_time);
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
      camera.process_keyboard(Direction::Backward, delta_time);
    }

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
      camera.process_keyboard(Direction::Left, delta_time);
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
      camera.process_keyboard(Direction::Right, delta_time);
    }

    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
      camera.process_keyboard(Direction::Up, delta_time);
    }

    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
      camera.process_keyboard(Direction::Down, delta_time);
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
      camera.center = glm::vec3{0.0};
    }
  }

  bool orbiting = false;
  bool panning = false;

  void mouse_cursor_callback (GLFWwindow *window, double xpos_in, double ypos_in) {

    auto x_pos = static_cast<float>(xpos_in);
    auto y_pos = static_cast<float>(ypos_in);

    const auto orbit_button = GLFW_MOUSE_BUTTON_RIGHT;
    const auto pan_button = GLFW_MOUSE_BUTTON_MIDDLE;

    // Detection for left mouse drag/release
    if (glfwGetMouseButton(window, orbit_button) == GLFW_RELEASE &&
        glfwGetMouseButton(window, pan_button) == GLFW_RELEASE) {
      if (orbiting || panning) {
        orbiting = false;
        panning = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      }
      return;
    } else if (glfwGetMouseButton(window, pan_button) == GLFW_RELEASE) {
      if (panning) {
        panning = false;
      }
      if (!orbiting) {
        orbiting = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        last_x = x_pos;
        last_y = y_pos;
      }
    } else {
      if (orbiting) {
        orbiting = false;
      }
      if (!panning) {
        panning = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        last_x = x_pos;
        last_y = y_pos;
      }
    }

    float offset_x = x_pos - last_x;
    float offset_y = y_pos - last_y;

    last_x = x_pos;
    last_y = y_pos;

    if (orbiting) {
      camera.process_mouse_movement(offset_x, offset_y, CameraMovement::Orbit);
    } else if (panning) {
      camera.process_mouse_movement(offset_x, offset_y, CameraMovement::Pan);
    }
  }

  void scroll_callback ([[maybe_unused]] GLFWwindow *window, [[maybe_unused]] double xoffset, double yoffset) {
    camera.process_mouse_scroll(static_cast<float>(yoffset));
  }

  void framebuffer_size_callback ([[maybe_unused]] GLFWwindow *window, int width, int height) {
    aspect_ratio = static_cast<float>(width)/static_cast<float>(height);
    glViewport(0, 0, width, height);
  }

} // namespace app

#endif