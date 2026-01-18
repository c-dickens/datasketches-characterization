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
#include <unordered_map>
#include <cmath>
#include <vector>
#include <iomanip>

#include <count_min.hpp>

#include "count_min_sketch_failure_probability_profile.hpp"
#include "zipf_distribution.hpp"

namespace datasketches {

void count_min_sketch_failure_probability_profile::run() {
  // Sketch depth parameters to compare: 3, 5, 7 hash functions
  const std::vector<uint8_t> depths = {3, 5, 7};

  // Fixed width for failure probability comparison
  const uint32_t num_buckets = 1024;  // lg_width = 10

  // Trial parameters - need many trials for reliable failure rate estimation
  const size_t num_trials = 1 << 12;  // 4096 trials

  // Stream length
  const size_t stream_length = 1 << 17;  // 131072 items per trial

  // Data distribution parameters
  const unsigned zipf_lg_range = 13;  // 8K distinct values
  const double zipf_exponent = 1.1;   // Zipfian skew

  // Metadata to stderr
  std::cerr << "# Count-Min Sketch Failure Probability Profile" << std::endl;
  std::cerr << "# Comparing depths (hash functions): ";
  for (uint8_t d : depths) {
    std::cerr << static_cast<unsigned>(d) << " ";
  }
  std::cerr << std::endl;
  std::cerr << "# Parameters: width=" << num_buckets
            << ", stream_length=" << stream_length
            << ", num_trials=" << num_trials << std::endl;
  std::cerr << "# Distribution: Zipf(range=2^" << zipf_lg_range
            << ", exponent=" << zipf_exponent << ")" << std::endl;
  std::cerr << "# Theoretical delta = e^(-depth)" << std::endl;
  std::cerr << "# " << std::endl;

  // Print theoretical bounds
  std::cerr << "# Theoretical failure probabilities:" << std::endl;
  for (uint8_t d : depths) {
    double theoretical_delta = std::exp(-static_cast<double>(d));
    std::cerr << "#   depth=" << static_cast<unsigned>(d)
              << ": delta = e^(-" << static_cast<unsigned>(d) << ") = "
              << std::fixed << std::setprecision(6) << theoretical_delta
              << " (" << (theoretical_delta * 100) << "%)" << std::endl;
  }
  std::cerr << "# " << std::endl;

  // TSV header for detailed per-trial output
  std::cout << "Trial\tDepth\tWidth\tEpsilon\tTheoreticalDelta\t"
            << "NumQueries\tViolations\tViolationRate\t"
            << "TotalWeight\tNumDistinct" << std::endl;

  // Aggregate statistics per depth
  std::vector<size_t> total_violations(depths.size(), 0);
  std::vector<size_t> total_queries(depths.size(), 0);

  for (size_t trial = 0; trial < num_trials; trial++) {
    // Each trial gets a unique seed
    const uint64_t trial_seed = 42 + trial * 1000;

    // Create zipf distribution
    zipf_distribution zipf(1 << zipf_lg_range, zipf_exponent);

    // Generate stream data - SAME stream for all depth configurations
    std::vector<uint64_t> values(stream_length);
    for (size_t j = 0; j < stream_length; j++) {
      values[j] = zipf.sample();
    }

    // Build ground truth frequency map
    std::unordered_map<uint64_t, uint64_t> true_frequencies;
    for (size_t j = 0; j < stream_length; j++) {
      true_frequencies[values[j]]++;
    }

    const size_t num_distinct = true_frequencies.size();

    // Test each depth configuration with the SAME stream
    for (size_t d_idx = 0; d_idx < depths.size(); d_idx++) {
      uint8_t depth = depths[d_idx];

      // Create and populate sketch
      count_min_sketch<uint64_t> sketch(depth, num_buckets, trial_seed);
      for (size_t j = 0; j < stream_length; j++) {
        sketch.update(values[j]);
      }

      const double epsilon = sketch.get_relative_error();
      const uint64_t total_weight = sketch.get_total_weight();
      const double error_bound = epsilon * total_weight;
      const double theoretical_delta = std::exp(-static_cast<double>(depth));

      // Count violations for this trial
      size_t trial_violations = 0;
      size_t trial_queries = 0;

      for (const auto& kv : true_frequencies) {
        uint64_t item = kv.first;
        uint64_t true_freq = kv.second;
        uint64_t estimate = sketch.get_estimate(item);

        // Check if bound is violated: estimate > true_freq + epsilon * N
        if (estimate > true_freq + error_bound) {
          trial_violations++;
          total_violations[d_idx]++;
        }
        trial_queries++;
        total_queries[d_idx]++;
      }

      double violation_rate = static_cast<double>(trial_violations) / trial_queries;

      std::cout << trial << "\t"
                << static_cast<unsigned>(depth) << "\t"
                << num_buckets << "\t"
                << epsilon << "\t"
                << theoretical_delta << "\t"
                << trial_queries << "\t"
                << trial_violations << "\t"
                << violation_rate << "\t"
                << total_weight << "\t"
                << num_distinct
                << std::endl;
    }

    // Progress indicator to stderr
    if ((trial + 1) % 500 == 0) {
      std::cerr << "# Completed trial " << (trial + 1) << "/" << num_trials << std::endl;
    }
  }

  // Summary statistics to stderr
  std::cerr << "# " << std::endl;
  std::cerr << "# ========================================" << std::endl;
  std::cerr << "# SUMMARY: Observed vs Theoretical Failure Rates" << std::endl;
  std::cerr << "# ========================================" << std::endl;
  for (size_t d_idx = 0; d_idx < depths.size(); d_idx++) {
    uint8_t depth = depths[d_idx];
    double observed_rate = static_cast<double>(total_violations[d_idx]) / total_queries[d_idx];
    double theoretical_delta = std::exp(-static_cast<double>(depth));

    std::cerr << "# depth=" << static_cast<unsigned>(depth) << ":" << std::endl;
    std::cerr << "#   Theoretical delta = " << std::fixed << std::setprecision(6)
              << theoretical_delta << " (" << (theoretical_delta * 100) << "%)" << std::endl;
    std::cerr << "#   Observed violation rate = " << observed_rate
              << " (" << (observed_rate * 100) << "%)" << std::endl;
    std::cerr << "#   Total queries = " << total_queries[d_idx]
              << ", Violations = " << total_violations[d_idx] << std::endl;

    // Compare observed to theoretical
    double ratio = (theoretical_delta > 0) ? (observed_rate / theoretical_delta) : 0;
    std::cerr << "#   Observed/Theoretical ratio = " << std::setprecision(4) << ratio << std::endl;
    std::cerr << "# " << std::endl;
  }
}

} /* namespace datasketches */
