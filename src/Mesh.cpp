#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>

#include "Mesh.hpp"
#include "gl_helpers.hpp"

// Output stream overloads for Mesh.hpp structs
std::ostream &operator<< (std::ostream &os, const Vertex &v) {
  os << "{ pos: " << v.pos << ", norm: " << v.norm << "}";
  return os;
}

std::ostream &operator<< (std::ostream &os, const TriElement &e) {
  os << "[" << e.i1 << ", " << e.i2 << ", " << e.i3 << "]";
  return os;
}

std::ostream &operator<< (std::ostream &os, const Mesh &m) {
  os << "Vertices\n=======================\n";
  for (size_t i = 0; i < m.num_vertices; i++) {
    os << i << ": " << m.vertices[i] << "\n";
  }

  os << "Elements\n=======================\n";
  for (size_t i = 0; i < m.num_triangles; i++) {
    std::cout << i << ": " << m.triangles.at(i) << "\n";
  }

  return os;
}

// Helper function for processing obj file delimiters
vector<string> split (const string &s, char delim);

void Mesh::read_from_obj (const string &path) {

  if (!(std::filesystem::exists(path))) {
    std::ostringstream msg;
    msg << "File " << path << " not found!\n";
    throw std::runtime_error(msg.str());
  }

  vector<glm::vec3> vertexCoords;
  vector<TriElement> triangleInds;

  // Read basic vertex information from file
  std::ifstream objFile(path);
  while (!objFile.eof()) {
    // Read a line from the file;
    string line;
    std::getline(objFile, line);
    std::istringstream lineStream(line);

    // Read first character of line;
    auto firstChar = lineStream.get();

    // Determine what to do based on what the first character is.
    switch (firstChar) {
      case ('v'): {
        // This line pertains to vertex data
        // We only care about vertex coords, so we want to read only if
        // the specifier is 'v' (as opposed to 'vt' or 'vn'), which describe
        // vertex texture coords and normal vectors, respectively.
        auto nextChar = lineStream.peek();
        if (!isspace(nextChar)) {
          continue;
        }

        // Read vertex coordinates from file
        float x{}, y{}, z{};
        lineStream >> x >> y >> z;

        // Place vertex coordinates in array.
        vertexCoords.emplace_back(x, y, z);

        break;
      }
      case ('f'): {
        // This line pertains to face data
        // Read face indices from file
        string i1_str, i2_str, i3_str;
        lineStream >> i1_str >> i2_str >> i3_str;

        // Discard information related to vertex/texture coords
        auto i1 = std::stoi(split(i1_str, '/').at(0));
        auto i2 = std::stoi(split(i2_str, '/').at(0));
        auto i3 = std::stoi(split(i3_str, '/').at(0));

        // Need to subtract one from each index, as obj files count from 1
        triangleInds.emplace_back(i1 - 1, i2 - 1, i3 - 1);
        break;
      }
      case ('s'): {
        // This line enables/disables smooth shading
        lineStream >> smooth;
        break;
      }
      default: {
        break;
      }
    }
  }

  // Next, if smooth shading is not enabled, we need to split vertices for each face
  // In both cases, we need to generate normals.
  if (smooth) {
    num_triangles = triangleInds.size();
    num_vertices = vertexCoords.size();
    triangles = triangleInds;
    vertices = vector<Vertex>(num_vertices);

    for (const auto &[i1, i2, i3]: triangles) {
      // Get vertex coordinates
      auto a = vertexCoords.at(i1);
      auto b = vertexCoords.at(i2);
      auto c = vertexCoords.at(i3);

      // Compute face normal and add to all three vertex normals
      vec3 faceNorm = glm::normalize(glm::cross((b - a), (c - a)));
      vertices.at(i1).norm += faceNorm;
      vertices.at(i2).norm += faceNorm;
      vertices.at(i3).norm += faceNorm;
    }

    // Assign vertex positions and normalize normal vectors
    for (size_t i = 0; i < num_vertices; i++) {
      vertices.at(i).pos = vertexCoords.at(i);
      vertices.at(i).norm = glm::normalize(vertices.at(i).norm);
    }
  } else {
    // Split vertices at so each edge has its own copy of each component vertex
    // This enables sharp (non-smooth) shading
    for (const auto &[i1, i2, i3]: triangleInds) {
      // Get vertex coordinates
      auto a = vertexCoords.at(i1);
      auto b = vertexCoords.at(i2);
      auto c = vertexCoords.at(i3);

      // Compute normal
      auto n = glm::normalize(glm::cross((b - a), (c - a)));

      // Add vertices to array
      vertices.emplace_back(a, n);
      vertices.emplace_back(b, n);
      vertices.emplace_back(c, n);

      // Add triangles
      triangles.emplace_back(num_vertices, num_vertices + 1, num_vertices + 2);

      num_vertices += 3;
      num_triangles += 1;
    }
  }
}

void Mesh::set_buffers () {
  auto vertSize = num_vertices*sizeof(Vertex);
  auto triSize = num_triangles*sizeof(TriElement);

  // Set up buffers
  GL_CHECK(glGenVertexArrays(1, &vao));
  GL_CHECK(glGenBuffers(1, &vbo));
  GL_CHECK(glGenBuffers(1, &ebo));

  // Assign vertex data
  GL_CHECK(glBindVertexArray(vao));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  GL_CHECK(glBufferData(GL_ARRAY_BUFFER, vertSize, vertices.data(), GL_STATIC_DRAW));

  // Assign element data
  GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));
  GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, triSize, triangles.data(), GL_STATIC_DRAW));

  // Vertex positions
  GL_CHECK(glEnableVertexAttribArray(0));
  GL_CHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), nullptr));

  // Vertex normals
  GL_CHECK(glEnableVertexAttribArray(1));
  GL_CHECK(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void *) (3*sizeof(float))));

  // Unbind arrays and buffers
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  buffers_set = true;
}

Mesh::~Mesh () {
  if (buffers_set) {
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
  }
}

void Mesh::draw () const {

  // draw mesh
  // std::cout << "VAO, VBO, EBO: " << VAO << ", " << VBO << ", " << EBO << "\n";
  GL_CHECK(glBindVertexArray(this->vao));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));

  GL_CHECK(glDrawElements(GL_TRIANGLES, 3*num_triangles, GL_UNSIGNED_INT, nullptr));

  GL_CHECK(glBindVertexArray(0));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
  GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

vector<string> split (const string &s, char delim) {
  vector<string> result;
  std::stringstream ss(s);
  string item;

  while (getline(ss, item, delim)) {
    result.push_back(item);
  }
  return result;
}
