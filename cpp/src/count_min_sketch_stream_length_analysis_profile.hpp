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

#ifndef COUNT_MIN_SKETCH_STREAM_LENGTH_ANALYSIS_PROFILE_HPP_
#define COUNT_MIN_SKETCH_STREAM_LENGTH_ANALYSIS_PROFILE_HPP_

#include "job_profile.hpp"

namespace datasketches {

/**
 * Stream Length Analysis profile for Count-Min Sketch.
 *
 * Investigates the failure mode where increasing stream length causes
 * the absolute error bound (εN) to grow, making low-frequency item
 * estimates less useful.
 *
 * Key CMS insight demonstrated:
 *   The guarantee estimate <= true_freq + ε*N means:
 *   - Absolute error bound grows linearly with stream length N
 *   - For low-frequency items, relative error can be arbitrarily large
 *   - The sketch provides useful estimates primarily for heavy hitters
 *
 * This profile varies stream length (2^10 to 2^22) while keeping sketch
 * parameters fixed to show:
 *   1. How error bound εN increases with stream length
 *   2. The impact on low vs high frequency items
 *   3. When CMS estimates become meaningful (freq > εN)
 */
class count_min_sketch_stream_length_analysis_profile: public job_profile {
public:
  void run() override;
};

}

#endif
