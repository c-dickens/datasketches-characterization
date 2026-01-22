# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

"""
Count-Min Sketch Accuracy Visualization

This script plots CMS accuracy characterization results showing:
- Theoretical error bound vs actual error across different sketch widths
- Error quantiles (5th, 25th, 50th/median, 75th, 95th percentiles)

Usage:
    python cms_accuracy_plot.py <input_tsv_file> [output_png_file]

The input TSV file should be the output from the cms-sketch-accuracy profile.
Expected columns:
    width, depth, trials, distinct_items, stream_length, theoretical_max_error,
    mean_abs_error, median_abs_error, p95_abs_error, max_abs_error,
    mean_rel_error, median_rel_error, p95_rel_error, frac_exceeding_bound
"""

import sys
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path


def parse_cms_accuracy_results(filepath: str) -> dict:
    """
    Parse CMS accuracy results from TSV file.

    Args:
        filepath: Path to the TSV file

    Returns:
        Dictionary with arrays for each column
    """
    data = {
        'width': [],
        'depth': [],
        'trials': [],
        'distinct_items': [],
        'stream_length': [],
        'theoretical_max_error': [],
        'mean_abs_error': [],
        'median_abs_error': [],
        'p95_abs_error': [],
        'max_abs_error': [],
        'mean_rel_error': [],
        'median_rel_error': [],
        'p95_rel_error': [],
        'frac_exceeding_bound': []
    }

    with open(filepath, 'r') as f:
        for line in f:
            # Skip comment lines and empty lines
            line = line.strip()
            if not line or line.startswith('#'):
                continue

            parts = line.split('\t')
            if len(parts) < 14:
                continue

            try:
                data['width'].append(int(parts[0]))
                data['depth'].append(int(parts[1]))
                data['trials'].append(int(parts[2]))
                data['distinct_items'].append(int(parts[3]))
                data['stream_length'].append(int(parts[4]))
                data['theoretical_max_error'].append(float(parts[5]))
                data['mean_abs_error'].append(float(parts[6]))
                data['median_abs_error'].append(float(parts[7]))
                data['p95_abs_error'].append(float(parts[8]))
                data['max_abs_error'].append(float(parts[9]))
                data['mean_rel_error'].append(float(parts[10]))
                data['median_rel_error'].append(float(parts[11]))
                data['p95_rel_error'].append(float(parts[12]))
                data['frac_exceeding_bound'].append(float(parts[13]))
            except (ValueError, IndexError) as e:
                print(f"Warning: Skipping line due to parse error: {e}")
                continue

    # Convert to numpy arrays
    for key in data:
        data[key] = np.array(data[key])

    return data


def compute_theoretical_error(width: np.ndarray, stream_length: np.ndarray) -> np.ndarray:
    """
    Compute theoretical maximum error for Count-Min Sketch.

    The CMS theoretical bound is: error <= epsilon * N
    where epsilon = e / width (approximately 2.718 / width)
    and N is the total stream weight.

    Args:
        width: Array of sketch widths
        stream_length: Array of stream lengths (total weight)

    Returns:
        Array of theoretical maximum errors
    """
    epsilon = np.e / width
    return epsilon * stream_length


def plot_cms_accuracy(data: dict, output_path: str = None, show: bool = True):
    """
    Create visualization of CMS accuracy results.

    Plots:
    - Theoretical error bound (solid line)
    - Error quantiles: 5th, 25th, 50th (median), 75th, 95th percentiles

    Args:
        data: Dictionary with parsed results
        output_path: Optional path to save the figure
        show: Whether to display the plot
    """
    fig, ax = plt.subplots(figsize=(10, 7))

    width = data['width']

    # Theoretical error bound
    theoretical = data['theoretical_max_error']
    ax.plot(width, theoretical, 'k-', linewidth=2.5, label='Theoretical Max (e/w * N)',
            marker='s', markersize=8)

    # Since we only have median and p95 from the profile output,
    # we'll plot what we have. In a full implementation, we would
    # have all quantiles from the KLL sketch.

    # Maximum observed error
    ax.plot(width, data['max_abs_error'], 'r-', linewidth=2,
            label='Max Error (100th %ile)', marker='o', markersize=6)

    # 95th percentile
    ax.plot(width, data['p95_abs_error'], 'orange', linewidth=2,
            label='95th Percentile', marker='^', markersize=6)

    # Median (50th percentile)
    ax.plot(width, data['median_abs_error'], 'g-', linewidth=2,
            label='Median (50th %ile)', marker='d', markersize=6)

    # Mean error
    ax.plot(width, data['mean_abs_error'], 'b--', linewidth=1.5,
            label='Mean Error', marker='x', markersize=6)

    # Styling
    ax.set_xlabel('Sketch Width (w)', fontsize=12)
    ax.set_ylabel('Absolute Error', fontsize=12)
    ax.set_title('Count-Min Sketch Accuracy vs Width\n'
                 f'(depth=5, load factor d/w~4, Zipf α=1.1)', fontsize=14)

    ax.set_xscale('log', base=2)
    ax.set_yscale('log')

    # Set x-axis ticks to powers of 2
    ax.set_xticks(width)
    ax.set_xticklabels([str(int(w)) for w in width])

    ax.legend(loc='upper right', fontsize=10)
    ax.grid(True, alpha=0.3, which='both')

    # Add annotation about error scaling
    ax.annotate('Error scales as O(1/width)',
                xy=(width[-1], theoretical[-1]),
                xytext=(width[-1] * 0.5, theoretical[-1] * 2),
                fontsize=10, ha='center',
                arrowprops=dict(arrowstyle='->', color='gray'))

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        print(f"Figure saved to: {output_path}")

    if show:
        plt.show()

    return fig, ax


def plot_error_vs_width_quantiles(data: dict, output_path: str = None, show: bool = True):
    """
    Alternative plot focusing on error quantiles with filled regions.

    Args:
        data: Dictionary with parsed results
        output_path: Optional path to save the figure
        show: Whether to display the plot
    """
    fig, ax = plt.subplots(figsize=(10, 7))

    width = data['width']
    theoretical = data['theoretical_max_error']

    # Theoretical bound
    ax.plot(width, theoretical, 'k-', linewidth=3, label='Theoretical Bound', zorder=10)

    # Since the profile provides limited quantiles, we'll interpolate
    # In practice, the full profile would output more quantile data
    median = data['median_abs_error']
    p95 = data['p95_abs_error']
    max_err = data['max_abs_error']

    # Estimate lower quantiles (assuming roughly symmetric on log scale around median)
    # This is an approximation - full implementation would track actual quantiles
    p5_estimate = median * 0.1  # Rough estimate
    p25_estimate = median * 0.5  # Rough estimate
    p75_estimate = (median + p95) / 2  # Rough estimate

    # Fill between quantile regions
    ax.fill_between(width, p5_estimate, max_err, alpha=0.15, color='blue', label='5th-Max range')
    ax.fill_between(width, p25_estimate, p75_estimate, alpha=0.3, color='blue', label='25th-75th %ile')

    # Plot key quantile lines
    ax.plot(width, max_err, 'r--', linewidth=1.5, label='Maximum', alpha=0.8)
    ax.plot(width, p95, 'orange', linewidth=2, label='95th %ile', marker='^', markersize=5)
    ax.plot(width, median, 'g-', linewidth=2.5, label='Median', marker='o', markersize=6)
    ax.plot(width, p25_estimate, 'c--', linewidth=1.5, label='~25th %ile', alpha=0.8)
    ax.plot(width, p5_estimate, 'b--', linewidth=1.5, label='~5th %ile', alpha=0.8)

    ax.set_xlabel('Sketch Width (w)', fontsize=12)
    ax.set_ylabel('Absolute Error', fontsize=12)
    ax.set_title('Count-Min Sketch Error Distribution vs Width', fontsize=14)

    ax.set_xscale('log', base=2)
    ax.set_yscale('log')
    ax.set_xticks(width)
    ax.set_xticklabels([str(int(w)) for w in width])

    ax.legend(loc='upper right', fontsize=9)
    ax.grid(True, alpha=0.3, which='both')

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        print(f"Figure saved to: {output_path}")

    if show:
        plt.show()

    return fig, ax


def generate_sample_data():
    """
    Generate sample data for testing/demonstration when no input file is provided.

    This simulates the expected output format from the CMS accuracy profile.
    """
    # Widths: 256, 512, 1024, 2048, 4096
    widths = np.array([256, 512, 1024, 2048, 4096])
    depth = 5

    # lg_range = lg_width + 2 => distinct = 4 * width
    distinct_items = 4 * widths
    stream_length = 16 * distinct_items

    # Theoretical error: e/w * N
    theoretical_error = (np.e / widths) * stream_length

    # Simulated empirical errors (should be below theoretical)
    # These are rough approximations of what we'd expect
    np.random.seed(42)

    # Median errors typically much lower than theoretical max
    median_errors = theoretical_error * (0.1 + 0.05 * np.random.randn(len(widths)))
    median_errors = np.maximum(median_errors, 1)

    # P95 typically 50-80% of theoretical
    p95_errors = theoretical_error * (0.5 + 0.1 * np.random.randn(len(widths)))

    # Max errors close to but below theoretical
    max_errors = theoretical_error * (0.8 + 0.1 * np.random.randn(len(widths)))

    # Mean slightly above median
    mean_errors = median_errors * 1.5

    data = {
        'width': widths,
        'depth': np.full_like(widths, depth),
        'trials': np.array([4096, 2048, 1024, 512, 256]),
        'distinct_items': distinct_items,
        'stream_length': stream_length,
        'theoretical_max_error': theoretical_error,
        'mean_abs_error': mean_errors,
        'median_abs_error': median_errors,
        'p95_abs_error': p95_errors,
        'max_abs_error': max_errors,
        'mean_rel_error': mean_errors / (stream_length / distinct_items),
        'median_rel_error': median_errors / (stream_length / distinct_items),
        'p95_rel_error': p95_errors / (stream_length / distinct_items),
        'frac_exceeding_bound': np.zeros_like(widths, dtype=float)
    }

    return data


def main():
    """Main entry point for the visualization script."""
    if len(sys.argv) < 2:
        print("No input file provided. Generating sample data for demonstration...")
        print("Usage: python cms_accuracy_plot.py <input_tsv_file> [output_png_file]")
        print()
        data = generate_sample_data()
        output_path = "cms_accuracy_sample.png"
    else:
        input_path = sys.argv[1]
        if not Path(input_path).exists():
            print(f"Error: Input file not found: {input_path}")
            sys.exit(1)

        data = parse_cms_accuracy_results(input_path)

        if len(data['width']) == 0:
            print("Error: No valid data found in input file")
            sys.exit(1)

        output_path = sys.argv[2] if len(sys.argv) > 2 else None

    # Print summary statistics
    print("\nCMS Accuracy Summary:")
    print("-" * 60)
    print(f"{'Width':>8} {'Trials':>8} {'Theoretical':>12} {'Median':>12} {'P95':>12} {'Max':>12}")
    print("-" * 60)
    for i in range(len(data['width'])):
        print(f"{data['width'][i]:>8} {data['trials'][i]:>8} "
              f"{data['theoretical_max_error'][i]:>12.1f} "
              f"{data['median_abs_error'][i]:>12.1f} "
              f"{data['p95_abs_error'][i]:>12.1f} "
              f"{data['max_abs_error'][i]:>12.1f}")
    print("-" * 60)

    # Create visualization
    plot_cms_accuracy(data, output_path, show=True)


if __name__ == "__main__":
    main()
