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

#include "count_min_sketch_stream_length_analysis_profile.hpp"
#include "zipf_distribution.hpp"

namespace datasketches {

void count_min_sketch_stream_length_analysis_profile::run() {
  // Stream length parameters to vary: 2^10 to 2^22
  const std::vector<unsigned> lg_stream_lengths = {10, 12, 14, 16, 18, 20, 22};

  // Fixed sketch parameters
  const uint32_t num_buckets = 1024;  // width = 1024
  const uint8_t num_hashes = 5;       // depth = 5

  // Trial parameters
  const size_t num_trials = 256;

  // Data distribution parameters
  const unsigned zipf_lg_range = 13;  // 8K distinct values
  const double zipf_exponent = 1.1;   // Zipfian skew

  // Theoretical parameters
  const double epsilon = std::exp(1.0) / num_buckets;

  // Metadata to stderr
  std::cerr << "# Count-Min Sketch Stream Length Analysis Profile" << std::endl;
  std::cerr << "# " << std::endl;
  std::cerr << "# PURPOSE: Investigate failure mode where absolute error bound εN" << std::endl;
  std::cerr << "#          grows with stream length, degrading low-frequency estimates" << std::endl;
  std::cerr << "# " << std::endl;
  std::cerr << "# Fixed parameters:" << std::endl;
  std::cerr << "#   width=" << num_buckets << ", depth=" << static_cast<unsigned>(num_hashes) << std::endl;
  std::cerr << "#   epsilon=e/w=" << std::fixed << std::setprecision(6) << epsilon << std::endl;
  std::cerr << "#   num_trials=" << num_trials << std::endl;
  std::cerr << "# " << std::endl;
  std::cerr << "# Varying stream lengths (lg): ";
  for (unsigned lg_n : lg_stream_lengths) {
    std::cerr << lg_n << " ";
  }
  std::cerr << std::endl;
  std::cerr << "# Distribution: Zipf(range=2^" << zipf_lg_range
            << ", exponent=" << zipf_exponent << ")" << std::endl;

  // TSV header
  std::cout << "LgStreamLen\tStreamLen\tErrorBound\t"
            << "NumDistinct\tAvgFreq\tMaxFreq\tMinFreq\t"
            << "AvgAbsError\tMaxAbsError\t"
            << "AvgRelErrorLowFreq\tAvgRelErrorHighFreq\t"
            << "ItemsAboveThreshold\tFracAboveThreshold\t"
            << "ViolationRate" << std::endl;

  for (unsigned lg_n : lg_stream_lengths) {
    size_t stream_length = 1ULL << lg_n;
    double error_bound = epsilon * stream_length;

    // Accumulators across trials
    double total_avg_abs_error = 0;
    double total_max_abs_error = 0;
    double total_avg_rel_error_low = 0;
    double total_avg_rel_error_high = 0;
    size_t total_low_freq_items = 0;
    size_t total_high_freq_items = 0;
    size_t total_above_threshold = 0;
    size_t total_distinct = 0;
    size_t total_violations = 0;
    size_t total_queries = 0;

    double total_max_freq = 0;
    double total_min_freq = 0;
    double total_avg_freq = 0;

    for (size_t trial = 0; trial < num_trials; trial++) {
      const uint64_t trial_seed = 42 + trial * 1000;

      // Create zipf distribution
      zipf_distribution zipf(1 << zipf_lg_range, zipf_exponent);

      // Generate stream
      std::vector<uint64_t> values(stream_length);
      for (size_t j = 0; j < stream_length; j++) {
        values[j] = zipf.sample();
      }

      // Ground truth
      std::unordered_map<uint64_t, uint64_t> true_frequencies;
      for (size_t j = 0; j < stream_length; j++) {
        true_frequencies[values[j]]++;
      }

      // Create and populate sketch
      count_min_sketch<uint64_t> sketch(num_hashes, num_buckets, trial_seed);
      for (size_t j = 0; j < stream_length; j++) {
        sketch.update(values[j]);
      }

      const uint64_t total_weight = sketch.get_total_weight();

      // Compute statistics
      size_t num_distinct = true_frequencies.size();
      total_distinct += num_distinct;

      double sum_abs_error = 0;
      double max_abs_error = 0;
      double sum_rel_error_low = 0;
      double sum_rel_error_high = 0;
      size_t low_freq_count = 0;
      size_t high_freq_count = 0;
      size_t above_threshold = 0;
      uint64_t max_freq = 0;
      uint64_t min_freq = UINT64_MAX;
      uint64_t sum_freq = 0;

      // Threshold: items with true freq > error_bound are "useful"
      for (const auto& kv : true_frequencies) {
        uint64_t item = kv.first;
        uint64_t true_freq = kv.second;
        uint64_t estimate = sketch.get_estimate(item);

        sum_freq += true_freq;
        max_freq = std::max(max_freq, true_freq);
        min_freq = std::min(min_freq, true_freq);

        uint64_t abs_error = (estimate >= true_freq) ? (estimate - true_freq) : 0;
        double rel_error = static_cast<double>(abs_error) / true_freq;

        sum_abs_error += abs_error;
        max_abs_error = std::max(max_abs_error, static_cast<double>(abs_error));

        // Check if bound is violated
        if (estimate > true_freq + error_bound) {
          total_violations++;
        }
        total_queries++;

        // Classify as low or high frequency relative to error bound
        if (true_freq <= error_bound) {
          sum_rel_error_low += rel_error;
          low_freq_count++;
        } else {
          sum_rel_error_high += rel_error;
          high_freq_count++;
          above_threshold++;
        }
      }

      total_avg_abs_error += sum_abs_error / num_distinct;
      total_max_abs_error = std::max(total_max_abs_error, max_abs_error);

      if (low_freq_count > 0) {
        total_avg_rel_error_low += sum_rel_error_low / low_freq_count;
        total_low_freq_items += low_freq_count;
      }
      if (high_freq_count > 0) {
        total_avg_rel_error_high += sum_rel_error_high / high_freq_count;
        total_high_freq_items += high_freq_count;
      }

      total_above_threshold += above_threshold;
      total_max_freq += max_freq;
      total_min_freq += min_freq;
      total_avg_freq += static_cast<double>(sum_freq) / num_distinct;
    }

    // Compute trial averages
    double avg_num_distinct = static_cast<double>(total_distinct) / num_trials;
    double avg_abs_error = total_avg_abs_error / num_trials;
    double avg_rel_error_low = (total_low_freq_items > 0) ?
                               (total_avg_rel_error_low / num_trials) : 0;
    double avg_rel_error_high = (total_high_freq_items > 0) ?
                                (total_avg_rel_error_high / num_trials) : 0;
    double avg_above_threshold = static_cast<double>(total_above_threshold) / num_trials;
    double frac_above_threshold = avg_above_threshold / avg_num_distinct;
    double violation_rate = static_cast<double>(total_violations) / total_queries;

    double avg_max_freq = total_max_freq / num_trials;
    double avg_min_freq = total_min_freq / num_trials;
    double avg_avg_freq = total_avg_freq / num_trials;

    std::cout << lg_n << "\t"
              << stream_length << "\t"
              << std::fixed << std::setprecision(2) << error_bound << "\t"
              << std::setprecision(1) << avg_num_distinct << "\t"
              << avg_avg_freq << "\t"
              << avg_max_freq << "\t"
              << avg_min_freq << "\t"
              << std::setprecision(2) << avg_abs_error << "\t"
              << total_max_abs_error << "\t"
              << std::setprecision(4) << avg_rel_error_low << "\t"
              << avg_rel_error_high << "\t"
              << std::setprecision(1) << avg_above_threshold << "\t"
              << std::setprecision(4) << frac_above_threshold << "\t"
              << std::setprecision(6) << violation_rate
              << std::endl;

    std::cerr << "# Completed lg_n=" << lg_n << " (N=" << stream_length << ")" << std::endl;
  }

  // Summary to stderr
  std::cerr << "# " << std::endl;
  std::cerr << "# ========================================" << std::endl;
  std::cerr << "# KEY INSIGHT: Stream Length Failure Mode" << std::endl;
  std::cerr << "# ========================================" << std::endl;
  std::cerr << "# " << std::endl;
  std::cerr << "# The CMS guarantee is: estimate <= true_freq + epsilon * N" << std::endl;
  std::cerr << "# This means:" << std::endl;
  std::cerr << "#   1. Absolute error bound grows linearly with N" << std::endl;
  std::cerr << "#   2. For items with true_freq << epsilon*N, relative error is large" << std::endl;
  std::cerr << "#   3. CMS is useful primarily for 'heavy hitters' (freq > epsilon*N)" << std::endl;
  std::cerr << "# " << std::endl;
  std::cerr << "# Practical implication:" << std::endl;
  std::cerr << "#   If you need accurate counts for low-frequency items," << std::endl;
  std::cerr << "#   increase sketch width or use a different data structure." << std::endl;
}

} /* namespace datasketches */
