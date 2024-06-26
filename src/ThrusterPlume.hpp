#pragma once
#ifndef SPUTTERER_THRUSTERPLUME_HPP
#define SPUTTERER_THRUSTERPLUME_HPP

#include <array>

#include "vec3.hpp"

struct CurrentFraction {
  double main;
  double scattered;
  double cex;
};

class ThrusterPlume {
public:
  // location of thruster in space
  vec3 location{0.0};
  // direction along which plume is pointing
  vec3 direction{0.0, 0.0, 1.0};

  // Model parameters
  std::array<double, 7> model_params{
    1.0f,   // ratio of the main beam to the total beam
    0.25f,  // ratio for divergence angle of the main to scattered beam
    0.0f,   // "slope" for linear divergence angle function
    0.0f,   // "intercept" for linear divergence angle function
    0.0f,   // "slope" for linear neutral density function
    0.0f,   // "intercept" for linear neutral density function
    1.0f,   // charge exchange collision cross section (square Angstroms)
  };

  // Design parameters
  double background_pressure{0.0}; // normalized background pressure
  double beam_current{5.0};

  // Beam energy
  double beam_energy_ev{300.0};
  double scattered_energy_ev{250.0};
  double cex_energy_ev{50.0};

  [[nodiscard]] double main_divergence_angle () const;

  [[nodiscard]] double scattered_divergence_angle () const;

  // convert 3D Cartesian coordinates (x, y, z) to thruster-relative polar coordinates (r, alpha)
  [[nodiscard]] vec2 convert_to_thruster_coords (vec3 position) const;

  [[nodiscard]] CurrentFraction current_fractions () const;

  void set_buffers ();

  void draw ();

private:
  unsigned int vbo{}, vao{};

};

struct Species;

double sputtering_yield (double energy, double angle, Species incident, Species target);

#endif // SPUTTERER_THRUSTERPLUME_HPP
