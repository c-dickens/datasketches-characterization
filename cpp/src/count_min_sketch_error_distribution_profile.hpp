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

#ifndef COUNT_MIN_SKETCH_ERROR_DISTRIBUTION_PROFILE_HPP_
#define COUNT_MIN_SKETCH_ERROR_DISTRIBUTION_PROFILE_HPP_

#include "job_profile.hpp"

namespace datasketches {

/**
 * Multi-trial raw error distribution profile for Count-Min Sketch.
 *
 * Runs many independent trials and outputs raw per-item data:
 * - Trial number
 * - True frequency
 * - Estimated frequency
 * - Absolute error
 * - Relative error
 *
 * This enables post-hoc analysis to:
 * - Compute quantiles at each frequency
 * - Generate pitchfork plots
 * - Apply any bucketing strategy
 *
 * Key CMS property demonstrated:
 *   true_freq <= estimate <= true_freq + epsilon * ||f||_1
 */
class count_min_sketch_error_distribution_profile: public job_profile {
public:
  void run() override;
};

}

#endif
