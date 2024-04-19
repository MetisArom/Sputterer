#ifndef _SURFACE_H
#define _SURFACE_H

#include <vector>
#include <string>
#include "Vec3.hpp"
#include "Shader.hpp"
using std::vector, std::string;

class Vertex{
    public:
        Point3<float> pos;
        Vec3<float> normal;
};

class Surface {
    public:
        int numVertices;
        int numElements;
        vector<Vertex> vertices;
        vector<Vec3<unsigned int>> elements;
        bool enable_smooth{false};
        bool enabled;

        string name{"noname"};
        bool emit{false};
        bool collect{false};

        glm::vec3 scale;
        glm::vec3 translate;
        glm::vec3 color;

        Surface() = default;
        Surface(string name, string path, bool emit, bool collect, glm::vec3 scale, glm::vec3 translate, glm::vec3 color);
        ~Surface();

        void draw(Shader &shader) const;
        void enable();
        void disable();
    private:
        unsigned int VAO, VBO, EBO;
};

std::ostream &operator<<(std::ostream &os, Surface const &m);

#endif