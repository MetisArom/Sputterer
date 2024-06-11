#include <random>

#include <thrust/distance.h>
#include <thrust/partition.h>

#include "ParticleContainer.cuh"
#include "cuda_helpers.cuh"
#include "gl_helpers.hpp"
#include "Constants.hpp"

//! Setup RNG for randomizing particles
__global__ void k_setup_rng (curandState *rng, uint64_t seed) {
  unsigned int tid = threadIdx.x + blockIdx.x*blockDim.x;
  curand_init(seed, tid, 0, &rng[tid]);
}

//! Constructor for ParticleContainer
ParticleContainer::ParticleContainer (string name, size_t num, double mass, int charge)
  : name(std::move(name)), mass(mass), charge(charge) {

  // Allocate memory on GPU
  d_position.resize(num);
  d_velocity.resize(num);
  d_weight.resize(num);
  d_tmp.resize(num);
  d_rng.resize(num);

  // Set up RNG for later use
  size_t block_size = 512;
  k_setup_rng<<<num/block_size, block_size>>>(thrust::raw_pointer_cast(d_rng.data()), time(nullptr));
  std::cout << "GPU RNG state initialized." << std::endl;
}

void
ParticleContainer::add_particles (const host_vector<float3> &pos, const host_vector<float3> &vel
                                  , const host_vector<float> &w) {
  auto n = static_cast<int>(
    std::min(
      std::min(pos.size(), vel.size()), 
      w.size()
    )
  );
  if (n == 0) return;

  position.resize(num_particles + n);
  velocity.resize(num_particles + n);
  weight.resize(num_particles + n);

  // Copy particles to CPU arrays
  for (size_t i = 0; i < n; i++) {
    auto id = num_particles + i;
    position[id] = pos[i];
    velocity[id] = vel[i];
    weight[id] = w[i];
  }

  // Copy particles to GPU
  thrust::copy(position.begin() + num_particles, position.end(), d_position.begin() + num_particles);
  thrust::copy(velocity.begin() + num_particles, velocity.end(), d_velocity.begin() + num_particles);
  thrust::copy(weight.begin() + num_particles, weight.end(), d_weight.begin() + num_particles);

  num_particles += n;
}

void ParticleContainer::copy_to_cpu () {
  position = host_vector<float3>(d_position.begin(), d_position.begin() + num_particles);
  velocity = host_vector<float3>(d_velocity.begin(), d_velocity.begin() + num_particles);
  weight = host_vector<float>(d_weight.begin(), d_weight.begin() + num_particles);
}

void ParticleContainer::set_buffers () {
  // enable buffer
  this->mesh.set_buffers();
  glGenBuffers(1, &this->buffer);
}

void ParticleContainer::draw () {

  // Bind vertex array
  auto vao = this->mesh.vao;
  GL_CHECK(glBindVertexArray(vao));

  // Send over model matrix data
  auto mat_vector_size = static_cast<GLsizei>(this->num_particles*sizeof(vec3));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, this->buffer));
  GL_CHECK(glBufferData(GL_ARRAY_BUFFER, mat_vector_size, &position[0], GL_DYNAMIC_DRAW));

  // Set attribute pointers for translation
  GL_CHECK(glEnableVertexAttribArray(2));
  GL_CHECK(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), nullptr));
  GL_CHECK(glVertexAttribDivisor(2, 1));

  // Bind element array buffer
  GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->mesh.ebo));

  // Draw meshes
  GL_CHECK(glDrawElementsInstanced(GL_TRIANGLES, static_cast<unsigned int>(3*this->mesh.num_triangles), GL_UNSIGNED_INT
                                   , nullptr, num_particles));

  // unbind buffers
  GL_CHECK(glBindVertexArray(0));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
  GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}


__host__ __device__ float carbon_diffuse_prob (float cos_incident_angle, float incident_energy_ev) {
  // fit parameters
  constexpr auto angle_offset = 1.6823f;
  constexpr auto energy_offset = 65.6925f;
  constexpr auto energy_scale = 34.5302f;

  auto fac = (cos_incident_angle - angle_offset)*logf((incident_energy_ev + energy_offset)/energy_scale);
  auto diffuse_coeff = 0.003f + fac*fac;
  return diffuse_coeff;
}

__device__ float3
sample_diffuse (const Triangle &tri, const float3 norm, float thermal_speed, curandState *rng) {
  // sample from a cosine distribution
//  auto c_tan1 = curand_normal(rng);
//  auto c_tan2 = curand_normal(rng);
//  auto c_norm = abs(curand_normal(rng));
  using namespace constants;

  auto samples = curand_normal2(rng);
  auto unif = curand_uniform(rng);

  auto c_norm = sqrt(-0.5*log(unif));
  auto c_tan1 = samples.x;
  auto c_tan2 = samples.y;

  // get tangent vectors
  // TODO: may be worth pre-computing these?
  auto tan1 = normalize(tri.v1 - tri.v0);
  auto tan2 = cross(tan1, norm);

  // Compute new velocity vector
  auto vel_refl = thermal_speed*(c_norm*norm + c_tan1*tan1 + c_tan2*tan2);
  return vel_refl;
}

DeviceParticleContainer ParticleContainer::data () {
  DeviceParticleContainer pc;
  pc.position = thrust::raw_pointer_cast(this->d_position.data());
  pc.velocity = thrust::raw_pointer_cast(this->d_velocity.data());
  pc.weight = thrust::raw_pointer_cast(this->d_weight.data());
  pc.rng = thrust::raw_pointer_cast(this->d_rng.data());
  pc.num_particles = this->num_particles;
  return pc;
}

__global__ void
k_evolve (DeviceParticleContainer pc
          , const Triangle *tris, const size_t num_triangles
          , const Material *materials, const size_t *material_ids
          , int *collected
          , const HitInfo *hits, const float *emit_prob, size_t num_hits
          , float input_weight, float dt) {

  // Thread ID, i.e. what particle we're currently moving
  unsigned int tid = threadIdx.x + blockIdx.x*blockDim.x;

  using namespace constants;

  // Particle mass
  // FIXME: currently hard-coded to carbon, easy to fix by passing in mass as a param
  const double mass = 12.011*m_u;

  // Particle energy
  const double energy_factor = 0.5*mass/q_e;

  // k_B / m_u (for thermal speed calculations)
  const auto thermal_speed_factor = static_cast<float>(sqrt(k_b/mass));

  // Push particles
  if (tid < pc.num_particles) {

    auto pos = pc.position[tid];
    auto vel = pc.velocity[tid];

    // Check for intersections with boundaries
    Ray ray{.origin = pos, .direction = dt*vel};
    auto closest_hit = ray.cast(tris, num_triangles);

    if (closest_hit.t <= 1) {
      auto &[_, t, hit_pos, norm, hit_triangle_id] = closest_hit;

      // Get material info where we hit
      auto &mat = materials[material_ids[hit_triangle_id]];

      // Generate a random number
      auto local_rng = &pc.rng[tid];
      auto uniform = curand_uniform(local_rng);

      // get incident angle and energy
      auto velnorm_2 = dot(vel, vel);
      auto cos_incident_angle = abs(dot(vel, -norm)/sqrt(velnorm_2));
      auto incident_energy_ev = static_cast<float>(energy_factor*velnorm_2);

      // Get sticking and diffuse coeff from model
      auto diffuse_coeff = carbon_diffuse_prob(cos_incident_angle, incident_energy_ev);
      auto sticking_coeff = 1.0f - diffuse_coeff;

      if (uniform < sticking_coeff) {
        // Particle sticks to surface
        pc.position[tid] = hit_pos;
        pc.velocity[tid] = float3{0.0f, 0.0f, 0.0f};

        // Record that we hit this triangle
        atomicAdd(&collected[hit_triangle_id], 1);

        // set weight negative to flag for removal
        // magnitude indicates which triangle we hit
        pc.weight[tid] = static_cast<float>(-hit_triangle_id);

      } else if (uniform < diffuse_coeff + sticking_coeff) {
        // Particle reflects diffusely based on surface temperature
        // TODO: pass thermal speed (or sqrt of temperature) instead of temperature to avoid this
        //
        auto sqrt_temp = sqrtf(mat.temperature_k);
        auto thermal_speed = thermal_speed_factor*sqrt_temp;
        auto vel_refl = sample_diffuse(tris[hit_triangle_id], norm, thermal_speed, local_rng);

        // Get particle position
        // (assuming particle reflects ~instantaneously then travels according to new velocity vector)
        // TODO: most of this code is shared with below--worth unifying?
        auto final_pos = hit_pos + (1 - t)*dt*vel_refl;
        pc.position[tid] = final_pos;
        pc.velocity[tid] = vel_refl;
      } else {
        // Particle reflects specularly
        float3 vel_norm = dot(vel, norm)*norm;
        float3 vel_refl = vel - 2*vel_norm;

        auto final_pos = hit_pos + (1 - t)*dt*vel_refl;
        pc.position[tid] = final_pos;
        pc.velocity[tid] = vel_refl;
      }
    } else {
      pc.position[tid] = pos + dt*vel;
    }
  } else if (tid < num_hits + pc.num_particles) {
    // Emit new particles
    auto &hit = hits[tid - pc.num_particles];
    auto p_emit = emit_prob[tid - pc.num_particles];
    auto local_rng = &pc.rng[tid];

    // compute diffuse velocity
    auto &tri = tris[hit.id];
    auto &mat = materials[material_ids[hit.id]];
    auto thermal_speed = sqrtf(mat.temperature_k)*thermal_speed_factor;
    auto vel = sample_diffuse(tri, hit.norm, thermal_speed, local_rng);

    // generate rng
    auto u = curand_uniform(local_rng);

    // add new particles (negative weight if not real)
    if (u < p_emit*dt) {
      pc.position[tid] = hit.pos + 1e-2*dt*vel;
      pc.velocity[tid] = vel;
      pc.weight[tid] = input_weight;
    } else {
      pc.position[tid] = -1000.000*hit.pos;
      pc.velocity[tid] = {0.0, 0.0, 0.0};
      pc.weight[tid] = -1.0;
    }
  }
}

std::pair<dim3, dim3> ParticleContainer::get_kernel_launch_params (size_t num_elems, size_t block_size) const {
  auto grid_size = static_cast<int>(ceil(static_cast<float>(num_elems)/static_cast<float>(block_size)));
  dim3 grid(grid_size, 1, 1);
  dim3 block(block_size, 1, 1);
  return std::make_pair(grid, block);
}

void ParticleContainer::evolve (const device_vector<Triangle> &tris
                                , const device_vector<Material> &mats, const device_vector<size_t> &ids
                                , device_vector<int> &collected
                                , const device_vector<HitInfo> &hits, const device_vector<float> &num_emit
                                , const float input_weight, const float dt) {


  // TODO: could move all of the device geometric info into a struct
  auto d_tri_ptr = thrust::raw_pointer_cast(tris.data());
  auto d_id_ptr = thrust::raw_pointer_cast(ids.data());
  auto d_mat_ptr = thrust::raw_pointer_cast(mats.data());

  auto d_col_ptr = thrust::raw_pointer_cast(collected.data());
  auto d_hit_ptr = thrust::raw_pointer_cast(hits.data());
  auto d_emit_ptr = thrust::raw_pointer_cast(num_emit.data());

  auto [grid, block] = get_kernel_launch_params(num_particles + hits.size());

  k_evolve<<<grid, block>>>(
    this->data()
    , d_tri_ptr, tris.size()
    , d_mat_ptr, d_id_ptr, d_col_ptr
    , d_hit_ptr, d_emit_ptr, hits.size()
    , input_weight, dt);

  this->num_particles += hits.size();

  cudaDeviceSynchronize();
}

float rand_uniform (float min, float max) {
  static std::default_random_engine rng;

  std::uniform_real_distribution<float> dist(min, max);
  return dist(rng);
}

float rand_normal (float mean, float std) {
  static std::default_random_engine rng;

  std::normal_distribution<float> dist(mean, std);
  return dist(rng);
}

void ParticleContainer::emit (Triangle &triangle, Emitter emitter, float dt) {
  auto num_emit = emitter.flux*triangle.area*dt;
  int num_emit_int = static_cast<int>(num_emit);
  auto remainder = num_emit - static_cast<float>(num_emit_int);

  auto u = rand_uniform();
  if (u < remainder) {
    num_emit_int += 1;
  }

  if (num_emit_int < 1) {
    return;
  }

  host_vector<float3> pos(num_emit_int);
  host_vector<float3> vel(num_emit_int);
  host_vector<float> w(num_emit_int, 1.0f);

  for (int i = 0; i < num_emit_int; i++) {
    auto pt = triangle.sample(rand_uniform(), rand_uniform());
    auto norm = emitter.reverse ? -triangle.norm : triangle.norm;
    // offset particle very slightly by norm
    auto tol = 0.0001f;
    pos[i] = pt + tol*norm;
    auto jitter = float3{
      rand_normal(0, emitter.spread), rand_normal(0, emitter.spread), rand_normal(0, emitter.spread)};
    vel[i] = emitter.velocity*(norm + jitter);
  }

  add_particles(pos, vel, w);
}

__global__ void k_flag_oob (float3 *pos, float *weight, float radius2, float halflength, size_t n) {
  unsigned int id = threadIdx.x + blockIdx.x*blockDim.x;
  if (id < n && weight[id] > 0) {
    auto r = pos[id];
    auto dist2 = r.x*r.x + r.y*r.y;
    if (dist2 > radius2 || r.z < -halflength || r.z > halflength) {
      // Particles that are oob get negative weight
      weight[id] = -1;
    }
  }
}

void ParticleContainer::flag_out_of_bounds (float radius, float length) {
  auto [grid, block] = get_kernel_launch_params(num_particles);

  auto d_pos_ptr = thrust::raw_pointer_cast(d_position.data());
  auto d_wgt_ptr = thrust::raw_pointer_cast(d_weight.data());
  k_flag_oob<<<grid, block>>>(d_pos_ptr, d_wgt_ptr, radius*radius, length/2, num_particles);
  cudaDeviceSynchronize();
}

struct IsPositive {
  __host__ __device__ bool operator() (const float &w) {
    return w > 0;
  }
};

void ParticleContainer::remove_flagged_particles () {
  // reorder positions and velocities so that particles with negative weight follow those with positive weight
  thrust::partition(d_position.begin(), d_position.begin() + num_particles, d_weight.begin(), IsPositive());
  thrust::partition(d_velocity.begin(), d_velocity.begin() + num_particles, d_weight.begin(), IsPositive());

  // reorder weights according to the same scheme as above
  // copy weights to temporary vector first
  // thrust partition likely is allocating some temporary memory
  // to avoid this, we would probably want to set up a custom allocator
  // c.f. https://github.com/NVIDIA/thrust/blob/1.6.0/examples/cuda/custom_temporary_allocation.cu
  // Alternatively, could use CUB device partition, which gives us more control to allocate temporary data
  // c.f. https://nvidia.github.io/cccl/cub/api/structcub_1_1DevicePartition.html#_CPPv4N3cub15DevicePartitionE
  thrust::copy(d_weight.begin(), d_weight.begin() + num_particles, d_tmp.begin());
  auto ret = thrust::partition(d_weight.begin(), d_weight.begin() + num_particles, d_tmp.begin(), IsPositive());

  // Reset number of particles to the middle of the partition
  num_particles = static_cast<int>(thrust::distance(d_weight.begin(), ret));
}


std::ostream &operator<< (std::ostream &os, ParticleContainer const &pc) {
  os << "==========================================================\n";
  os << "Particle container \"" << pc.name << "\"\n";
  os << "==========================================================\n";
  os << "Mass: " << pc.mass << "\n";
  os << "Charge: " << pc.charge << "\n";
  os << "Number of particles: " << pc.num_particles << "\n";
  os << "----------------------------------------------------------\n";
  os << "\tx\ty\tz\tvx\tvy\tvz\tw\t\n";
  os << "----------------------------------------------------------\n";
  for (int i = 0; i < pc.num_particles; i++) {
    os << "\t" << pc.position[i].x << " ";
    os << pc.position[i].y << "  ";
    os << pc.position[i].z << "  ";
    os << pc.velocity[i].x << "  ";
    os << pc.velocity[i].y << "  ";
    os << pc.velocity[i].z << "  ";
    os << pc.weight[i] << "\n";
  }
  os << "==========================================================\n";

  return os;
}