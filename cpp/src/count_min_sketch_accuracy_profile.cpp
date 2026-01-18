/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <iostream>
#include <algorithm>
#include <random>
#include <chrono>
#include <unordered_map>
#include <cmath>

#include <count_min.hpp>

#include "count_min_sketch_accuracy_profile.hpp"
#include "zipf_distribution.hpp"

namespace datasketches {

void count_min_sketch_accuracy_profile::run() {
  // Stream length parameters
  const unsigned lg_min_stream_len = 0;
  const unsigned lg_max_stream_len = 23;
  const unsigned ppo = 16;

  // Trial parameters
  const unsigned lg_max_trials = 1;
  const unsigned lg_min_trials = 1;

  // Sketch parameters - following CLAUDE.md guidelines
  // Medium size: width=1024, depth=5
  const uint32_t num_buckets = 1024;
  const uint8_t num_hashes = 5;

  // Data distribution parameters
  const unsigned zipf_lg_range = 13;  // 8K distinct values
  const double zipf_exponent = 1.1;   // Zipfian skew

  // Fixed seed for reproducibility
  const uint64_t seed = 42;

  zipf_distribution zipf(1 << zipf_lg_range, zipf_exponent);

  // Output header
  std::cout << "StreamLen\tTrials\tNumBuckets\tNumHashes\t"
            << "AvgRelError\tMaxRelError\tTheoreticalEpsilon\t"
            << "ErrorBoundViolations\tAvgOverestimate\tMaxOverestimate"
            << std::endl;

  size_t stream_length = 1 << lg_min_stream_len;
  while (stream_length <= (1ULL << lg_max_stream_len)) {
    const size_t num_trials = get_num_trials(
        stream_length, lg_min_stream_len, lg_max_stream_len,
        lg_min_trials, lg_max_trials);

    double total_rel_error = 0.0;
    double max_rel_error = 0.0;
    uint64_t error_bound_violations = 0;
    double total_overestimate = 0.0;
    double max_overestimate = 0.0;
    uint64_t total_queries = 0;

    uint64_t* values = new uint64_t[stream_length];

    for (size_t trial = 0; trial < num_trials; trial++) {
      // Generate stream data
      for (size_t j = 0; j < stream_length; j++) {
        values[j] = zipf.sample();
      }

      // Build ground truth frequency map
      std::unordered_map<uint64_t, uint64_t> true_frequencies;
      for (size_t j = 0; j < stream_length; j++) {
        true_frequencies[values[j]]++;
      }

      // Create and populate sketch
      count_min_sketch<uint64_t> sketch(num_hashes, num_buckets, seed);
      for (size_t j = 0; j < stream_length; j++) {
        sketch.update(values[j]);
      }

      const double epsilon = sketch.get_relative_error();
      const uint64_t total_weight = sketch.get_total_weight();

      // Query all distinct items and compute error metrics
      for (const auto& kv : true_frequencies) {
        uint64_t item = kv.first;
        uint64_t true_count = kv.second;
        uint64_t estimate = sketch.get_estimate(item);

        // Count-Min always overestimates
        uint64_t overestimate = estimate - true_count;
        double rel_error = static_cast<double>(overestimate) / total_weight;

        total_rel_error += rel_error;
        max_rel_error = std::max(max_rel_error, rel_error);

        total_overestimate += overestimate;
        max_overestimate = std::max(max_overestimate, static_cast<double>(overestimate));

        // Check if error bound is violated
        // Theoretical guarantee: estimate <= true_count + epsilon * total_weight
        if (overestimate > epsilon * total_weight) {
          error_bound_violations++;
        }

        total_queries++;
      }
    }

    delete[] values;

    // Compute averages
    double avg_rel_error = total_rel_error / total_queries;
    double avg_overestimate = total_overestimate / total_queries;
    double theoretical_epsilon = std::exp(1.0) / num_buckets;

    std::cout << stream_length << "\t"
              << num_trials << "\t"
              << num_buckets << "\t"
              << static_cast<unsigned>(num_hashes) << "\t"
              << avg_rel_error << "\t"
              << max_rel_error << "\t"
              << theoretical_epsilon << "\t"
              << error_bound_violations << "\t"
              << avg_overestimate << "\t"
              << max_overestimate
              << std::endl;

    stream_length = pwr_2_law_next(ppo, stream_length);
  }
}

} /* namespace datasketches */