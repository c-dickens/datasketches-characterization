#!/usr/bin/env python3
"""
Visualization script for Count-Min Sketch Failure Probability profile.

This script generates documentation-quality plots showing how CMS failure
probability (bound violations) decreases with increasing sketch depth.

Usage:
    ./characterization count-min-sketch-failure-probability > failure_prob_data.tsv
    python visualize_failure_probability.py failure_prob_data.tsv

Output:
    - cms_failure_probability.png: Multi-panel figure showing failure analysis
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import logging
import argparse
from scipy import stats

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')


def main():
    parser = argparse.ArgumentParser(description='Visualize CMS Failure Probability data')
    parser.add_argument('input_file', help='TSV file from count-min-sketch-failure-probability profile')
    parser.add_argument('--output', '-o', default='cms_failure_probability.png',
                        help='Output filename (default: cms_failure_probability.png)')
    args = parser.parse_args()

    logging.info(f"Loading data from {args.input_file}...")
    df = pd.read_csv(args.input_file, sep='\t', comment='#')
    logging.info(f"Loaded {len(df)} rows")

    # Get unique depths
    depths = sorted(df['Depth'].unique())
    logging.info(f"Found depths: {depths}")

    # Compute aggregate statistics per depth
    stats_per_depth = []
    for d in depths:
        df_d = df[df['Depth'] == d]
        total_violations = df_d['Violations'].sum()
        total_queries = df_d['NumQueries'].sum()
        observed_rate = total_violations / total_queries
        theoretical_delta = np.exp(-d)

        stats_per_depth.append({
            'depth': d,
            'total_violations': total_violations,
            'total_queries': total_queries,
            'observed_rate': observed_rate,
            'theoretical_delta': theoretical_delta,
            'num_trials': len(df_d)
        })

    stats_df = pd.DataFrame(stats_per_depth)

    # Create figure with subplots
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))

    # Color scheme for depths
    colors = plt.cm.coolwarm(np.linspace(0.2, 0.8, len(depths)))

    # =========================================================================
    # Panel 1: Observed vs Theoretical Failure Rate (top-left)
    # =========================================================================
    ax1 = axes[0, 0]

    x_pos = np.arange(len(depths))
    width_bar = 0.35

    bars1 = ax1.bar(x_pos - width_bar/2, stats_df['observed_rate'] * 100, width_bar,
                    label='Observed Violation Rate', color='steelblue', alpha=0.8)
    bars2 = ax1.bar(x_pos + width_bar/2, stats_df['theoretical_delta'] * 100, width_bar,
                    label='Theoretical δ = e^(-d)', color='orange', alpha=0.8)

    ax1.set_xticks(x_pos)
    ax1.set_xticklabels([f'd={d}' for d in depths])
    ax1.set_xlabel('Sketch Depth (number of hash functions)')
    ax1.set_ylabel('Failure Rate (%)')
    ax1.set_title('Observed vs Theoretical Failure Rate')
    ax1.legend(loc='upper right')
    ax1.grid(True, alpha=0.3, axis='y')

    # Add value labels on bars
    for bar, val in zip(bars1, stats_df['observed_rate'] * 100):
        ax1.annotate(f'{val:.3f}%', xy=(bar.get_x() + bar.get_width()/2, bar.get_height()),
                    ha='center', va='bottom', fontsize=8)
    for bar, val in zip(bars2, stats_df['theoretical_delta'] * 100):
        ax1.annotate(f'{val:.3f}%', xy=(bar.get_x() + bar.get_width()/2, bar.get_height()),
                    ha='center', va='bottom', fontsize=8)

    # =========================================================================
    # Panel 2: Log-scale Comparison (top-right)
    # =========================================================================
    ax2 = axes[0, 1]

    ax2.semilogy(depths, stats_df['observed_rate'], 'o-', markersize=10,
                 linewidth=2, label='Observed', color='steelblue')
    ax2.semilogy(depths, stats_df['theoretical_delta'], 's--', markersize=10,
                 linewidth=2, label='Theoretical δ = e^(-d)', color='orange')

    # Add e^(-d) annotation
    for d, delta in zip(depths, stats_df['theoretical_delta']):
        ax2.annotate(f'e^(-{d})={delta:.4f}', xy=(d, delta * 1.5),
                    ha='center', fontsize=9, color='orange')

    ax2.set_xlabel('Sketch Depth (d)')
    ax2.set_ylabel('Failure Rate (log scale)')
    ax2.set_title('Failure Rate Comparison (Log Scale)')
    ax2.legend(loc='upper right')
    ax2.grid(True, alpha=0.3, which='both')
    ax2.set_xticks(depths)

    # =========================================================================
    # Panel 3: Per-Trial Violation Distribution (bottom-left)
    # =========================================================================
    ax3 = axes[1, 0]

    # Box plot of per-trial violation rates
    box_data = []
    for d in depths:
        df_d = df[df['Depth'] == d]
        box_data.append(df_d['ViolationRate'].values * 100)

    bp = ax3.boxplot(box_data, labels=[f'd={d}' for d in depths],
                     showfliers=True, flierprops={'markersize': 2, 'alpha': 0.5})

    # Add theoretical lines
    for i, d in enumerate(depths):
        theoretical_delta = np.exp(-d) * 100
        ax3.hlines(theoretical_delta, i + 0.6, i + 1.4, colors='red',
                  linestyles='--', linewidth=2)

    ax3.set_xlabel('Sketch Depth')
    ax3.set_ylabel('Per-Trial Violation Rate (%)')
    ax3.set_title('Distribution of Per-Trial Violation Rates\n(red dashed = theoretical δ)')
    ax3.grid(True, alpha=0.3, axis='y')

    # =========================================================================
    # Panel 4: Observed/Theoretical Ratio (bottom-right)
    # =========================================================================
    ax4 = axes[1, 1]

    ratios = stats_df['observed_rate'] / stats_df['theoretical_delta']

    ax4.bar(x_pos, ratios, color=colors, alpha=0.8)
    ax4.axhline(y=1.0, color='red', linestyle='--', linewidth=2, label='Ratio = 1 (perfect match)')

    ax4.set_xticks(x_pos)
    ax4.set_xticklabels([f'd={d}' for d in depths])
    ax4.set_xlabel('Sketch Depth')
    ax4.set_ylabel('Observed / Theoretical Ratio')
    ax4.set_title('Observed vs Theoretical Failure Rate Ratio')
    ax4.legend(loc='upper right')
    ax4.grid(True, alpha=0.3, axis='y')

    # Add value labels
    for i, ratio in enumerate(ratios):
        ax4.annotate(f'{ratio:.3f}', xy=(i, ratio + 0.02),
                    ha='center', va='bottom', fontsize=10, fontweight='bold')

    # =========================================================================
    # Finalize and save
    # =========================================================================
    fig.suptitle('Count-Min Sketch: Failure Probability Analysis\n'
                 '(Comparing d=3,5,7 hash functions)',
                 fontsize=14, fontweight='bold')
    fig.tight_layout()

    logging.info(f"Saving figure to {args.output}...")
    fig.savefig(args.output, dpi=150, bbox_inches='tight')
    logging.info("Visualization complete!")

    # Print summary statistics
    print("\n" + "="*70)
    print("FAILURE PROBABILITY SUMMARY")
    print("="*70)
    print(f"\n{'Depth':<8} {'Theoretical δ':<18} {'Observed Rate':<18} {'Ratio':<10} {'Total Queries':<15}")
    print("-"*70)
    for _, row in stats_df.iterrows():
        d = int(row['depth'])
        theo = row['theoretical_delta']
        obs = row['observed_rate']
        ratio = obs / theo
        queries = int(row['total_queries'])
        print(f"d={d:<5} e^(-{d})={theo:<10.6f} {obs:<18.8f} {ratio:<10.4f} {queries:<15,}")

    print("\n" + "="*70)
    print("KEY INSIGHTS:")
    print("="*70)
    print("- Theoretical bound: Pr[estimate > true + εN] ≤ δ = e^(-d)")
    print("- Increasing depth reduces failure probability exponentially")
    print("- Observed rates should be ≤ theoretical δ (ratio ≤ 1)")
    if all(ratios <= 1.1):
        print("- ✓ All depths satisfy the theoretical bound (within tolerance)")
    else:
        print("- ⚠ Some depths may exceed theoretical bound (investigate)")


if __name__ == '__main__':
    main()
