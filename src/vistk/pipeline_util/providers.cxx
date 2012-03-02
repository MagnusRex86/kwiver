/*ckwg +5
 * Copyright 2011 by Kitware, Inc. All Rights Reserved. Please refer to
 * KITWARE_LICENSE.TXT for licensing information, or contact General Counsel,
 * Kitware, Inc., 28 Corporate Drive, Clifton Park, NY 12065.
 */

#include "providers.h"

#include "pipe_bakery_exception.h"

#include <vistk/utilities/path.h>

#include <vistk/pipeline/config.h>
#include <vistk/pipeline/utils.h>

#include <boost/filesystem/operations.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread/thread.hpp>
#include <boost/lexical_cast.hpp>

/**
 * \file providers.cxx
 *
 * \brief Implementation of configuration providers.
 */

namespace vistk
{

provider
::provider()
{
}

provider
::~provider()
{
}

config_provider
::config_provider(config_t const conf)
  : m_config(conf)
{
}

config_provider
::~config_provider()
{
}

config::value_t
config_provider
::operator () (config::value_t const& index) const
{
  return m_config->get_value<config::value_t>(index);
}

system_provider
::system_provider()
{
}

system_provider
::~system_provider()
{
}

config::value_t
system_provider
::operator () (config::value_t const& index) const
{
  config::value_t value;

  if (index == "processors")
  {
    value = boost::lexical_cast<config::value_t>(boost::thread::hardware_concurrency());
  }
  else if (index == "homedir")
  {
    envvar_value_t home = get_envvar(
#if defined(_WIN32) || defined(_WIN64)
      "UserProfile"
#else
      "HOME"
#endif
    );

    if (home)
    {
      value = config::value_t(home);
    }

    free_envvar(home);
  }
  else if (index == "curdir")
  {
    boost::system::error_code ec;
    path_t const curdir = boost::filesystem::current_path(ec);

    /// \todo Check ec.

    path_t::string_type const& raw_path = curdir.native();

    value = config::value_t(raw_path.begin(), raw_path.end());
  }
  else
  {
    throw unrecognized_system_index_exception(index);
  }

  return value;
}

environment_provider
::environment_provider()
{
}

environment_provider
::~environment_provider()
{
}

config::value_t
environment_provider
::operator () (config::value_t const& index) const
{
  envvar_name_t const envvar_name = index.c_str();
  envvar_value_t envvar_value = get_envvar(envvar_name);

  config::value_t value;

  if (envvar_value)
  {
    value = config::value_t(envvar_value);
  }

  free_envvar(envvar_value);

  return value;
}

}
