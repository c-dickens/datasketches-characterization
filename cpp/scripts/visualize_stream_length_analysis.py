#!/usr/bin/env python3
"""
Visualization script for Count-Min Sketch Stream Length Analysis profile.

This script generates documentation-quality plots showing how increasing
stream length affects CMS accuracy, particularly for low-frequency items.

Usage:
    ./characterization count-min-sketch-stream-length-analysis > stream_length_data.tsv
    python visualize_stream_length_analysis.py stream_length_data.tsv

Output:
    - cms_stream_length_analysis.png: Multi-panel figure showing the failure mode
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import logging
import argparse

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')


def main():
    parser = argparse.ArgumentParser(description='Visualize CMS Stream Length Analysis data')
    parser.add_argument('input_file', help='TSV file from count-min-sketch-stream-length-analysis profile')
    parser.add_argument('--output', '-o', default='cms_stream_length_analysis.png',
                        help='Output filename (default: cms_stream_length_analysis.png)')
    args = parser.parse_args()

    logging.info(f"Loading data from {args.input_file}...")
    df = pd.read_csv(args.input_file, sep='\t', comment='#')
    logging.info(f"Loaded {len(df)} rows")

    # Create figure with subplots
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))

    # =========================================================================
    # Panel 1: Error Bound vs Stream Length (top-left)
    # =========================================================================
    ax1 = axes[0, 0]

    ax1.loglog(df['StreamLen'], df['ErrorBound'], 'o-', linewidth=2, markersize=8,
               color='steelblue', label='Error Bound (εN)')
    ax1.loglog(df['StreamLen'], df['AvgAbsError'], 's--', linewidth=2, markersize=8,
               color='orange', label='Avg Absolute Error')
    ax1.loglog(df['StreamLen'], df['MaxAbsError'], '^:', linewidth=2, markersize=8,
               color='red', alpha=0.7, label='Max Absolute Error')

    ax1.set_xlabel('Stream Length (N)')
    ax1.set_ylabel('Error')
    ax1.set_title('Error Bound Growth with Stream Length\n(ε = e/w is constant, εN grows linearly)')
    ax1.legend(loc='upper left')
    ax1.grid(True, alpha=0.3, which='both')

    # =========================================================================
    # Panel 2: Relative Error: Low vs High Frequency Items (top-right)
    # =========================================================================
    ax2 = axes[0, 1]

    ax2.semilogy(df['LgStreamLen'], df['AvgRelErrorLowFreq'], 'o-', linewidth=2, markersize=8,
                 color='red', label='Low-Freq Items (freq ≤ εN)')
    ax2.semilogy(df['LgStreamLen'], df['AvgRelErrorHighFreq'], 's-', linewidth=2, markersize=8,
                 color='green', label='High-Freq Items (freq > εN)')

    ax2.set_xlabel('lg(Stream Length)')
    ax2.set_ylabel('Average Relative Error')
    ax2.set_title('Relative Error by Item Frequency Class\n(Low-freq items have unbounded relative error)')
    ax2.legend(loc='upper right')
    ax2.grid(True, alpha=0.3, which='both')

    # Add annotation
    ax2.annotate('Low-frequency items\nhave unusable estimates',
                xy=(df['LgStreamLen'].iloc[4], df['AvgRelErrorLowFreq'].iloc[4]),
                xytext=(df['LgStreamLen'].iloc[2], df['AvgRelErrorLowFreq'].iloc[4] * 5),
                arrowprops=dict(arrowstyle='->', color='red', alpha=0.7),
                fontsize=9, color='red')

    # =========================================================================
    # Panel 3: Fraction of "Useful" Items (bottom-left)
    # =========================================================================
    ax3 = axes[1, 0]

    ax3.bar(df['LgStreamLen'], df['FracAboveThreshold'] * 100, color='steelblue', alpha=0.8)

    ax3.set_xlabel('lg(Stream Length)')
    ax3.set_ylabel('% Items with Freq > εN')
    ax3.set_title('Fraction of Items with "Useful" Estimates\n(Only heavy hitters have meaningful accuracy)')
    ax3.grid(True, alpha=0.3, axis='y')

    # Add value labels
    for i, (lg_n, frac) in enumerate(zip(df['LgStreamLen'], df['FracAboveThreshold'])):
        ax3.annotate(f'{frac*100:.1f}%', xy=(lg_n, frac * 100 + 1),
                    ha='center', va='bottom', fontsize=9)

    # =========================================================================
    # Panel 4: Summary Statistics (bottom-right)
    # =========================================================================
    ax4 = axes[1, 1]

    # Create dual axis
    ax4_twin = ax4.twinx()

    # Plot error bound and avg freq
    ln1 = ax4.plot(df['LgStreamLen'], df['ErrorBound'], 'o-', linewidth=2, markersize=8,
                   color='steelblue', label='Error Bound (εN)')
    ln2 = ax4.plot(df['LgStreamLen'], df['AvgFreq'], 's--', linewidth=2, markersize=8,
                   color='orange', label='Avg Item Frequency')
    ln3 = ax4.plot(df['LgStreamLen'], df['MinFreq'], '^:', linewidth=2, markersize=8,
                   color='red', alpha=0.7, label='Min Item Frequency')

    ax4.set_xlabel('lg(Stream Length)')
    ax4.set_ylabel('Count')
    ax4.set_yscale('log')

    # Plot num distinct on twin axis
    ln4 = ax4_twin.plot(df['LgStreamLen'], df['NumDistinct'], 'D-', linewidth=2, markersize=8,
                        color='green', label='Num Distinct Items')
    ax4_twin.set_ylabel('Number of Distinct Items', color='green')
    ax4_twin.tick_params(axis='y', labelcolor='green')

    # Combined legend
    lns = ln1 + ln2 + ln3 + ln4
    labs = [l.get_label() for l in lns]
    ax4.legend(lns, labs, loc='upper left', fontsize=8)

    ax4.set_title('Error Bound vs Frequency Statistics')
    ax4.grid(True, alpha=0.3, which='both')

    # =========================================================================
    # Finalize and save
    # =========================================================================
    fig.suptitle('Count-Min Sketch: Stream Length Failure Mode Analysis\n'
                 '(Fixed width=1024, depth=5)',
                 fontsize=14, fontweight='bold')
    fig.tight_layout()

    logging.info(f"Saving figure to {args.output}...")
    fig.savefig(args.output, dpi=150, bbox_inches='tight')
    logging.info("Visualization complete!")

    # Print summary statistics
    print("\n" + "="*80)
    print("STREAM LENGTH FAILURE MODE ANALYSIS")
    print("="*80)
    print("\nKEY INSIGHT: The CMS error bound is additive: estimate ≤ true + εN")
    print("As stream length N increases, the absolute error bound εN grows linearly.")
    print("This means low-frequency items have increasingly unreliable estimates.\n")

    print(f"{'lg(N)':<8} {'N':<12} {'εN':<12} {'Useful Items':<15} {'Rel Err (low)':<15} {'Rel Err (high)':<15}")
    print("-"*80)

    for _, row in df.iterrows():
        print(f"{int(row['LgStreamLen']):<8} {int(row['StreamLen']):<12,} "
              f"{row['ErrorBound']:<12,.1f} {row['FracAboveThreshold']*100:>6.1f}%         "
              f"{row['AvgRelErrorLowFreq']:<15.4f} {row['AvgRelErrorHighFreq']:<15.4f}")

    print("\n" + "="*80)
    print("PRACTICAL RECOMMENDATION:")
    print("="*80)
    print("For accurate low-frequency item estimates, either:")
    print("  1. Increase sketch width (decrease ε)")
    print("  2. Use a different data structure (e.g., Frequent Items sketch)")
    print("  3. Accept that CMS is primarily useful for heavy hitter detection")


if __name__ == '__main__':
    main()
