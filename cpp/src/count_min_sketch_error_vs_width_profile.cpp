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

#include <count_min.hpp>

#include "count_min_sketch_error_vs_width_profile.hpp"
#include "zipf_distribution.hpp"

namespace datasketches {

void count_min_sketch_error_vs_width_profile::run() {
  // Sketch width parameters to compare: lg_width = 8, 10, 12, 14
  // Corresponding widths: 256, 1024, 4096, 16384
  const std::vector<unsigned> lg_widths = {8, 10, 12, 14};

  // Fixed depth (number of hash functions) for error comparison
  const uint8_t num_hashes = 5;

  // Trial parameters
  const size_t num_trials = 1 << 10;  // 1024 independent trials

  // Stream length - chosen to surface stream length effects
  // Using 2^17 = 131072 items per trial
  const size_t stream_length = 1 << 17;

  // Data distribution parameters
  const unsigned zipf_lg_range = 13;  // 8K distinct values
  const double zipf_exponent = 1.1;   // Zipfian skew

  // Metadata to stderr
  std::cerr << "# Count-Min Sketch Error vs Width Profile" << std::endl;
  std::cerr << "# Comparing lg_widths: ";
  for (unsigned lg_w : lg_widths) {
    std::cerr << lg_w << " ";
  }
  std::cerr << std::endl;
  std::cerr << "# Parameters: depth=" << static_cast<unsigned>(num_hashes)
            << ", stream_length=" << stream_length
            << ", num_trials=" << num_trials << std::endl;
  std::cerr << "# Distribution: Zipf(range=2^" << zipf_lg_range
            << ", exponent=" << zipf_exponent << ")" << std::endl;
  std::cerr << "# Theoretical epsilon = e / width" << std::endl;
  std::cerr << "# Theoretical bound: estimate <= true_freq + epsilon * N" << std::endl;

  // TSV header - self-describing output for analysis
  std::cout << "Trial\tLgWidth\tWidth\tDepth\tEpsilon\tTheoreticalEpsilon\t"
            << "TrueFreq\tEstimate\tAbsError\tRelError\t"
            << "TotalWeight\tErrorBound\tWithinBound\t"
            << "FreqRatio\tNumDistinct" << std::endl;

  // Track aggregate statistics per width
  std::vector<size_t> total_violations(lg_widths.size(), 0);
  std::vector<size_t> total_items(lg_widths.size(), 0);

  for (size_t trial = 0; trial < num_trials; trial++) {
    // Each trial gets a unique seed
    const uint64_t trial_seed = 42 + trial * 1000;

    // Create zipf distribution
    zipf_distribution zipf(1 << zipf_lg_range, zipf_exponent);

    // Generate stream data - SAME stream for all width configurations
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

    // Test each width configuration with the SAME stream
    for (size_t w_idx = 0; w_idx < lg_widths.size(); w_idx++) {
      unsigned lg_width = lg_widths[w_idx];
      uint32_t width = 1 << lg_width;

      // Create and populate sketch
      count_min_sketch<uint64_t> sketch(num_hashes, width, trial_seed);
      for (size_t j = 0; j < stream_length; j++) {
        sketch.update(values[j]);
      }

      const double epsilon = sketch.get_relative_error();
      const double theoretical_epsilon = std::exp(1.0) / width;
      const uint64_t total_weight = sketch.get_total_weight();
      const double error_bound = epsilon * total_weight;

      // Output per-item data for this trial and width
      for (const auto& kv : true_frequencies) {
        uint64_t item = kv.first;
        uint64_t true_freq = kv.second;
        uint64_t estimate = sketch.get_estimate(item);

        int64_t abs_error = static_cast<int64_t>(estimate) - static_cast<int64_t>(true_freq);
        double rel_error = static_cast<double>(abs_error) / true_freq;

        // Check theoretical bound
        bool within_bound = (estimate <= true_freq + error_bound);
        if (!within_bound) total_violations[w_idx]++;
        total_items[w_idx]++;

        // Frequency ratio: how frequent is this item relative to total?
        double freq_ratio = static_cast<double>(true_freq) / total_weight;

        std::cout << trial << "\t"
                  << lg_width << "\t"
                  << width << "\t"
                  << static_cast<unsigned>(num_hashes) << "\t"
                  << epsilon << "\t"
                  << theoretical_epsilon << "\t"
                  << true_freq << "\t"
                  << estimate << "\t"
                  << abs_error << "\t"
                  << rel_error << "\t"
                  << total_weight << "\t"
                  << error_bound << "\t"
                  << (within_bound ? 1 : 0) << "\t"
                  << freq_ratio << "\t"
                  << num_distinct
                  << std::endl;
      }
    }

    // Progress indicator to stderr
    if ((trial + 1) % 100 == 0) {
      std::cerr << "# Completed trial " << (trial + 1) << "/" << num_trials << std::endl;
    }
  }

  // Summary statistics to stderr
  std::cerr << "# Summary:" << std::endl;
  for (size_t w_idx = 0; w_idx < lg_widths.size(); w_idx++) {
    unsigned lg_width = lg_widths[w_idx];
    double violation_rate = static_cast<double>(total_violations[w_idx]) / total_items[w_idx];
    double theoretical_epsilon = std::exp(1.0) / (1 << lg_width);
    std::cerr << "#   lg_width=" << lg_width
              << " (width=" << (1 << lg_width) << ")"
              << ": epsilon=" << theoretical_epsilon
              << ", violations=" << total_violations[w_idx]
              << "/" << total_items[w_idx]
              << " (" << (violation_rate * 100) << "%)"
              << std::endl;
  }
}

} /* namespace datasketches */
