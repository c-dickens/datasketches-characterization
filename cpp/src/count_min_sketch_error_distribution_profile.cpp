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

#include "count_min_sketch_error_distribution_profile.hpp"
#include "zipf_distribution.hpp"

namespace datasketches {

void count_min_sketch_error_distribution_profile::run() {
  // Sketch parameters - medium size per CLAUDE.md
  const uint32_t num_buckets = 1024;
  const uint8_t num_hashes = 5;

  // Trial parameters
  const size_t num_trials = 1<<10;  // Number of independent trials
  const size_t stream_length = 1<<17;  // 100K items per trial

  // Data distribution parameters
  const unsigned zipf_lg_range = 13;  // 8K distinct values
  const double zipf_exponent = 1.1;   // Zipfian skew

  // Metadata to stderr
  std::cerr << "# Count-Min Sketch Error Distribution Profile" << std::endl;
  std::cerr << "# Parameters: width=" << num_buckets
            << ", depth=" << static_cast<unsigned>(num_hashes)
            << ", stream_length=" << stream_length
            << ", num_trials=" << num_trials << std::endl;
  std::cerr << "# Distribution: Zipf(range=2^" << zipf_lg_range
            << ", exponent=" << zipf_exponent << ")" << std::endl;

  // TSV header to stdout - include configuration for self-describing output
  std::cout << "Trial\tWidth\tDepth\tEpsilon\tTrueFreq\tEstimate\tAbsError\tRelError\tTotalWeight\tErrorBound" << std::endl;

  size_t total_violations = 0;
  size_t total_items = 0;

  for (size_t trial = 0; trial < num_trials; trial++) {
    // Each trial gets a different seed for both zipf and sketch
    const uint64_t trial_seed = 42 + trial * 1000;

    // Create zipf distribution with trial-specific seed
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

    // Create and populate sketch with trial-specific seed
    count_min_sketch<uint64_t> sketch(num_hashes, num_buckets, trial_seed);
    for (size_t j = 0; j < stream_length; j++) {
      sketch.update(values[j]);
    }

    const double epsilon = sketch.get_relative_error();
    const uint64_t total_weight = sketch.get_total_weight();
    const double error_bound = epsilon * total_weight;

    // Output per-item data for this trial
    for (const auto& kv : true_frequencies) {
      uint64_t item = kv.first;
      uint64_t true_freq = kv.second;
      uint64_t estimate = sketch.get_estimate(item);

      int64_t abs_error = static_cast<int64_t>(estimate) - static_cast<int64_t>(true_freq);
      double rel_error = static_cast<double>(abs_error) / true_freq;

      // Check bound
      bool within_bound = (estimate <= true_freq + error_bound);
      if (!within_bound) total_violations++;
      total_items++;

      std::cout << trial << "\t"
                << num_buckets << "\t"
                << static_cast<unsigned>(num_hashes) << "\t"
                << epsilon << "\t"
                << true_freq << "\t"
                << estimate << "\t"
                << abs_error << "\t"
                << rel_error << "\t"
                << total_weight << "\t"
                << error_bound
                << std::endl;
    }

    // Progress indicator to stderr
    if ((trial + 1) % 10 == 0) {
      std::cerr << "# Completed trial " << (trial + 1) << "/" << num_trials << std::endl;
    }
  }

  // Summary to stderr
  std::cerr << "# Summary: " << total_items << " total item queries across "
            << num_trials << " trials, " << total_violations << " bound violations" << std::endl;
}

} /* namespace datasketches */
