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

#ifndef CMS_ACCURACY_PROFILE_HPP_
#define CMS_ACCURACY_PROFILE_HPP_

#include "job_profile.hpp"

namespace datasketches {

/**
 * Count-Min Sketch accuracy characterization profile.
 *
 * This profile measures the accuracy of Count-Min Sketch frequency estimates
 * across different sketch widths while maintaining a constant load factor
 * (distinct_items / width) to ensure meaningful collision behavior.
 *
 * Key metrics:
 * - Absolute error: estimate - true_frequency
 * - Relative error: (estimate - true_frequency) / true_frequency
 * - Comparison against theoretical bound: epsilon * N where epsilon = e/width
 *
 * The profile uses Zipfian distribution for realistic frequency patterns.
 */
class cms_accuracy_profile: public job_profile {
public:
  void run();
};

}

#endif
