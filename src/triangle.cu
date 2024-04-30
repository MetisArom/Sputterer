#include <iostream>
#include "triangle.cuh"

std::ostream &operator<< (std::ostream &os, const float3 &v) {
  os << "[" << v.x << ", " << v.y << ", " << v.z << "]";
  return os;
}