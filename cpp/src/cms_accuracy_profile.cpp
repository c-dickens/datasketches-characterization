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
#include <iomanip>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>

#include <count_min.hpp>

#include "cms_accuracy_profile.hpp"
#include "zipf_distribution.hpp"
#include "kll_sketch.hpp"

namespace datasketches {

/**
 * CMS Accuracy Profile
 *
 * This profile characterizes the accuracy of Count-Min Sketch by:
 * 1. Maintaining constant load factor (distinct_items / width) across configurations
 * 2. Measuring error metrics across the frequency spectrum
 * 3. Comparing empirical errors against theoretical bounds
 *
 * Load Factor Rationale:
 * - Too sparse (d/w < 0.5): Most items get dedicated buckets, trivial errors
 * - Sweet spot (d/w ~ 2-8): Meaningful collisions, interesting error patterns
 * - Saturated (d/w > 32): Everything collides, worst-case throughout
 *
 * We target d/w ~ 4 by setting lg_range = lg_width + 2
 *
 * Why the theoretical error bound (epsilon * N) is constant across widths:
 * - Theoretical bound = (e/w) * N where w = width, N = total stream weight
 * - We scale stream weight proportionally with width to maintain constant load factor:
 *     distinct_items = 4 * width, stream_weight = 64 * width
 * - Therefore: (e/w) * (64*w) = 64*e ≈ 174 (constant!)
 *
 * Why constant load factor matters for comparison:
 * - Ensures all width configurations operate in the same collision regime
 * - Avoids comparing overloaded small sketches vs underloaded large ones
 * - The constant theoretical bound provides a fixed reference line
 * - Empirical errors staying well below this bound (and roughly constant)
 *   shows CMS performs consistently relative to worst-case guarantees
 */
void cms_accuracy_profile::run() {
  // Configuration parameters
  const unsigned depth = 5;  // Fixed depth (number of hash functions)

  // Width sweep: 256, 512, 1024, 2048, 4096
  const unsigned lg_min_width = 8;
  const unsigned lg_max_width = 12;

  // Trials configuration
  const unsigned lg_max_trials = 14;  // More trials for smaller widths
  const unsigned lg_min_trials = 10;  // Fewer trials for larger widths

  // Zipf distribution parameters
  // Primary test: alpha = 1.1 (high skew, realistic)
  // Alternative: alpha = 0.7 (lower skew, stress test)
  const double zipf_exponent = 1.1;

  // Stream length multiplier (stream_length = distinct_items * multiplier)
  const unsigned stream_multiplier = 16;

  // Print header
  std::cout << "# CMS Accuracy Profile" << std::endl;
  std::cout << "# depth=" << depth << " zipf_exponent=" << zipf_exponent << std::endl;
  std::cout << "# Load factor d/w ~ 4 (lg_range = lg_width + 2)" << std::endl;
  std::cout << "# Stream multiplier=" << stream_multiplier << std::endl;
  std::cout << "# Columns: width, depth, trials, distinct_items, stream_length, "
            << "theoretical_max_error, mean_abs_error, median_abs_error, "
            << "p95_abs_error, max_abs_error, mean_rel_error, median_rel_error, "
            << "p95_rel_error, frac_exceeding_bound" << std::endl;

  // Iterate over width configurations
  for (unsigned lg_width = lg_min_width; lg_width <= lg_max_width; lg_width++) {
    const uint32_t width = 1 << lg_width;

    // Set lg_range = lg_width + 2 to achieve load factor d/w ~ 4
    const unsigned lg_range = lg_width + 2;
    const unsigned distinct_items = 1 << lg_range;
    const size_t stream_length = static_cast<size_t>(distinct_items) * stream_multiplier;

    // Theoretical error bound: epsilon * N where epsilon = e / width
    // For CMS: epsilon = e / width (approximately 2.718 / width)
    const double epsilon = std::exp(1.0) / width;
    const double theoretical_max_error = epsilon * stream_length;

    // Adaptive trial count: more trials for smaller widths
    const size_t num_trials = get_num_trials(width, 1 << lg_min_width, 1 << lg_max_width,
                                              lg_min_trials, lg_max_trials);

    // Initialize Zipf distribution
    zipf_distribution zipf(distinct_items, zipf_exponent);

    // Accumulators for error statistics across trials
    double sum_abs_error = 0;
    double sum_rel_error = 0;
    size_t total_exceeding_bound = 0;
    size_t total_items_queried = 0;

    // KLL sketches for tracking error distributions
    kll_sketch<double> abs_error_sketch(200);
    kll_sketch<double> rel_error_sketch(200);

    for (size_t trial = 0; trial < num_trials; trial++) {
      // Generate stream using Zipf distribution
      std::vector<uint64_t> stream(stream_length);
      for (size_t i = 0; i < stream_length; i++) {
        stream[i] = zipf.sample();
      }

      // Compute true frequencies
      std::unordered_map<uint64_t, uint64_t> true_frequencies;
      for (size_t i = 0; i < stream_length; i++) {
        true_frequencies[stream[i]]++;
      }

      // Create and populate CMS
      count_min_sketch<uint64_t> cms(depth, width);
      for (size_t i = 0; i < stream_length; i++) {
        cms.update(stream[i]);
      }

      // Query all items and compute errors
      for (const auto& kv : true_frequencies) {
        uint64_t item = kv.first;
        uint64_t true_freq = kv.second;
        uint64_t estimated_freq = cms.get_estimate(item);

        // Absolute error (CMS always overestimates, so estimate >= true)
        double abs_error = static_cast<double>(estimated_freq) - static_cast<double>(true_freq);

        // Relative error
        double rel_error = abs_error / static_cast<double>(true_freq);

        abs_error_sketch.update(abs_error);
        rel_error_sketch.update(rel_error);

        sum_abs_error += abs_error;
        sum_rel_error += rel_error;
        total_items_queried++;

        // Check if error exceeds theoretical bound
        if (abs_error > theoretical_max_error) {
          total_exceeding_bound++;
        }
      }
    }

    // Compute summary statistics
    double mean_abs_error = sum_abs_error / total_items_queried;
    double median_abs_error = abs_error_sketch.get_quantile(0.5);
    double p95_abs_error = abs_error_sketch.get_quantile(0.95);
    double max_abs_error = abs_error_sketch.get_max_item();

    double mean_rel_error = sum_rel_error / total_items_queried;
    double median_rel_error = rel_error_sketch.get_quantile(0.5);
    double p95_rel_error = rel_error_sketch.get_quantile(0.95);

    double frac_exceeding = static_cast<double>(total_exceeding_bound) /
                            static_cast<double>(total_items_queried);

    // Output summary row
    std::cout << width << "\t"
              << depth << "\t"
              << num_trials << "\t"
              << distinct_items << "\t"
              << stream_length << "\t"
              << theoretical_max_error << "\t"
              << mean_abs_error << "\t"
              << median_abs_error << "\t"
              << p95_abs_error << "\t"
              << max_abs_error << "\t"
              << mean_rel_error << "\t"
              << median_rel_error << "\t"
              << p95_rel_error << "\t"
              << frac_exceeding << std::endl;
  }
}

} /* namespace datasketches */
