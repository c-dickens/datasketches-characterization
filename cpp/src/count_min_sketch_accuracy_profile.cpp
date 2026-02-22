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
#include <iomanip>

#include <count_min.hpp>
#include <kll_sketch.hpp>

#include "count_min_sketch_accuracy_profile.hpp"
#include "zipf_distribution.hpp"

namespace datasketches {

typedef count_min_sketch<int64_t> cms_int64;

// Standard normal quantile fractions: -3σ, -2σ, -1σ, median, +1σ, +2σ, +3σ
static const double FRACTIONS[] = {
  0.00135, 0.02275, 0.15866, 0.5, 0.84134, 0.97725, 0.99865
};
static const size_t NUM_FRACTIONS = 7;

void count_min_sketch_accuracy_profile::run() {
  // Moderate run: ~1-2 min, enough to validate trends
  // Production: lg_max_stream_len=23, lg_min_trials=10, lg_max_trials=18
  const unsigned lg_min_stream_len = 5;
  const unsigned lg_max_stream_len = 17;
  const unsigned ppo = 16;
  const unsigned lg_max_trials = 10;
  const unsigned lg_min_trials = 5;

  const uint32_t num_buckets = cms_int64::suggest_num_buckets(0.01);
  const uint8_t num_hashes = cms_int64::suggest_num_hashes(0.95);

  const unsigned zipf_lg_range = 13;
  const double zipf_exponent = 1.1;

  const double epsilon = std::exp(1.0) / num_buckets;

  // Metadata to stderr
  std::cerr << "# Count-Min Sketch Accuracy Profile — Point Query Error"
            << std::endl;
  std::cerr << "# Parameters:" << std::endl;
  std::cerr << "#   num_buckets (width) = " << num_buckets << std::endl;
  std::cerr << "#   num_hashes (depth) = "
            << static_cast<unsigned>(num_hashes) << std::endl;
  std::cerr << "#   epsilon = e/width = " << std::fixed
            << std::setprecision(6) << epsilon << std::endl;
  std::cerr << "#   zipf_range = 2^" << zipf_lg_range
            << ", zipf_exponent = " << zipf_exponent << std::endl;
  std::cerr << "#   stream_len range = [2^" << lg_min_stream_len
            << ", 2^" << lg_max_stream_len << "]" << std::endl;
  std::cerr << "#   trials range = [2^" << lg_min_trials
            << ", 2^" << lg_max_trials << "]" << std::endl;
  std::cerr << "#   quantile fractions:";
  for (size_t i = 0; i < NUM_FRACTIONS; i++) {
    std::cerr << " " << FRACTIONS[i];
  }
  std::cerr << std::endl;
  std::cerr << "# " << std::endl;

  // TSV header
  std::cout << "StreamLen\tTrials\tNumDistinct\tEpsTotalWeight"
            << "\tp0.00135\tp0.02275\tp0.15866\tp0.50"
            << "\tp0.84134\tp0.97725\tp0.99865"
            << "\tBoundViolationRate"
            << std::endl;

  zipf_distribution zipf(1 << zipf_lg_range, zipf_exponent);

  size_t stream_length = 1 << lg_min_stream_len;
  while (stream_length <= 1ULL << lg_max_stream_len) {
    const size_t num_trials = get_num_trials(
        stream_length, lg_min_stream_len, lg_max_stream_len,
        lg_min_trials, lg_max_trials);

    // KLL sketch to accumulate per-item absolute errors across all trials
    kll_sketch<double> err_sketch(200);

    double total_num_distinct = 0;
    double total_eps_total_weight = 0;
    size_t total_violations = 0;
    size_t total_queries = 0;

    unsigned* values = new unsigned[stream_length];

    for (size_t trial = 0; trial < num_trials; trial++) {
      const uint64_t trial_seed = 42 + trial * 1000;

      // Generate stream
      for (size_t j = 0; j < stream_length; j++) {
        values[j] = zipf.sample();
      }

      // Build exact frequency map
      std::unordered_map<unsigned, int64_t> exact_counts;
      for (size_t j = 0; j < stream_length; j++) {
        exact_counts[values[j]]++;
      }

      // Create and populate sketch
      cms_int64 sketch(num_hashes, num_buckets, trial_seed);
      for (size_t j = 0; j < stream_length; j++) {
        sketch.update(static_cast<int64_t>(values[j]));
      }

      const int64_t total_weight = sketch.get_total_weight();
      const double eps_total_weight = epsilon * total_weight;
      total_eps_total_weight += eps_total_weight;

      const size_t num_distinct = exact_counts.size();
      total_num_distinct += num_distinct;

      for (const auto& kv : exact_counts) {
        int64_t est = sketch.get_estimate(
            static_cast<int64_t>(kv.first));
        int64_t true_count = kv.second;

        // CMS overestimates: abs_err = est - true_count (always >= 0)
        double abs_err = static_cast<double>(est - true_count);
        if (abs_err < 0) abs_err = 0; // defensive

        err_sketch.update(abs_err);

        // CMS theoretical bound: overestimation <= eps * total_weight
        if (abs_err > eps_total_weight) {
          total_violations++;
        }
        total_queries++;
      }
    }

    delete[] values;

    double violation_rate = (total_queries > 0)
        ? static_cast<double>(total_violations) / total_queries : 0;

    // Output row
    std::cout << stream_length << "\t"
              << num_trials << "\t"
              << std::fixed << std::setprecision(1)
              << (total_num_distinct / num_trials) << "\t"
              << std::setprecision(2)
              << (total_eps_total_weight / num_trials);

    for (size_t i = 0; i < NUM_FRACTIONS; i++) {
      std::cout << "\t" << std::setprecision(4)
                << err_sketch.get_quantile(FRACTIONS[i]);
    }

    std::cout << "\t" << std::setprecision(8) << violation_rate
              << std::endl;

    stream_length = pwr_2_law_next(ppo, stream_length);
  }
}

} /* namespace datasketches */
