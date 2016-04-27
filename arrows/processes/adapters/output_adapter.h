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

#ifndef KWIVER_NPUT_ADAPTER_H
#define KWIVER_NPUT_ADAPTER_H

#include <kwiver_adapter_export.h>

#include "adapter_base.h"
#include "adapter_data_set.h"


namespace kwiver {

class output_adapter_process;

// -----------------------------------------------------------------
/**
 *
 *
 */
class KWIVER_ADAPTER_EXPORT output_adapter
{
public:
  output_adapter();
  virtual ~output_adapter();

  /**
   * @brief Connect to named process.
   *
   * The named process is located in the specified pipeline.
   *
   * @param proc Process name.
   * @param pipe Pipeline to search.
   *
   * @return Pointer to adapter base object
   *
   * @throws sprokit::no_such_port_exception if the process is not found
   */
  void connect( sprokit::process::name_t proc, sprokit::pipeline_t pipe );

  /**
   * @brief Return list of ports connected to adapter process.
   *
   * This method returns the list of output ports that are connected
   * to the adapter process.
   *
   * @return List of port names
   */
  sprokit::process::ports_t port_list() const;

  /**
   * @brief Return list of active ports.
   *
   * This method returns the list of currently active ports and
   * associated port info items.
   *
   * @return List of port names and info.
   */
  adapter::ports_info_t get_ports() const;

  /**
   * @brief Send data set to output adapter process.
   *
   * The specified data set is sent to the output adapter process that
   * is currently connected to this object.
   *
   * @returns Data set
   */
  kwiver::adapter::adapter_data_set_t receive();

  /**
   * @brief Is interface queue empty?
   *
   *
   * @return \b true if interface queue is full and thread would wait for receive().
   */
  bool empty() const;

private:
  kwiver::output_adapter_process* m_process;
  kwiver::adapter::interface_ref_t m_interface_queue;
}; // end class output_adapter

} // end namespace

#endif // KWIVER_NPUT_ADAPTER_H