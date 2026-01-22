/*
 * Standalone CMS Accuracy Test
 * Builds without the full characterization framework
 */

#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <random>
#include <chrono>

#include <count_min.hpp>
#include <kll_sketch.hpp>

using namespace datasketches;

// Simple Zipf distribution implementation
class zipf_distribution {
public:
    zipf_distribution(uint64_t n, double alpha, uint64_t seed = 0)
        : n_(n), alpha_(alpha), gen_(seed ? seed : std::random_device{}()) {
        // Precompute normalization constant
        c_ = 0;
        for (uint64_t i = 1; i <= n_; i++) {
            c_ += 1.0 / std::pow(static_cast<double>(i), alpha_);
        }
        c_ = 1.0 / c_;
    }

    uint64_t sample() {
        double u = uniform_(gen_);
        double sum = 0;
        for (uint64_t i = 1; i <= n_; i++) {
            sum += c_ / std::pow(static_cast<double>(i), alpha_);
            if (sum >= u) return i;
        }
        return n_;
    }

private:
    uint64_t n_;
    double alpha_;
    double c_;
    std::mt19937_64 gen_;
    std::uniform_real_distribution<double> uniform_{0.0, 1.0};
};

size_t get_num_trials(size_t x, size_t min_x, size_t max_x,
                      unsigned lg_min_trials, unsigned lg_max_trials) {
    double log_x = std::log2(static_cast<double>(x));
    double log_min = std::log2(static_cast<double>(min_x));
    double log_max = std::log2(static_cast<double>(max_x));

    double t = (log_x - log_min) / (log_max - log_min);
    double lg_trials = lg_max_trials - t * (lg_max_trials - lg_min_trials);
    return static_cast<size_t>(std::pow(2.0, lg_trials));
}

int main() {
    // Configuration parameters
    const unsigned depth = 5;  // Fixed depth (number of hash functions)

    // Width sweep: 256, 512, 1024, 2048, 4096
    const unsigned lg_min_width = 8;
    const unsigned lg_max_width = 12;

    // Trials configuration
    const unsigned lg_max_trials = 12;  // Reduced for faster execution
    const unsigned lg_min_trials = 8;

    // Zipf distribution parameters
    const double zipf_exponent = 1.1;

    // Stream length multiplier
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
        const double epsilon = std::exp(1.0) / width;
        const double theoretical_max_error = epsilon * stream_length;

        // Adaptive trial count
        const size_t num_trials = get_num_trials(width, 1 << lg_min_width, 1 << lg_max_width,
                                                  lg_min_trials, lg_max_trials);

        std::cerr << "Running width=" << width << " trials=" << num_trials << "..." << std::endl;

        // Initialize Zipf distribution
        zipf_distribution zipf(distinct_items, zipf_exponent);

        // Accumulators
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

                // Absolute error
                double abs_error = static_cast<double>(estimated_freq) - static_cast<double>(true_freq);

                // Relative error
                double rel_error = abs_error / static_cast<double>(true_freq);

                abs_error_sketch.update(abs_error);
                rel_error_sketch.update(rel_error);

                sum_abs_error += abs_error;
                sum_rel_error += rel_error;
                total_items_queried++;

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

    return 0;
}
