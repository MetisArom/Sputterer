// defines to get TOML working with nvcc
#define TOML_RETURN_BOOL_FROM_FOR_EACH_BROKEN 1
#define TOML_RETURN_BOOL_FROM_FOR_EACH_BROKEN_ACKNOWLEDGED 1

#include <toml++/toml.hpp>
#include <iostream>
#include <filesystem>
#include <unordered_map>

#include "Input.hpp"

namespace fs = std::filesystem;

template<typename T>

// Read toml input file table entry 
// REQUIRES: toml table file and input name
// EFFECTS: Returns parsed value
T read_table_entry_as (toml::table &table, const std::string &input_name) {
  auto node = table[input_name];
  bool valid = true;
  T value{};

  if constexpr (std::is_same_v<T, string>) {            // String check
    if (node.is_string()) {
      value = node.as_string()->get();
    } else {
      valid = false;
    }
  } else if constexpr (std::is_same_v<T, glm::vec3>) {  // Nested Table check
    if (node.is_table()) {
      auto tab = node.as_table();
      auto x = read_table_entry_as<float>(*tab, "x");
      auto y = read_table_entry_as<float>(*tab, "y");
      auto z = read_table_entry_as<float>(*tab, "z");
      value = glm::vec3(x, y, z);
    } else {
      valid = false;
    }
  } else {                                              // Literal types check
    if (node.is_integer()) {
      value = static_cast<T>(node.as_integer()->get());
    } else if (node.is_boolean()) {
      value = static_cast<T>(node.as_boolean()->get());
    } else if (node.is_floating_point()) {
      value = static_cast<T>(node.as_floating_point()->get());
    } else if (node.is_string()) {
      string str = node.as_string()->get();
      std::istringstream ss(str);
      ss >> value;
    } else {
      valid = false;
    }
  }
  if (!valid) {
    std::cerr << "Invalid input for option " << input_name << ".\n Expected value of type " << typeid(T).name()
              << "\n.";
  }

  return value;
}

// Retrieve toml input file table according to param name 
// REQUIRES: toml table and input file name
// EFFECTS: Returns toml table
toml::table get_table (toml::table input, const std::string &name) {
  if (input.contains(name)) {
    return *input.get_as<toml::table>(name);
  } else {
    std::ostringstream msg;
    msg << "TOML parsing error:\n"
        << "Key " << name << " not found in table" << std::endl;
    throw std::runtime_error(msg.str());
  }
}

void Input::read () {
  // TODO: need nice error checking

  toml::table input;
  try {
    input = toml::parse_file(filename);
  } catch (const toml::parse_error &err) {
    std::cerr << "Parsing failed:\n" << err << "\n";
  }

  // Read simulation parameters
  auto sim = get_table(input, "simulation");
  timestep_s = read_table_entry_as<float>(sim, "timestep_s");
  max_time_s = read_table_entry_as<float>(sim, "max_time_s");
  output_interval = read_table_entry_as<float>(sim, "output_interval_s");
  particle_weight = read_table_entry_as<double>(sim, "particle_weight");

  // Read chamber features
  auto chamber = get_table(input, "chamber");
  chamber_radius = read_table_entry_as<float>(chamber, "radius_m");
  chamber_length = read_table_entry_as<float>(chamber, "length_m");

  // Read plume parameters
  // TODO: would be good to read these directly into ThrusterPlume structure
  auto plume = get_table(input, "plume_model");
  this->plume_origin = read_table_entry_as<vec3>(plume, "plume_origin");
  this->plume_direction = read_table_entry_as<vec3>(plume, "plume_direction");
  this->background_pressure_torr = read_table_entry_as<double>(plume, "background_pressure_Torr");
  this->ion_current_a = read_table_entry_as<double>(plume, "ion_current_A");
  auto plume_params_arr = plume.get_as<toml::array>("model_parameters");
  auto ind = 0;
  for (auto &&plume_param: *plume_params_arr) {
    this->plume_model_params[ind] = static_cast<double>(plume_param.as_floating_point()->get());;
    ind++;
  }
  this->beam_energy_ev = read_table_entry_as<double>(plume, "beam_energy_eV");
  this->scattered_energy_ev = read_table_entry_as<double>(plume, "scattered_energy_eV");
  this->cex_energy_ev = read_table_entry_as<double>(plume, "cex_energy_eV");

  // Read materials
  std::unordered_map<string, Material> materials;
  std::unordered_map<string, vec3> material_colors;

  auto input_materials = *input.get_as<toml::array>("material");

  for (auto &&material_node: input_materials) {
    auto material_table = material_node.as_table();

    // Populate material
    Material material;

    auto material_name = read_table_entry_as<string>(*material_table, "name");
    auto material_color = read_table_entry_as<vec3>(*material_table, "color");

    material.sticking_coeff = read_table_entry_as<float>(*material_table, "sticking_coeff");
    material.diffuse_coeff = read_table_entry_as<float>(*material_table, "diffuse_coeff");
    material.temperature_k = read_table_entry_as<float>(*material_table, "temperature_K");

    // Add material to list
    materials.insert(std::make_pair(material_name, material));
    material_colors.insert(std::make_pair(material_name, material_color));
  }

  // Read surfaces
  auto geometry = *input.get_as<toml::array>("geometry");
  auto num_surfaces = geometry.size();
  surfaces.resize(num_surfaces);

  int id = 0;
  for (auto &&elem: geometry) {
    auto tab = elem.as_table();
    auto &surf = surfaces.at(id);

    // get material
    auto mat_name = read_table_entry_as<string>(*tab, "material");
    if (materials.find(mat_name) != materials.end()) {
      surf.material = materials.at(mat_name);
      surf.color = material_colors.at(mat_name);
    } else {
      std::cerr << "Material \"" << mat_name << "\" not found in input file!" << std::endl;
    }

    auto &emitter = surf.emitter;
    auto &material = surf.material;

    if (tab->contains("name"))
      surf.name = read_table_entry_as<string>(*tab, "name");

    if (tab->contains("emit"))
      emitter.emit = read_table_entry_as<bool>(*tab, "emit");

    if (tab->contains("collect"))
      material.collect = read_table_entry_as<bool>(*tab, "collect");

    // need to append the current working directory to make sure mesh files are relative to where
    // the input file was run
    auto mesh_file = read_table_entry_as<string>(*tab, "file");
    auto mesh_path = fs::absolute({this->filename}).parent_path()/mesh_file;

    // Read emitter options
    if (emitter.emit && tab->contains("emitter")) {
      auto emit_tab = tab->get_as<toml::table>("emitter");
      emitter.flux = read_table_entry_as<float>(*emit_tab, "flux");
      emitter.velocity = read_table_entry_as<float>(*emit_tab, "velocity");
      if (emit_tab->contains("reverse_direction")) {
        emitter.reverse = read_table_entry_as<bool>(*emit_tab, "reverse_direction");
      }
      if (emit_tab->contains("spread")) {
        emitter.spread = read_table_entry_as<float>(*emit_tab, "spread");
      }
    }

    // object positions (optional)
    auto &transform = surf.transform;

    if (tab->contains("translate")) {
      transform.translate = read_table_entry_as<glm::vec3>(*tab, "translate");
    }

    if (tab->contains("scale")) {
      transform.scale = read_table_entry_as<glm::vec3>(*tab, "scale");
    }

    if (tab->contains("rotate")) {
      auto rot_tab = tab->get_as<toml::table>("rotate");
      if (rot_tab->contains("angle")) {
        transform.rotation_angle = read_table_entry_as<float>(*rot_tab, "angle");
      }

      if (rot_tab->contains("axis")) {
        transform.rotation_axis = read_table_entry_as<vec3>(*rot_tab, "axis");
      }
    }

    // Color (overwrites color specified by material)
    if (tab->contains("color")) {
      surf.color = read_table_entry_as<vec3>(*tab, "color");
    }

    // Temperature (overwrites temperature specified my material)
    if (tab->contains("temperature_K")) {
      surf.material.temperature_k = read_table_entry_as<float>(*tab, "temperature_K");
    }

    // Read mesh data
    auto &mesh = surf.mesh;
    mesh.read_from_obj({mesh_path});

    id++;
  }

  // Read particles (optional)

  if (input.contains("particle")) {
    auto particles = *input.get_as<toml::array>("particle");

    for (auto &&particle: particles) {
      auto particle_tab = particle.as_table();

      auto pos = particle_tab->get_as<toml::table>("position");
      particle_x.push_back(read_table_entry_as<float>(*pos, "x"));
      particle_y.push_back(read_table_entry_as<float>(*pos, "y"));
      particle_z.push_back(read_table_entry_as<float>(*pos, "z"));

      auto vel = particle_tab->get_as<toml::table>("velocity");
      particle_vx.push_back(read_table_entry_as<float>(*vel, "x"));
      particle_vy.push_back(read_table_entry_as<float>(*vel, "y"));
      particle_vz.push_back(read_table_entry_as<float>(*vel, "z"));

      auto weight = read_table_entry_as<float>(*particle_tab, "weight");
      particle_w.push_back(weight);
    }
  }
}
