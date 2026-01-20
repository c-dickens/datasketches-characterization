#!/usr/bin/env python3
"""
Visualization script for Count-Min Sketch Error vs Width profile.

This script generates documentation-quality plots showing how CMS error
decreases with increasing sketch width.

Usage:
    ./build/cpp/characterization count-min-sketch-error-vs-width > cpp/results/error_vs_width.csv
    python cpp/scripts/visualize_error_vs_width.py cpp/results/error_vs_width.csv

Output:
    - cms_error_vs_width.png: Multi-panel figure showing error analysis
"""

import pandas as pd
import matplotlib.pyplot as plt
from datasketches import kll_floats_sketch
import numpy as np
import logging
import sys
import argparse

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')


def compute_sketch_quantiles(values, quantile_levels):
    """Compute quantiles using DataSketches KLL for scalability."""
    sketch = kll_floats_sketch()
    for val in values:
        sketch.update(float(val))
    return sketch.get_quantiles(quantile_levels)


def main():
    parser = argparse.ArgumentParser(description='Visualize CMS Error vs Width data')
    parser.add_argument('input_file', help='TSV file from count-min-sketch-error-vs-width profile')
    parser.add_argument('--output', '-o', default='cms_error_vs_width.png',
                        help='Output filename (default: cms_error_vs_width.png)')
    args = parser.parse_args()

    logging.info(f"Loading data from {args.input_file}...")
    df = pd.read_csv(args.input_file, sep='\t', comment='#')
    logging.info(f"Loaded {len(df)} rows")

    # Get unique widths
    widths = sorted(df['LgWidth'].unique())
    logging.info(f"Found lg_widths: {widths}")

    # Gaussian quantile levels: 1σ (~68%), 2σ (~95%), 3σ (~99.7%)
    quantile_levels = [0.00135, 0.0228, 0.1587, 0.5, 0.8413, 0.9772, 0.99865]

    # Create figure with subplots
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))

    # Color scheme for widths
    colors = plt.cm.viridis(np.linspace(0.2, 0.8, len(widths)))

    # =========================================================================
    # Panel 1: True vs Estimated Frequency for each width (top-left)
    # =========================================================================
    ax1 = axes[0, 0]

    for i, lg_w in enumerate(widths):
        df_w = df[df['LgWidth'] == lg_w]
        width = 2 ** lg_w
        epsilon = np.exp(1.0) / width

        # Sample points for scatter (to avoid overplotting)
        sample_idx = np.random.choice(len(df_w), min(5000, len(df_w)), replace=False)
        df_sample = df_w.iloc[sample_idx]

        ax1.scatter(df_sample['TrueFreq'], df_sample['Estimate'],
                   alpha=0.1, s=2, c=[colors[i]], label=f'w=2^{lg_w}', rasterized=True)

    # Perfect estimate line
    max_freq = df['TrueFreq'].max()
    ax1.plot([1, max_freq], [1, max_freq], 'k:', label='y = x (perfect)', linewidth=1.5)

    ax1.set_xscale('log', base=2)
    ax1.set_yscale('log', base=2)
    ax1.set_xlabel('True Frequency')
    ax1.set_ylabel('Estimated Frequency')
    ax1.set_title('True vs Estimated Frequency')
    ax1.legend(loc='lower right', fontsize=8)
    ax1.grid(True, alpha=0.3)

    # =========================================================================
    # Panel 2: Relative Error vs True Frequency for each width (top-right)
    # =========================================================================
    ax2 = axes[0, 1]

    for i, lg_w in enumerate(widths):
        df_w = df[df['LgWidth'] == lg_w]
        width = 2 ** lg_w
        epsilon = np.exp(1.0) / width
        N = df_w['TotalWeight'].iloc[0]

        # Compute quantiles at each true frequency
        grouped = df_w.groupby('TrueFreq')['RelError']

        # Get median and quantiles
        medians = grouped.median()
        q_high = grouped.quantile(0.95)

        ax2.plot(medians.index, medians.values, '-', color=colors[i],
                label=f'w=2^{lg_w} (median)', linewidth=2)
        ax2.plot(q_high.index, q_high.values, '--', color=colors[i],
                alpha=0.5, linewidth=1)

        # Theoretical bound: epsilon * N / true_freq
        freqs = np.array(sorted(medians.index))
        theoretical_rel_error = (epsilon * N) / freqs
        ax2.plot(freqs, theoretical_rel_error, ':', color=colors[i],
                alpha=0.7, linewidth=1.5)

    ax2.set_xscale('log', base=2)
    ax2.set_ylim(0, 2)
    ax2.set_xlabel('True Frequency')
    ax2.set_ylabel('Relative Error (est - true) / true')
    ax2.set_title('Relative Error vs True Frequency\n(solid=median, dashed=95th pctl, dotted=theoretical)')
    ax2.legend(loc='upper right', fontsize=8)
    ax2.grid(True, alpha=0.3)

    # =========================================================================
    # Panel 3: Absolute Error Distribution by Width (bottom-left)
    # =========================================================================
    ax3 = axes[1, 0]

    # Box plot of absolute error for each width
    box_data = []
    box_labels = []
    theoretical_bounds = []

    for lg_w in widths:
        df_w = df[df['LgWidth'] == lg_w]
        width = 2 ** lg_w
        epsilon = np.exp(1.0) / width
        N = df_w['TotalWeight'].iloc[0]

        # Sample to avoid memory issues
        sample_size = min(10000, len(df_w))
        sample_idx = np.random.choice(len(df_w), sample_size, replace=False)
        box_data.append(df_w.iloc[sample_idx]['AbsError'].values)
        box_labels.append(f'2^{lg_w}')
        theoretical_bounds.append(epsilon * N)

    bp = ax3.boxplot(box_data, labels=box_labels, showfliers=False)

    # Add theoretical bounds as horizontal lines
    for i, bound in enumerate(theoretical_bounds):
        ax3.hlines(bound, i + 0.6, i + 1.4, colors='red', linestyles='--', linewidth=2)

    ax3.set_xlabel('Sketch Width')
    ax3.set_ylabel('Absolute Error (estimate - true)')
    ax3.set_title('Absolute Error Distribution by Width\n(red dashed = theoretical bound εN)')
    ax3.grid(True, alpha=0.3, axis='y')

    # =========================================================================
    # Panel 4: Violation Rate and Epsilon vs Width (bottom-right)
    # =========================================================================
    ax4 = axes[1, 1]

    # Compute violation rates for each width
    violation_rates = []
    epsilons_empirical = []
    epsilons_theoretical = []

    for lg_w in widths:
        df_w = df[df['LgWidth'] == lg_w]
        width = 2 ** lg_w

        # Violation rate
        violations = (df_w['WithinBound'] == 0).sum()
        total = len(df_w)
        violation_rates.append(violations / total * 100)

        # Empirical epsilon (mean overestimate / N)
        N = df_w['TotalWeight'].iloc[0]
        mean_overest = df_w['AbsError'].mean()
        epsilons_empirical.append(mean_overest / N)

        # Theoretical epsilon
        epsilons_theoretical.append(np.exp(1.0) / width)

    x_positions = np.arange(len(widths))

    # Twin axis for violation rate
    ax4_twin = ax4.twinx()

    # Plot epsilon values
    ax4.bar(x_positions - 0.2, epsilons_empirical, 0.35, label='Empirical ε (avg error/N)',
           color='steelblue', alpha=0.8)
    ax4.bar(x_positions + 0.2, epsilons_theoretical, 0.35, label='Theoretical ε (e/w)',
           color='orange', alpha=0.8)

    # Plot violation rate
    ax4_twin.plot(x_positions, violation_rates, 'go-', linewidth=2, markersize=8,
                  label='Bound Violation Rate')

    ax4.set_xticks(x_positions)
    ax4.set_xticklabels([f'2^{lg_w}' for lg_w in widths])
    ax4.set_xlabel('Sketch Width')
    ax4.set_ylabel('Epsilon (ε)')
    ax4_twin.set_ylabel('Violation Rate (%)', color='green')
    ax4_twin.tick_params(axis='y', labelcolor='green')

    ax4.set_title('Error Parameter (ε) and Bound Violations by Width')
    ax4.legend(loc='upper right', fontsize=8)
    ax4_twin.legend(loc='center right', fontsize=8)
    ax4.grid(True, alpha=0.3)

    # =========================================================================
    # Finalize and save
    # =========================================================================
    fig.suptitle('Count-Min Sketch: Error vs Sketch Width Analysis', fontsize=14, fontweight='bold')
    fig.tight_layout()

    logging.info(f"Saving figure to {args.output}...")
    fig.savefig(args.output, dpi=150, bbox_inches='tight')
    logging.info("Visualization complete!")

    # Print summary statistics
    print("\n" + "="*60)
    print("SUMMARY STATISTICS")
    print("="*60)
    for i, lg_w in enumerate(widths):
        width = 2 ** lg_w
        print(f"\nWidth = 2^{lg_w} = {width}:")
        print(f"  Theoretical ε = {epsilons_theoretical[i]:.6f}")
        print(f"  Empirical ε   = {epsilons_empirical[i]:.6f}")
        print(f"  Violation rate = {violation_rates[i]:.4f}%")


if __name__ == '__main__':
    main()
