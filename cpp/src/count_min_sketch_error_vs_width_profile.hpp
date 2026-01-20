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

#ifndef COUNT_MIN_SKETCH_ERROR_VS_WIDTH_PROFILE_HPP_
#define COUNT_MIN_SKETCH_ERROR_VS_WIDTH_PROFILE_HPP_

#include "job_profile.hpp"

namespace datasketches {

/**
 * Error vs Sketch Width profile for Count-Min Sketch.
 *
 * Compares error performance across different sketch widths (lg_width = 8, 10, 12, 14)
 * using the same input stream to enable fair comparison.
 *
 * Key CMS theoretical bounds demonstrated:
 *   - epsilon = e / width (error parameter)
 *   - Guarantee: estimate <= true_freq + epsilon * N with probability >= 1 - delta
 *
 * This profile surfaces the relationship between:
 *   - Sketch width and accuracy (wider = more accurate)
 *   - Stream length and absolute error (longer stream = larger absolute error bound)
 *   - True frequency and relative error (low-frequency items have higher relative error)
 *
 * Output: TSV with per-item data for each width configuration, enabling
 * visualization of error distributions and comparison to theoretical bounds.
 */
class count_min_sketch_error_vs_width_profile: public job_profile {
public:
  void run() override;
};

}

#endif
