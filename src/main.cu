// C++ headers
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>

// GLM headers
#include <glm/glm.hpp>

// ImGUI headers
#include "imgui.h"

// My headers (c++)
#include "app.hpp"
#include "input.hpp"
#include "shader.hpp"
#include "surface.hpp"
#include "window.hpp"

// My headers (CUDA)
#include "cuda.cuh"
#include "particle_container.cuh"
#include "triangle.cuh"

using std::vector, std::string;

string print_time (double time_s) {
  char buf[64];
  int factor = 1;
  string str = "s";

  if (time_s < 1e-6) {
    factor = 1'000'000'000;
    str = "ns";
  } else if (time_s < 1e-3) {
    factor = 1'000'000;
    str = "us";
  } else if (time_s < 1) {
    factor = 1000;
    str = "ms";
  }

  sprintf(buf, "%.3f %s", time_s*factor, str.c_str());

  return {buf};
}

int main (int argc, char *argv[]) {
  // Handle command line arguments
  string filename{"../input.toml"};
  bool display{true};

  if (argc > 1) {
    filename = argv[1];
  }
  if (argc > 2) {
    display = static_cast<bool>(std::stoi(argv[2]));
  }

  input input(filename);
  input.read();

  std::cout << "Input read." << std::endl;

  app::camera.orientation = glm::normalize(glm::vec3(input.chamber_radius));
  app::camera.distance = 2.0f*input.chamber_radius;
  app::camera.yaw = -135;
  app::camera.pitch = 30;
  app::camera.update_vectors();

  // Create particle container, including any explicitly-specified initial particles
  particle_container pc{"noname", 1.0f, 1};
  pc.add_particles(input.particle_x, input.particle_y, input.particle_z, input.particle_vx, input.particle_vy
                   , input.particle_vz, input.particle_w);

  // construct triangles
  host_vector<triangle> h_triangles;

  host_vector<size_t> h_material_ids;
  host_vector<material> h_materials;

  host_vector<char> h_to_collect;
  std::vector<int> collect_inds_global;
  std::vector<int> collect_inds_local;

  std::vector<string> surface_names;

  for (size_t id = 0; id < input.surfaces.size(); id++) {
    const auto &surf = input.surfaces.at(id);
    const auto &mesh = surf.mesh;
    const auto &material = surf.material;

    surface_names.push_back(surf.name);
    h_materials.push_back(surf.material);

    auto ind = 0;
    for (const auto &[i1, i2, i3]: mesh.triangles) {
      auto model = surf.transform.get_matrix();
      auto v1 = make_float3(model*glm::vec4(mesh.vertices[i1].pos, 1.0));
      auto v2 = make_float3(model*glm::vec4(mesh.vertices[i2].pos, 1.0));
      auto v3 = make_float3(model*glm::vec4(mesh.vertices[i3].pos, 1.0));

      h_triangles.push_back({v1, v2, v3});
      h_material_ids.push_back(id);
      if (material.collect) {
        collect_inds_global.push_back(static_cast<int>(h_triangles.size()) - 1);
        collect_inds_local.push_back(ind);
      }
      ind++;
    }
  }

  host_vector<int> collected(collect_inds_global.size(), 0);
  std::cout << "Meshes read." << std::endl;

  // Send mesh data to GPU. Really slow for some reason (multiple seconds)!
  device_vector<triangle> d_triangles = h_triangles;
  device_vector<size_t> d_surface_ids{h_material_ids};
  device_vector<material> d_materials{h_materials};
  device_vector<int> d_collected(h_triangles.size(), 0);

  // Display objects
  window window{.name = "Sputterer", .width = app::screen_width, .height = app::screen_height};
  shader mesh_shader{}, particle_shader{};
  if (display) {
    // enable window
    window.enable();

    // Register window callbacks
    glfwSetFramebufferSizeCallback(window.glfw_window, app::framebuffer_size_callback);
    glfwSetCursorPosCallback(window.glfw_window, app::mouse_cursor_callback);
    glfwSetScrollCallback(window.glfw_window, app::scroll_callback);

    // Load mesh shader
    mesh_shader.load("../shaders/shader.vert", "../shaders/shader.frag");

    // initialize mesh buffers
    for (auto &surf: input.surfaces) {
      surf.mesh.setBuffers();
    }

    // Load particle shader
    particle_shader.load("../shaders/particle.vert", "../shaders/particle.frag");
    particle_shader.use();
    constexpr vec3 particle_color{0.05f};
    constexpr vec3 particle_scale{0.01f};
    particle_shader.set_vec3("scale", particle_scale);
    particle_shader.set_vec3("objectColor", particle_color);

    // Set up particle mesh
    pc.mesh.read_from_obj("../o_sphere.obj");
    pc.set_buffers();
  }

  // Create timing objects
  size_t frame = 0;

  float avg_time_compute = 0.0f, avg_time_total = 0.0f;
  float iter_reset = 25;
  float time_const = 1/iter_reset;
  double physical_time = 0, physical_timestep = 0;
  float delta_time_smoothed = 0;

  auto next_output_time = 0.0f;

  cuda::event start{}, stop_compute{}, stop_copy{};

  std::cout << "Beginning main loop." << std::endl;

  auto current_time = std::chrono::system_clock::now();
  auto last_time = std::chrono::system_clock::now();

  // Create output file for deposition
  string output_filename{"deposition.csv"};
  std::ofstream output_file;
  output_file.open(output_filename);
  output_file << "Time(s),Surface name,Local triangle ID,Global triangle ID,Particles collected" << std::endl;
  output_file.close();

  while ((display && window.open) || (!display && physical_time < input.max_time)) {

    if (display) {
      window::begin_render_loop();

      // Timing info
      auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings;
      float padding = 0.0f;
      ImVec2 bottom_right =
              ImVec2(ImGui::GetIO().DisplaySize.x - padding, ImGui::GetIO().DisplaySize.y - padding);
      ImGui::SetNextWindowPos(bottom_right, ImGuiCond_Always, ImVec2(1.0, 1.0));
      ImGui::Begin("Frame time", nullptr, flags);
      ImGui::Text("Simulation step %li (%s)\nSimulation time: %s\nCompute time: %.3f ms (%.2f%% data "
                  "transfer)   \nFrame time: %.3f ms (%.1f fps, %.2f%% compute)   \nParticles: %i", frame
                  , print_time(physical_timestep).c_str(), print_time(physical_time).c_str(), avg_time_compute,
              (1.0f - avg_time_compute/avg_time_total)*100, delta_time_smoothed, 1000/delta_time_smoothed,
              (avg_time_total/delta_time_smoothed)*100, pc.num_particles);
      ImGui::End();

      // Table of collected particle amounts
      auto table_flags = ImGuiTableFlags_BordersH;
      ImVec2 bottom_left = ImVec2(0, ImGui::GetIO().DisplaySize.y - padding);
      ImGui::SetNextWindowPos(bottom_left, ImGuiCond_Always, ImVec2(0.0, 1.0));
      ImGui::Begin("Particle collection info", nullptr, flags);
      if (ImGui::BeginTable("Table", 4, table_flags)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Surface name");
        ImGui::TableNextColumn();
        ImGui::Text("Triangle ID");
        ImGui::TableNextColumn();
        ImGui::Text("Particles collected");
        ImGui::TableNextColumn();
        ImGui::Text("Collection rate (#/s)");
        for (int row = 0; row < collect_inds_global.size(); row++) {
          auto triangle_id = collect_inds_global[row];
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%s", surface_names.at(h_material_ids[triangle_id]).c_str());
          ImGui::TableNextColumn();
          ImGui::Text("%i", static_cast<int>(collect_inds_local[row]));
          ImGui::TableNextColumn();
          ImGui::Text("%d", collected[row]);
          ImGui::TableNextColumn();
          ImGui::Text("%.3e", static_cast<double>(collected[row])/physical_time);
        }
        ImGui::EndTable();
      }
      ImGui::End();
    }

    // Record iteration timing information
    current_time = std::chrono::system_clock::now();
    app::delta_time = static_cast<float>(
                              std::chrono::duration_cast<std::chrono::microseconds>(
                                      current_time - last_time).count())/
                      1e6;
    last_time = current_time;

    // set physical timestep. if we're displaying a window, we set the physical timestep based on the rendering
    // timestep to get smooth performance at different window sizes. If not, we just use the user-provided timestep
    float this_timestep{0};
    if (display) {
      this_timestep = static_cast<float>(input.timestep*app::delta_time/(15e-3));
    } else {
      this_timestep = input.timestep;
    }
    physical_time += this_timestep;
    physical_timestep = (1 - time_const)*physical_timestep + time_const*this_timestep;
    delta_time_smoothed = (1 - time_const)*delta_time_smoothed + time_const*app::delta_time*1000;

    // Main computations
    if (frame > 0) {
      start.record();

      // Emit particles
      size_t tri_count{0};
      for (const auto &surf: input.surfaces) {
        auto &emitter = surf.emitter;
        if (!emitter.emit) {
          continue;
        }

        for (size_t i = 0; i < surf.mesh.num_triangles; i++) {
          pc.emit(h_triangles[i], emitter, this_timestep);
        }
        tri_count += surf.mesh.num_triangles;
      }

      // Push particles
      pc.push(this_timestep, d_triangles, d_surface_ids, d_materials, d_collected);

      // Remove particles that are out of bounds
      pc.flag_out_of_bounds(input.chamber_radius, input.chamber_length);
      pc.remove_flagged_particles();
      stop_compute.record();

      // Track particles collected by each triangle flagged 'collect'
      for (int id = 0; id < collect_inds_global.size(); id++) {
        auto d_begin = d_collected.begin() + collect_inds_global[id];
        thrust::copy(d_begin, d_begin + 1, collected.begin() + id);
      }

      // Copy particle data back to CPU
      pc.copy_to_cpu();

      stop_copy.record();

      float elapsed_compute, elapsed_copy;
      elapsed_compute = cuda::event_elapsed_time(start, stop_compute);
      elapsed_copy = cuda::event_elapsed_time(start, stop_copy);

      avg_time_compute = (1 - time_const)*avg_time_compute + time_const*elapsed_compute;
      avg_time_total = (1 - time_const)*avg_time_total + time_const*elapsed_copy;
    }

    // Rendering
    if (display) {
      // update camera projection mesh shader
      mesh_shader.use();
      mesh_shader.update_view(app::camera, app::aspect_ratio);

      // draw meshes
      for (const auto &surface: input.surfaces) {
        // set the model matrix
        mesh_shader.use();
        surface.mesh.draw(mesh_shader, surface.transform, surface.color);
      }

      // draw particles (instanced!)
      if (pc.num_particles > 0) {
        // activate particle shader
        particle_shader.use();

        // send camera information to shader
        auto cam = app::camera.get_projection_matrix(app::aspect_ratio)*app::camera.get_view_matrix();
        particle_shader.set_mat4("camera", cam);

        // draw particles
        pc.draw(particle_shader);
      }
    }

    if (physical_time > next_output_time || (!display && physical_time >= input.max_time) ||
        (display && !window.open)) {
      // Write output to console at regular intervals, plus one additional when simulation terminates
      std::cout << "Step " << frame << ", Simulation time: " << print_time(physical_time)
                << ", Timestep: " << print_time(physical_timestep) << ", Avg. step time: " << delta_time_smoothed
                << " ms" << std::endl;

      // Log deposition rate info
      output_file.open(output_filename, std::ios_base::app);
      for (int i = 0; i < collect_inds_global.size(); i++) {
        auto triangle_id_global = collect_inds_global[i];
        output_file << physical_time << ",";
        output_file << surface_names.at(h_material_ids[triangle_id_global]) << ",";
        output_file << collect_inds_local.at(i) << ",";
        output_file << triangle_id_global << ",";
        output_file << collected[i] << "\n";
      }
      output_file.close();

      next_output_time += input.output_interval;
    }

    if (display) {
      window.end_render_loop();
      app::process_input(window.glfw_window);
    }

    frame += 1;
  }

  std::cout << "Program terminated successfully." << std::endl;

  return 0;
}
