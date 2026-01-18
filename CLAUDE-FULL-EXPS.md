# Reproducing CMS Experiments on Apple M4 MacBook

This guide provides step-by-step instructions to reproduce the Count-Min Sketch characterization experiments.

## 1. Install prerequisites

```bash
# Install Xcode command line tools (if not already)
xcode-select --install

# Install Homebrew (if not already)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install build tools
brew install cmake libomp
```

## 2. Clone and install DataSketches C++

```bash
cd ~/projects  # or wherever you keep code
git clone https://github.com/apache/datasketches-cpp.git
cd datasketches-cpp
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/local/datasketches -DBUILD_TESTS=OFF
make -j$(sysctl -n hw.ncpu)
make install
```

## 3. Clone and build characterization

```bash
cd ~/projects
git clone https://github.com/c-dickens/datasketches-characterization.git
cd datasketches-characterization
git checkout claude/review-cms-profiles-35rvu  # the branch with CMS profiles

cd cpp
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=$HOME/local/datasketches
make -j$(sysctl -n hw.ncpu)
```

## 4. Run the experiments

```bash
cd ~/projects/datasketches-characterization/cpp
mkdir -p results

# Quick (~30 sec)
./build/characterization count-min-sketch-stream-length-analysis > results/stream_length_analysis.csv

# Medium (~5 min)
./build/characterization count-min-sketch-failure-probability > results/failure_probability.csv

# Long (~15 min, 1.7GB output)
./build/characterization count-min-sketch-error-vs-width > results/error_vs_width.csv
```

## 5. Generate visualizations

```bash
# Create Python environment
python3 -m venv venv
source venv/bin/activate
pip install pandas matplotlib numpy scipy datasketches

# Generate plots
python scripts/visualize_stream_length_analysis.py results/stream_length_analysis.csv
python scripts/visualize_failure_probability.py results/failure_probability.csv
python scripts/visualize_error_vs_width.py results/error_vs_width.csv
```

## Output files

After completion you'll have:

```
results/
├── stream_length_analysis.csv      # 2KB
├── failure_probability.csv         # 674KB
└── error_vs_width.csv              # 1.7GB

cms_stream_length_analysis.png
cms_failure_probability.png
cms_error_vs_width.png
```

## Available profiles

The following CMS characterization profiles are available:

| Profile | Description | Runtime |
|---------|-------------|---------|
| `count-min-sketch-stream-length-analysis` | Stream length failure mode analysis | ~30 sec |
| `count-min-sketch-failure-probability` | Depth vs failure rate (d=3,5,7) | ~5 min |
| `count-min-sketch-error-vs-width` | Width vs error (lg_w=8,10,12,14) | ~15 min |
| `count-min-sketch-accuracy` | Accuracy vs stream length | ~1 min |
| `count-min-sketch-error-vs-freq` | Per-item error analysis (single trial) | ~5 sec |
| `count-min-sketch-error-distribution` | Multi-trial raw error distribution | ~10 min |

## Tuning parameters

To adjust experiment parameters, edit the corresponding profile source file in `cpp/src/`:

- `num_trials` - more trials = smoother results, longer runtime
- `stream_length` - test different input sizes
- `lg_widths` / `depths` - test different sketch configurations
- `zipf_exponent` - change data skewness (1.1 is typical)

Then rebuild with `make` in the build directory.
