#pragma once
#ifndef SPUTTERER_PARTICLECONTAINER_CUH
#define SPUTTERER_PARTICLECONTAINER_CUH

// STL headers
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// CUDA headers
#include <curand.h>
#include <curand_kernel.h>

// Thrust headers
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>

#include "Surface.hpp"

#include "cuda.cuh"
#include "Triangle.cuh"

using thrust::host_vector, thrust::device_vector;

using std::vector, std::string;

constexpr size_t max_particles = 35'000'000;

struct DeviceParticleContainer {
  float3 *position;
  float3 *velocity;
  float *weight;
  int num_particles;
  curandState *rng;
};

//! Holds information for many particles of a specific species.
//! Species are differentiated by charge state and mass.
class ParticleContainer {

public:
  string name;            // name of particles
  double mass;            // mass in atomic mass units
  int charge;          // charge number
  int num_particles{0}; // number of particles in container

  //! RNG state
  device_vector<curandState> d_rng;

  //! Position in meters
  host_vector<float3> position;
  device_vector<float3> d_position;

  //! Velocity in m/s
  host_vector<float3> velocity;
  device_vector<float3> d_velocity;

  //! Particle weight (computational particles per real particle
  host_vector<float> weight;
  device_vector<float> d_weight;

  device_vector<float> d_tmp;

  //! Particle mesh
  Mesh mesh{};

  void draw ();

  void set_buffers ();

  //! Constructor
  ParticleContainer (string name, size_t num = max_particles, double mass = 0.0, int charge = 0);

  //! push particles to next positions (for now just use forward Euler)
  void evolve (const device_vector<Triangle> &tris
               , const device_vector<Material> &mats, const device_vector<size_t> &ids
               , device_vector<int> &collected
               , const device_vector<HitInfo> &hits, const device_vector<float> &num_emit
               , float input_weight, float dt);

  //! add particles to the container
  void add_particles (const host_vector<float3> &pos, const host_vector<float3> &vel, const host_vector<float> &w);

  //! Emit particles from a given triangle
  void emit (Triangle &triangle, Emitter emitter, float dt);

  //! Returns kernel launch params
  [[nodiscard]] std::pair<dim3, dim3>
  get_kernel_launch_params (size_t num_elems, size_t block_size = 32) const;

  //! Set particles that leave bounds to have negative weights
  void flag_out_of_bounds (float radius, float length);

  //! Remove particles with negative weights
  void remove_flagged_particles ();

  //! Copy particles on GPU to CPU
  void copy_to_cpu ();

private:
  unsigned int buffer{};

  [[nodiscard]] DeviceParticleContainer data ();

};

float rand_uniform (float min = 0.0f, float max = 1.0f);

float rand_normal (float mean = 0.0f, float std = 1.0f);

__host__ __device__ float carbon_diffuse_prob (float cos_incident_angle, float incident_energy_ev);

__device__ float3 sample_diffuse (const Triangle &tri, float3 norm, float thermal_speed, curandState *rng);

std::ostream &operator<< (std::ostream &os, ParticleContainer const &pc);


#endif
