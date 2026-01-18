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

#include "count_min_sketch_error_vs_freq_profile.hpp"
#include "zipf_distribution.hpp"

namespace datasketches {

void count_min_sketch_error_vs_freq_profile::run() {
  // Sketch parameters - medium size per CLAUDE.md
  const uint32_t num_buckets = 1024;
  const uint8_t num_hashes = 5;
  const uint64_t seed = 42;

  // Stream parameters - fixed size for per-item analysis
  const size_t stream_length = 1000000;  // 1M items

  // Data distribution parameters
  const unsigned zipf_lg_range = 13;  // 8K distinct values
  const double zipf_exponent = 1.1;   // Zipfian skew

  zipf_distribution zipf(1 << zipf_lg_range, zipf_exponent);

  // Generate stream data
  std::vector<uint64_t> values(stream_length);
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
  const double error_bound = epsilon * total_weight;  // This is the key bound!

  // Output header - per-item data for visualization
  std::cerr << "# Count-Min Sketch Per-Item Error vs Frequency Profile" << std::endl;
  std::cerr << "# Parameters: width=" << num_buckets
            << ", depth=" << static_cast<unsigned>(num_hashes)
            << ", stream_length=" << stream_length
            << ", distinct_items=" << true_frequencies.size() << std::endl;
  std::cerr << "# Theoretical epsilon=" << epsilon
            << ", total_weight=" << total_weight
            << ", error_bound=" << error_bound << std::endl;
  std::cerr << "# Key property: true_freq <= estimate <= true_freq + error_bound" << std::endl;
  std::cerr << "#" << std::endl;

  // TSV header to stdout
  std::cout << "TrueFreq\tEstimate\tAbsError\tRelError\tErrorBound\tWithinBound" << std::endl;

  // Query all distinct items and output per-item data
  size_t violations = 0;
  for (const auto& kv : true_frequencies) {
    uint64_t item = kv.first;
    uint64_t true_freq = kv.second;
    uint64_t estimate = sketch.get_estimate(item);

    // Count-Min always overestimates (or exact)
    int64_t abs_error = static_cast<int64_t>(estimate) - static_cast<int64_t>(true_freq);
    double rel_error = static_cast<double>(abs_error) / true_freq;

    // Check the key guarantee: estimate <= true_freq + epsilon * total_weight
    bool within_bound = (estimate <= true_freq + error_bound);
    if (!within_bound) violations++;

    std::cout << true_freq << "\t"
              << estimate << "\t"
              << abs_error << "\t"
              << rel_error << "\t"
              << error_bound << "\t"
              << (within_bound ? "1" : "0")
              << std::endl;
  }

  // Summary statistics to stderr
  std::cerr << "# Summary: " << true_frequencies.size() << " distinct items, "
            << violations << " bound violations" << std::endl;
}

} /* namespace datasketches */
