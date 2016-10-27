/*ckwg +29
 * Copyright 2016 by Kitware, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither name of Kitware, Inc. nor the names of any contributors may be used
 *    to endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef KWIVER_ARROWS_CLASS_PROBABLITY_FILTER_H_
#define KWIVER_ARROWS_CLASS_PROBABLITY_FILTER_H_

#include <vital/vital_config.h>
#include <arrows/core/kwiver_algo_export.h>

#include <vital/algo/algorithm.h>
#include <vital/algo/detected_object_filter.h>
#include <vital/types/image_container.h>

#include <opencv2/core/core.hpp>

#include <utility>
#include <set>

namespace kwiver {
namespace arrows {
namespace core {

// ----------------------------------------------------------------
/**
 * @brief Filters detections based on class probability.
 *
 * This algorithm filters out items that are less than the threshold.
 */

class KWIVER_ALGO_EXPORT class_probablity_filter
  : public vital::algorithm_impl<class_probablity_filter, vital::algo::detected_object_filter>
{
public:

  class_probablity_filter();

  virtual ~class_probablity_filter() { }

  virtual std::string impl_name() const { return "class_probablity_filter"; }

  virtual vital::config_block_sptr get_configuration() const;

  virtual void set_configuration(vital::config_block_sptr config);
  virtual bool check_configuration(vital::config_block_sptr config) const;

  virtual vital::detected_object_set_sptr filter( const vital::detected_object_set_sptr input_set) const;

private:
  bool m_keep_all_classes;
  std::set<std::string> m_keep_classes;
  double m_threshold;
};

}}} //End namespace


#endif // KWIVER_ARROWS_CLASS_PROBABLITY_FILTER_H_