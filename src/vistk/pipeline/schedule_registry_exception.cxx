/*ckwg +5
 * Copyright 2011 by Kitware, Inc. All Rights Reserved. Please refer to
 * KITWARE_LICENSE.TXT for licensing information, or contact General Counsel,
 * Kitware, Inc., 28 Corporate Drive, Clifton Park, NY 12065.
 */

#include "schedule_registry_exception.h"

#include <sstream>

/**
 * \file schedule_registry_exception.cxx
 *
 * \brief Implementation of exceptions used within the \link vistk::schedule_registry schedule registry\endlink.
 */

namespace vistk
{

no_such_schedule_type
::no_such_schedule_type(schedule_registry::type_t const& type) throw()
  : schedule_registry_exception()
  , m_type(type)
{
  std::ostringstream sstr;

  sstr << "There is no such schedule of type \'" << type << "\' "
       << "in the registry.";

  m_what = sstr.str();
}

no_such_schedule_type
::~no_such_schedule_type() throw()
{
}

char const*
no_such_schedule_type
::what() const throw()
{
  return m_what.c_str();
}

schedule_type_already_exists
::schedule_type_already_exists(schedule_registry::type_t const& type) throw()
  : schedule_registry_exception()
  , m_type(type)
{
  std::ostringstream sstr;

  sstr << "There is already a schedule of type \'" << type << "\' "
       << "in the registry.";

  m_what = sstr.str();
}

schedule_type_already_exists
::~schedule_type_already_exists() throw()
{
}

char const*
schedule_type_already_exists
::what() const throw()
{
  return m_what.c_str();
}

} // end namespace vistk
