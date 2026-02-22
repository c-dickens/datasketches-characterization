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

#ifndef COUNT_MIN_SKETCH_ACCURACY_PROFILE_HPP_
#define COUNT_MIN_SKETCH_ACCURACY_PROFILE_HPP_

#include "job_profile.hpp"

namespace datasketches {

/**
 * Accuracy profile for Count-Min Sketch point queries.
 *
 * Measures point query error against brute-force exact counts.
 * For each trial: generate a Zipf stream, insert into sketch,
 * then for every distinct item compute |estimate - true_count|.
 *
 * Per-item absolute errors from all trials at each stream length
 * are accumulated in a KLL sketch to produce the full error
 * distribution. Quantiles are reported at standard normal
 * fractions: -3σ, -2σ, -1σ, median, +1σ, +2σ, +3σ.
 *
 * Output columns (TSV):
 *   StreamLen | Trials | NumDistinct | EpsTotalWeight |
 *   p0.00135 | p0.02275 | p0.15866 | p0.50 |
 *   p0.84134 | p0.97725 | p0.99865 | BoundViolationRate
 *
 * Parameters: eps=0.01, delta=0.05, zipf_exponent=1.1
 */
class count_min_sketch_accuracy_profile: public job_profile {
public:
  void run() override;
};

}

#endif
