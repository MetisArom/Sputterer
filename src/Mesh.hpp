#pragma once

#ifndef SPUTTERER_MESH_HPP
#define SPUTTERER_MESH_HPP

// Standard libraries
#include <cstddef>
#include <string>
#include <vector>
#include <memory>

#include "vec3.hpp"

using std::string, std::vector;

//! Struct for defining object vertices
struct Vertex {
  vec3 pos;
  vec3 norm;

  //! Default constructor use for vector movement, reallocation, etc
  Vertex() = default;

  //! glm::vec3 constructor for assigning position and normal vectors
  //! used by .emplace_back()
  Vertex(const glm::vec3& position, const glm::vec3& normal) {
    pos = position;
    norm = normal;
  }
};

std::ostream &operator<< (std::ostream &os, const Vertex &v);

//! Struct for defining triangle objects
struct TriElement {
  unsigned int i1, i2, i3;

  //! Default constructor use for vector movement, reallocation, etc
  TriElement() = default;

  //! int constructor for assigning TriElement vertices, used by .emplace_back()
  TriElement(int a, int b, int c) {
    i1 = a, i2 = b, i3 = c;
  }
  
  //! unsingled long constructor for assigning TriElement vertices, used by .emplace_back()
  TriElement(unsigned long a, unsigned long b, unsigned long c) {
    i1 = a, i2 = b, i3 = c;
  }

};

std::ostream &operator<< (std::ostream &os, const TriElement &t);

//! Struct for defining geometric matrix transformations
struct Transform {
  vec3 scale{1.0};
  vec3 translate{0.0, 0.0, 0.0};
  vec3 rotation_axis{0.0, 1.0, 0.0};
  float rotation_angle{0.0};

  //! Default transformation constructor
  Transform () = default;

  //! Constructor passing scaling, translation, and rotational transformation vectors
  [[maybe_unused]] Transform (vec3 scale, vec3 translate, vec3 rotation_axis, float rotation_angle)
    : scale(scale), translate(translate), rotation_axis(glm::normalize(rotation_axis)),
      rotation_angle(rotation_angle) {}

  //! Member function for generating transformation matrix from member vectors
  [[nodiscard]] glm::mat4 get_matrix () const {
    glm::mat4 model{1.0f};
    model = glm::translate(model, translate);
    model = glm::rotate(model, glm::radians(rotation_angle), rotation_axis);
    model = glm::scale(model, scale);
    return model;
  }
};

//! Class definiton for objects mesh
class Mesh {
public:
  size_t num_vertices{0};
  size_t num_triangles{0};

  bool smooth{false};
  bool buffers_set{false};

  vector<Vertex> vertices{};
  vector<TriElement> triangles{};

  //! Default Mesh object constructor
  Mesh () = default;

  //! Mesh object destructor to remove buffers if present
  ~Mesh ();

  //! Read object file to determine Mesh geometry parameters
  //! REQUIRES: Mesh object file and valid file PATH
  //! MODIFIES: vector<Vertex> vertices, vector<TriElement> triangles
  //! EFFECTS: Inputs and constructs triangle and vertex objects int corresponding
  //!          vector utilizing emplace_back(...)
  void read_from_obj (const string &path);

  //! MODIFIES: Mesh buffers and object data
  //! EFFECTS: Set up buffers for mesh objects for simulation
  void set_buffers ();

  //! Draws mesh object using gl
  void draw () const;

  //! Vertex array buffer
  //! Public so we can access this from InstancedArray
  unsigned int vao{}, ebo{};

private:
  //! OpenGL buffers
  unsigned int vbo{};
};

std::ostream &operator<< (std::ostream &os, const Mesh &m);

#endif