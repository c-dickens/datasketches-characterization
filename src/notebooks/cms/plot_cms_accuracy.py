#!/usr/bin/env python3
# /// script
# requires-python = ">=3.9"
# dependencies = [
#     "matplotlib",
#     "numpy",
#     "pandas",
# ]
# ///
#
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
Plot CMS accuracy profile results with percentile fan charts.

Reads TSV files from cpp/results/ and produces:
  1. cms_point_query_error.svg — percentile fan chart of absolute error
     vs stream length with the theoretical eps * total_weight bound overlaid
  2. cms_bound_violation_rate.svg — empirical violation rate vs stream length

Usage:
  python plot_cms_accuracy.py [--results-dir PATH] [--output-dir PATH]
"""

import argparse
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


RESULTS_DIR = os.path.join(
    os.path.dirname(__file__), "..", "..", "..", "cpp", "results"
)
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "svg")

# Quantile column names matching the C++ output
QUANTILE_COLS = [
    "p0.00135", "p0.02275", "p0.15866", "p0.50",
    "p0.84134", "p0.97725", "p0.99865",
]
# Labels for the bands
BAND_LABELS = [
    r"$\pm 3\sigma$",
    r"$\pm 2\sigma$",
    r"$\pm 1\sigma$",
]


def load_tsv(path):
    """Load a TSV file produced by count_min_sketch_accuracy_profile."""
    return pd.read_csv(path, sep="\t")


def parse_log_params(log_path):
    """Parse sketch parameters from the .log file next to a .tsv file."""
    params = {}
    if not os.path.exists(log_path):
        return params
    with open(log_path) as f:
        for line in f:
            if "num_hashes (depth)" in line:
                params["depth"] = int(line.strip().split("=")[-1].strip())
            elif "num_buckets (width)" in line:
                params["width"] = int(line.strip().split("=")[-1].strip())
            elif "epsilon = e/width" in line:
                params["epsilon"] = float(
                    line.strip().split("=")[-1].strip())
    if "depth" in params:
        params["delta"] = np.exp(-params["depth"])
    return params


def find_accuracy_files(results_dir):
    """Find CMS accuracy TSV files matching the eps/zipf naming convention."""
    files = []
    for f in sorted(os.listdir(results_dir)):
        if (f.startswith("count_min_sketch_accuracy_eps")
                and f.endswith(".tsv")):
            files.append(os.path.join(results_dir, f))
    return files


def label_from_filename(path):
    """Extract a human-readable label from the filename and log params."""
    base = os.path.basename(path).replace("count_min_sketch_accuracy_", "")
    base = base.replace(".tsv", "")
    parts = base.split("_")
    label_parts = []
    for p in parts:
        if p.startswith("eps"):
            val = p[3:]
            label_parts.append(f"eps=0.{val}")
        elif p.startswith("zipf"):
            val = p[4:]
            if len(val) == 2:
                label_parts.append(f"zipf={val[0]}.{val[1]}")
            else:
                label_parts.append(f"zipf={val}")

    log_path = path.replace(".tsv", ".log")
    params = parse_log_params(log_path)
    if "delta" in params:
        label_parts.append(
            f"delta={params['delta']:.4f} (depth={params['depth']})")

    return ", ".join(label_parts) if label_parts else base


def plot_point_query_error(dataframes, labels, output_path):
    """
    Plot percentile fan chart of absolute error vs stream length,
    with theoretical eps * total_weight bound overlaid.

    Bands:
      lightest: p0.00135 to p0.99865 (-3σ to +3σ)
      medium:   p0.02275 to p0.97725 (-2σ to +2σ)
      darkest:  p0.15866 to p0.84134 (-1σ to +1σ)
      line:     p0.50 (median)
      dashed:   EpsTotalWeight (theoretical bound)
    """
    fig, ax = plt.subplots(figsize=(10, 6))

    base_colors = plt.cm.tab10.colors

    for i, (df, label) in enumerate(zip(dataframes, labels)):
        color = base_colors[i % len(base_colors)]
        sl = df["StreamLen"].values

        # Band pairs: (lower, upper, alpha)
        bands = [
            ("p0.00135", "p0.99865", 0.15),  # -3σ to +3σ
            ("p0.02275", "p0.97725", 0.25),  # -2σ to +2σ
            ("p0.15866", "p0.84134", 0.35),  # -1σ to +1σ
        ]

        for (lo_col, hi_col, alpha), blabel in zip(bands, BAND_LABELS):
            band_label = f"{blabel} ({label})" if i == 0 or len(dataframes) > 1 else blabel
            ax.fill_between(
                sl, df[lo_col].values, df[hi_col].values,
                color=color, alpha=alpha, label=band_label,
            )

        # Median line
        ax.plot(sl, df["p0.50"].values, "-", color=color, linewidth=1.5,
                label=f"Median ({label})")

        # Theoretical bound
        ax.plot(sl, df["EpsTotalWeight"].values, "--", color=color,
                linewidth=2, alpha=0.7,
                label=r"$\epsilon \cdot N$ bound" + f" ({label})")

    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.set_xlabel("Stream Length", fontsize=12)
    ax.set_ylabel("Absolute Error", fontsize=12)
    ax.set_title("CMS Point Query Error Distribution vs Stream Length",
                 fontsize=14)
    ax.legend(fontsize=8, loc="upper left")
    ax.grid(True, alpha=0.3)

    fig.tight_layout()
    fig.savefig(output_path, format="svg", bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {output_path}")


def plot_bound_violation_rate(dataframes, labels, output_path):
    """Plot empirical bound violation rate vs stream length."""
    fig, ax = plt.subplots(figsize=(10, 6))

    colors = plt.cm.tab10.colors

    for i, (df, label) in enumerate(zip(dataframes, labels)):
        color = colors[i % len(colors)]
        sl = df["StreamLen"]
        vr = df["BoundViolationRate"]

        ax.plot(sl, vr, "-o", color=color, markersize=3, label=f"{label}")

    ax.set_xscale("log", base=2)
    ax.set_xlabel("Stream Length", fontsize=12)
    ax.set_ylabel("Bound Violation Rate", fontsize=12)
    ax.set_title("CMS Bound Violation Rate vs Stream Length", fontsize=14)
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3)

    ax.axhline(y=0, color="green", linestyle="--", alpha=0.3,
               label="Expected (0)")

    fig.tight_layout()
    fig.savefig(output_path, format="svg", bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {output_path}")


def print_commentary(dataframes, labels):
    """Print analysis commentary about the results."""
    print("\n" + "=" * 60)
    print("CMS ACCURACY PROFILE — COMMENTARY")
    print("=" * 60)

    for df, label in zip(dataframes, labels):
        print(f"\n--- {label} ---")
        max_vr = df["BoundViolationRate"].max()
        mean_vr = df["BoundViolationRate"].mean()
        print(f"  Bound violation rate: max={max_vr:.6f}, "
              f"mean={mean_vr:.6f}")

        if max_vr == 0:
            print("  -> Theoretical bounds hold perfectly across all "
                  "stream lengths.")
        elif max_vr < 0.01:
            print("  -> Theoretical bounds hold well "
                  "(< 1% violation rate).")
        else:
            print(f"  -> WARNING: violation rate up to {max_vr:.2%}")

        # Compare +3σ quantile to theoretical bound at largest stream
        last_row = df.iloc[-1]
        ratio = last_row["p0.99865"] / last_row["EpsTotalWeight"]
        print(f"  +3sigma/eps*N ratio at largest stream: {ratio:.4f}")
        if ratio <= 1.0:
            print("  -> +3sigma error stays within theoretical bound.")
        else:
            print(f"  -> +3sigma error exceeds theoretical bound "
                  f"by {ratio:.2f}x")

        # Median error summary
        print(f"  Median absolute error range: "
              f"{df['p0.50'].min():.4f} to {df['p0.50'].max():.4f}")
        print(f"  +3sigma error range: "
              f"{df['p0.99865'].min():.4f} to "
              f"{df['p0.99865'].max():.4f}")

    print("\n" + "=" * 60)


def main():
    parser = argparse.ArgumentParser(
        description="Plot CMS accuracy profile results"
    )
    parser.add_argument(
        "--results-dir", default=RESULTS_DIR,
        help="Directory containing TSV result files"
    )
    parser.add_argument(
        "--output-dir", default=OUTPUT_DIR,
        help="Directory for SVG output files"
    )
    args = parser.parse_args()

    results_dir = os.path.abspath(args.results_dir)
    output_dir = os.path.abspath(args.output_dir)
    os.makedirs(output_dir, exist_ok=True)

    files = find_accuracy_files(results_dir)
    if not files:
        print(f"No CMS accuracy TSV files found in {results_dir}")
        sys.exit(1)

    print(f"Found {len(files)} result file(s):")
    for f in files:
        print(f"  {os.path.basename(f)}")

    dataframes = []
    labels = []
    for f in files:
        df = load_tsv(f)
        dataframes.append(df)
        labels.append(label_from_filename(f))

    plot_point_query_error(
        dataframes, labels,
        os.path.join(output_dir, "cms_point_query_error.svg")
    )

    plot_bound_violation_rate(
        dataframes, labels,
        os.path.join(output_dir, "cms_bound_violation_rate.svg")
    )

    print_commentary(dataframes, labels)


if __name__ == "__main__":
    main()
