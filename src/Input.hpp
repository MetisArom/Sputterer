#pragma once
#ifndef SPUTTERER_INPUT_HPP
#define SPUTTERER_INPUT_HPP

#include <string>
#include <vector>
#include <array>

#include "Surface.hpp"
#include "Window.hpp"
#include "vec3.hpp"

using std::string, std::vector;

//! Class definition for input objects
class Input {
public:
  //! Constructors
  Input () = default;

  explicit Input (string filename)
    : filename{std::move(filename)} {}

  string filename{"input.toml"};
  string current_path{"."};

  //! user-specified geometry
  vector<Surface> surfaces;

  //! simulation variables
  float timestep_s{0.0};
  float max_time_s{0.0};
  float output_interval{0.0};

  //! chamber geometry
  float chamber_radius{-1.0};
  float chamber_length{-1.0};

  //! plume model inputs
  vec3 plume_origin{};
  vec3 plume_direction{};
  double background_pressure_torr{};
  double ion_current_a{};
  std::array<double, 7> plume_model_params{};
  double beam_energy_ev{};
  double scattered_energy_ev{};
  double cex_energy_ev{};

  //! particle weight
  double particle_weight{1.0f};

  //! initial particles (if any)
  std::vector<float> particle_w;
  std::vector<float> particle_x;
  std::vector<float> particle_y;
  std::vector<float> particle_z;
  std::vector<float> particle_vx;
  std::vector<float> particle_vy;
  std::vector<float> particle_vz;

  //! Input class member function to categorize needed toml inputs
  //! REQUIRES: Input object parent
  //! MODIFIES: Input caller member variables
  //! EFFECTS: Sets input member variables according to toml input parameters
  void read ();
};

#endif