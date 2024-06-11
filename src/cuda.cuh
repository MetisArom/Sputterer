#pragma once
#ifndef SPUTTERER_CUDA_CUH
#define SPUTTERER_CUDA_CUH

#include "../include/cuda_helpers.cuh"

namespace cuda {
  //! Class Definition for Event type objects
  class Event {
  public:
    Event () {
      CUDA_CHECK(cudaEventCreate(&m_event));
    }

    ~Event () {
      CUDA_CHECK(cudaEventDestroy(m_event));
    };

    void record () const {
      CUDA_CHECK(cudaEventRecord(m_event));
      CUDA_CHECK(cudaEventSynchronize(m_event));
    }

    cudaEvent_t m_event{};
  };

  //! Function for computing time elapsed between events
  //! REQUIRES: Two event references
  //! MODIFIES: float elpased using the cudaEventElapsedTime function
  //! EFFECTS: Returns the computed elapsed time
  float event_elapsed_time (const Event &e1, const Event &e2);

} // namespace cuda

#endif