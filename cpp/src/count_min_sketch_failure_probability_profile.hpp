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

#ifndef COUNT_MIN_SKETCH_FAILURE_PROBABILITY_PROFILE_HPP_
#define COUNT_MIN_SKETCH_FAILURE_PROBABILITY_PROFILE_HPP_

#include "job_profile.hpp"

namespace datasketches {

/**
 * Failure Probability profile for Count-Min Sketch.
 *
 * Evaluates how often the theoretical error bound is violated for different
 * numbers of hash functions (depth = 3, 5, 7).
 *
 * Key CMS theoretical bounds:
 *   - epsilon = e / width (error parameter)
 *   - delta = e^(-depth) (failure probability)
 *   - Guarantee: Pr[estimate <= true_freq + epsilon * N] >= 1 - delta
 *
 * Expected theoretical failure rates:
 *   - depth=3: delta = e^(-3) ≈ 4.98%
 *   - depth=5: delta = e^(-5) ≈ 0.67%
 *   - depth=7: delta = e^(-7) ≈ 0.09%
 *
 * Output: Aggregate statistics showing bound violation rates compared to
 * theoretical predictions for each depth configuration.
 */
class count_min_sketch_failure_probability_profile: public job_profile {
public:
  void run() override;
};

}

#endif
