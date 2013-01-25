/*ckwg +5
 * Copyright 2011-2013 by Kitware, Inc. All Rights Reserved. Please refer to
 * KITWARE_LICENSE.TXT for licensing information, or contact General Counsel,
 * Kitware, Inc., 28 Corporate Drive, Clifton Park, NY 12065.
 */

#include "pipe_bakery.h"
#include "pipe_bakery_exception.h"

#include "load_pipe.h"
#include "pipe_declaration_types.h"
#include "providers.h"

#include <vistk/pipeline/config.h>
#include <vistk/pipeline/pipeline.h>
#include <vistk/pipeline/process.h>
#include <vistk/pipeline/process_cluster.h>
#include <vistk/pipeline/process_registry.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/graph/directed_graph.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>
#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include <algorithm>
#include <istream>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

/**
 * \file pipe_bakery.cxx
 *
 * \brief Implementation of baking a pipeline.
 */

namespace vistk
{

namespace
{

static config::key_t const config_pipeline_key = config::key_t("_pipeline");

static config_flag_t const flag_read_only = config_flag_t("ro");
static config_flag_t const flag_append = config_flag_t("append");
static config_flag_t const flag_comma_append = config_flag_t("cappend");

static config_provider_t const provider_config = config_provider_t("CONF");
static config_provider_t const provider_environment = config_provider_t("ENV");
static config_provider_t const provider_system = config_provider_t("SYS");

}

class bakery_base
  : public boost::static_visitor<>
{
  public:
    bakery_base();
    virtual ~bakery_base();

    void operator () (config_pipe_block const& config_block);
    void operator () (process_pipe_block const& process_block);
    void operator () (connect_pipe_block const& connect_block);

    /**
     * \note We do *not* want std::map for the block management. With a map, we
     * may hide errors in the blocks (setting ro values, duplicate process
     * names, etc.)
     */

    typedef std::pair<config_provider_t, config::value_t> provider_request_t;
    typedef boost::variant<config::value_t, provider_request_t> config_reference_t;
    class config_info_t
    {
      public:
        config_info_t(config_reference_t const& ref,
                      bool ro,
                      bool app,
                      bool capp);
        ~config_info_t();

        config_reference_t reference;
        bool read_only;
        bool append;
        bool comma_append;
    };
    typedef std::pair<config::key_t, config_info_t> config_decl_t;
    typedef std::vector<config_decl_t> config_decls_t;

    typedef std::pair<process::name_t, process::type_t> process_decl_t;
    typedef std::vector<process_decl_t> process_decls_t;

    config_decls_t m_configs;
    process_decls_t m_processes;
    process::connections_t m_connections;
  protected:
    void register_config_value(config::key_t const& root_key, config_value_t const& value);
};

pipeline_t
bake_pipe_from_file(path_t const& fname)
{
  return bake_pipe_blocks(load_pipe_blocks_from_file(fname));
}

pipeline_t
bake_pipe(std::istream& istr, path_t const& inc_root)
{
  return bake_pipe_blocks(load_pipe_blocks(istr, inc_root));
}

class pipe_bakery
  : public bakery_base
{
  public:
    pipe_bakery();
    ~pipe_bakery();

    using bakery_base::operator ();
};

static config_t extract_configuration(bakery_base::config_decls_t& configs);

pipeline_t
bake_pipe_blocks(pipe_blocks const& blocks)
{
  pipeline_t pipe;

  pipe_bakery bakery;

  std::for_each(blocks.begin(), blocks.end(), boost::apply_visitor(bakery));

  bakery_base::config_decls_t& configs = bakery.m_configs;
  config_t global_conf = extract_configuration(configs);

  // Create pipeline.
  config_t const pipeline_conf = global_conf->subblock_view(config_pipeline_key);

  pipe = boost::make_shared<pipeline>(pipeline_conf);

  // Create processes.
  {
    process_registry_t reg = process_registry::self();

    BOOST_FOREACH (bakery_base::process_decl_t const& decl, bakery.m_processes)
    {
      process::name_t const& proc_name = decl.first;
      process::type_t const& proc_type = decl.second;
      config_t const proc_conf = global_conf->subblock_view(proc_name);

      process_t const proc = reg->create_process(proc_type, proc_name, proc_conf);

      pipe->add_process(proc);
    }
  }

  // Make connections.
  {
    BOOST_FOREACH (process::connection_t const& conn, bakery.m_connections)
    {
      process::port_addr_t const& up = conn.first;
      process::port_addr_t const& down = conn.second;

      process::name_t const& up_name = up.first;
      process::port_t const& up_port = up.second;
      process::name_t const& down_name = down.first;
      process::port_t const& down_port = down.second;

      pipe->connect(up_name, up_port, down_name, down_port);
    }
  }

  return pipe;
}

cluster_info
::cluster_info(process::type_t const& type_,
               process_registry::description_t const& description_,
               process_ctor_t const& ctor_)
  : type(type_)
  , description(description_)
  , ctor(ctor_)
{
}

cluster_info
::~cluster_info()
{
}

cluster_info_t
bake_cluster_from_file(path_t const& fname)
{
  return bake_cluster_blocks(load_cluster_blocks_from_file(fname));
}

cluster_info_t
bake_cluster(std::istream& istr, path_t const& inc_root)
{
  return bake_cluster_blocks(load_cluster_blocks(istr, inc_root));
}

class cluster_bakery
  : public bakery_base
{
  public:
    cluster_bakery();
    ~cluster_bakery();

    using bakery_base::operator ();
    void operator () (cluster_pipe_block const& cluster_block_);

    class cluster_component_info_t
    {
      public:
        cluster_component_info_t();
        ~cluster_component_info_t();

        typedef std::vector<cluster_config_t> config_maps_t;
        typedef std::vector<cluster_input_t> input_maps_t;
        typedef std::vector<cluster_output_t> output_maps_t;

        config_maps_t m_configs;
        input_maps_t m_inputs;
        output_maps_t m_outputs;
    };
    typedef boost::optional<cluster_component_info_t> opt_cluster_component_info_t;

    process::type_t m_type;
    process_registry::description_t m_description;
    opt_cluster_component_info_t m_cluster;
};

static void dereference_static_providers(bakery_base::config_decls_t& bakery);

class cluster_creator
{
  public:
    cluster_creator(cluster_bakery const& bakery);
    ~cluster_creator();

    process_t operator () (config_t const& config) const;

    cluster_bakery const m_bakery;
  private:
    config_t m_default_config;
};

cluster_info_t
bake_cluster_blocks(cluster_blocks const& blocks)
{
  cluster_bakery bakery;

  std::for_each(blocks.begin(), blocks.end(), boost::apply_visitor(bakery));

  if (bakery.m_processes.empty())
  {
    throw cluster_without_processes_exception();
  }

  cluster_bakery::opt_cluster_component_info_t const& opt_cluster = bakery.m_cluster;

  if (!opt_cluster)
  {
    throw missing_cluster_block_exception();
  }

  cluster_bakery::cluster_component_info_t const& cluster = *opt_cluster;

  if (cluster.m_inputs.empty() &&
      cluster.m_outputs.empty())
  {
    throw cluster_without_ports_exception();
  }

  bakery_base::config_decls_t& configs = bakery.m_configs;

  dereference_static_providers(configs);

  process::type_t const& type = bakery.m_type;
  process_registry::description_t const& description = bakery.m_description;
  process_ctor_t const ctor = cluster_creator(bakery);

  cluster_info_t const info = boost::make_shared<cluster_info>(type, description, ctor);

  return info;
}

config_t
extract_configuration(pipe_blocks const& blocks)
{
  pipe_bakery bakery;

  std::for_each(blocks.begin(), blocks.end(), boost::apply_visitor(bakery));

  bakery_base::config_decls_t& configs = bakery.m_configs;

  return extract_configuration(configs);
}

class provider_dereferencer
  : public boost::static_visitor<bakery_base::config_reference_t>
{
  public:
    provider_dereferencer();
    provider_dereferencer(config_t const conf);
    ~provider_dereferencer();

    bakery_base::config_reference_t operator () (config::value_t const& value) const;
    bakery_base::config_reference_t operator () (bakery_base::provider_request_t const& request) const;
  private:
    typedef std::map<config_provider_t, provider_t> provider_map_t;
    provider_map_t m_providers;
};

class ensure_provided
  : public boost::static_visitor<config::value_t>
{
  public:
    ensure_provided();
    ~ensure_provided();

    config::value_t operator () (config::value_t const& value) const;
    config::value_t operator () (bakery_base::provider_request_t const& request) const;
};

static void set_config_value(config_t conf, bakery_base::config_info_t const& flags, config::key_t const& key, config::value_t const& value);

class config_provider_sorter
  : public boost::static_visitor<>
{
  public:
    config_provider_sorter();
    ~config_provider_sorter();

    void operator () (config::key_t const& key, config::value_t const& value) const;
    void operator () (config::key_t const& key, bakery_base::provider_request_t const& request);

    config::keys_t sorted() const;
  private:
    class node_t
    {
      public:
        node_t();
        ~node_t();

        bool deref;
        config::key_t name;
    };

    typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, node_t> config_graph_t;
    typedef boost::graph_traits<config_graph_t>::vertex_descriptor vertex_t;
    typedef std::vector<vertex_t> vertices_t;
    typedef std::map<config::key_t, vertex_t> vertex_map_t;
    typedef vertex_map_t::value_type vertex_entry_t;

    vertex_map_t m_vertex_map;
    config_graph_t m_graph;
};

config_t
extract_configuration(bakery_base::config_decls_t& configs)
{
  dereference_static_providers(configs);

  config_t tmp_conf = config::empty_config();

  ensure_provided const ensure;

  BOOST_FOREACH (bakery_base::config_decl_t& decl, configs)
  {
    config::key_t const& key = decl.first;
    bakery_base::config_info_t const& info = decl.second;
    bakery_base::config_reference_t const& ref = info.reference;

    config::value_t val;

    // Only add provided configurations to the configuration.
    try
    {
      val = boost::apply_visitor(ensure, ref);
    }
    catch (...)
    {
      continue;
    }

    set_config_value(tmp_conf, info, key, val);
  }

  // Dereference configuration providers.
  {
    config_provider_sorter sorter;

    BOOST_FOREACH (bakery_base::config_decl_t& decl, configs)
    {
      config::key_t const& key = decl.first;
      bakery_base::config_info_t const& info = decl.second;
      bakery_base::config_reference_t const& ref = info.reference;

      /// \bug Why must this be done?
      typedef boost::variant<config::key_t> dummy_variant;

      dummy_variant const var(key);

      boost::apply_visitor(sorter, var, ref);
    }

    config::keys_t const keys = sorter.sorted();

    provider_dereferencer const deref(tmp_conf);

    /// \todo This is algorithmically naive, but I'm not sure if there's a better way.
    BOOST_FOREACH (config::key_t const& key, keys)
    {
      BOOST_FOREACH (bakery_base::config_decl_t& decl, configs)
      {
        config::key_t const& cur_key = decl.first;

        if (key != cur_key)
        {
          continue;
        }

        bakery_base::config_info_t& info = decl.second;
        bakery_base::config_reference_t& ref = info.reference;

        ref = boost::apply_visitor(deref, ref);

        config::value_t const val = boost::apply_visitor(ensure, ref);

        set_config_value(tmp_conf, info, key, val);
      }
    }
  }

  config_t conf = config::empty_config();

  BOOST_FOREACH (bakery_base::config_decl_t& decl, configs)
  {
    config::key_t const& key = decl.first;
    bakery_base::config_info_t const& info = decl.second;
    bakery_base::config_reference_t const& ref = info.reference;

    config::value_t val;

    try
    {
      val = boost::apply_visitor(ensure, ref);
    }
    catch (unrecognized_provider_exception const& e)
    {
      throw unrecognized_provider_exception(key, e.m_provider, e.m_index);
    }

    set_config_value(conf, info, key, val);
  }

  return conf;
}

bakery_base::config_info_t
::config_info_t(config_reference_t const& ref,
                bool ro,
                bool app,
                bool capp)
  : reference(ref)
  , read_only(ro)
  , append(app)
  , comma_append(capp)
{
}

bakery_base::config_info_t
::~config_info_t()
{
}

bakery_base
::bakery_base()
  : m_configs()
  , m_processes()
  , m_connections()
{
}

bakery_base
::~bakery_base()
{
}

static config::key_t flatten_keys(config::keys_t const& keys);

void
bakery_base
::operator () (config_pipe_block const& config_block)
{
  config::key_t const root_key = flatten_keys(config_block.key);

  config_values_t const& values = config_block.values;

  BOOST_FOREACH (config_value_t const& value, values)
  {
    register_config_value(root_key, value);
  }
}

void
bakery_base
::operator () (process_pipe_block const& process_block)
{
  config_values_t const& values = process_block.config_values;

  BOOST_FOREACH (config_value_t const& value, values)
  {
    register_config_value(process_block.name, value);
  }

  m_processes.push_back(process_decl_t(process_block.name, process_block.type));
}

void
bakery_base
::operator () (connect_pipe_block const& connect_block)
{
  m_connections.push_back(process::connection_t(connect_block.from, connect_block.to));
}

void
bakery_base
::register_config_value(config::key_t const& root_key, config_value_t const& value)
{
  config_key_t const key = value.key;

  config::key_t const subkey = flatten_keys(key.key_path);

  config_reference_t c_value;

  if (key.options.provider)
  {
    c_value = provider_request_t(*key.options.provider, value.value);
  }
  else
  {
    c_value = value.value;
  }

  config::key_t const full_key = root_key + config::block_sep + subkey;

  bool is_readonly = false;
  bool append = false;
  bool comma_append = false;

  if (key.options.flags)
  {
    BOOST_FOREACH (config_flag_t const& flag, *key.options.flags)
    {
      if (flag == flag_read_only)
      {
        is_readonly = true;
      }
      else if (flag == flag_append)
      {
        append = true;
      }
      else if (flag == flag_comma_append)
      {
        comma_append = true;
      }
      else
      {
        throw unrecognized_config_flag_exception(full_key, flag);
      }
    }
  }

  config_info_t const info = config_info_t(c_value, is_readonly, append, comma_append);

  config_decl_t const decl = config_decl_t(full_key, info);

  m_configs.push_back(decl);
}

pipe_bakery
::pipe_bakery()
  : bakery_base()
{
}

pipe_bakery
::~pipe_bakery()
{
}

cluster_bakery
::cluster_bakery()
  : bakery_base()
{
}

cluster_bakery
::~cluster_bakery()
{
}

class cluster_splitter
  : public boost::static_visitor<>
{
  public:
    cluster_splitter(cluster_bakery::cluster_component_info_t& info);
    ~cluster_splitter();

    void operator () (cluster_config_t const& config_block);
    void operator () (cluster_input_t const& input_block);
    void operator () (cluster_output_t const& output_block);

    cluster_bakery::cluster_component_info_t& m_info;
  private:
    typedef std::set<process::port_t> unique_ports_t;

    unique_ports_t m_input_ports;
    unique_ports_t m_output_ports;
};

void
cluster_bakery
::operator () (cluster_pipe_block const& cluster_block_)
{
  if (m_cluster)
  {
    throw multiple_cluster_blocks_exception();
  }

  m_type = cluster_block_.type;
  m_description = cluster_block_.description;

  cluster_component_info_t cluster;

  cluster_subblocks_t const& subblocks = cluster_block_.subblocks;

  cluster_splitter splitter(cluster);

  std::for_each(subblocks.begin(), subblocks.end(), boost::apply_visitor(splitter));

  m_cluster = cluster;

  cluster_component_info_t::config_maps_t const& values = cluster.m_configs;
  BOOST_FOREACH (cluster_config_t const& value, values)
  {
    config_value_t const& config_value = value.config_value;

    register_config_value(m_type, config_value);
  }
}

cluster_bakery::cluster_component_info_t
::cluster_component_info_t()
{
}

cluster_bakery::cluster_component_info_t
::~cluster_component_info_t()
{
}

void
dereference_static_providers(bakery_base::config_decls_t& configs)
{
  provider_dereferencer const deref;

  BOOST_FOREACH (bakery_base::config_decl_t& decl, configs)
  {
    bakery_base::config_info_t& info = decl.second;
    bakery_base::config_reference_t& ref = info.reference;

    ref = boost::apply_visitor(deref, ref);
  }
}

cluster_creator
::cluster_creator(cluster_bakery const& bakery)
  : m_bakery(bakery)
{
  bakery_base::config_decls_t default_configs = m_bakery.m_configs;

  m_default_config = extract_configuration(default_configs);
}

cluster_creator
::~cluster_creator()
{
}

class loaded_cluster
  : public process_cluster
{
  public:
    loaded_cluster(config_t const& config);
    ~loaded_cluster();

    friend class cluster_creator;
};

process_t
cluster_creator
::operator () (config_t const& config) const
{
  bakery_base::config_decls_t all_configs = m_bakery.m_configs;

  process::type_t const& type = m_bakery.m_type;

  // Append the given configuration to the declarations in the file.
  config::keys_t const& keys = config->available_values();
  BOOST_FOREACH (config::key_t const& key, keys)
  {
    config::value_t const value = config->get_value<config::value_t>(key);
    bakery_base::config_reference_t const ref = bakery_base::config_reference_t(value);
    bool const is_read_only = config->is_read_only(key);
    bakery_base::config_info_t const info = bakery_base::config_info_t(ref, is_read_only, false, false);
    config::key_t const full_key = config::key_t(type) + config::block_sep + key;
    bakery_base::config_decl_t const decl = bakery_base::config_decl_t(full_key, info);

    all_configs.push_back(decl);
  }

  config_t const full_config = extract_configuration(all_configs);

  typedef boost::shared_ptr<loaded_cluster> loaded_cluster_t;

  // Pull out the main config block to the top-level.
  config_t const cluster_config = full_config->subblock_view(type);
  full_config->merge_config(cluster_config);

  loaded_cluster_t const cluster = boost::make_shared<loaded_cluster>(full_config);

  cluster_bakery::opt_cluster_component_info_t const& opt_info = m_bakery.m_cluster;

  if (!opt_info)
  {
    static std::string const reason = "Failed to catch missing cluster block earlier";

    throw std::logic_error(reason);
  }

  cluster_bakery::cluster_component_info_t const& info = *opt_info;

  config_t const main_config = m_default_config->subblock_view(type);

  // Declare configuration values.
  BOOST_FOREACH (cluster_config_t const& conf, info.m_configs)
  {
    // Calls to map_config are not necessary because extract_configuration does
    // the mappings for us via configuration providers.

    config_value_t const& config_value = conf.config_value;
    config_key_t const& config_key = config_value.key;
    config::keys_t const& key_path = config_key.key_path;
    config::key_t const& key = flatten_keys(key_path);
    config::value_t const& value = main_config->get_value<config::value_t>(key);
    config::description_t const& description = conf.description;

    cluster->declare_configuration_key(
      key,
      value,
      description);
  }

  // Add processes.
  BOOST_FOREACH (bakery_base::process_decl_t const& proc_decl, m_bakery.m_processes)
  {
    process::name_t const& proc_name = proc_decl.first;
    process::type_t const& proc_type = proc_decl.second;

    config_t const proc_config = full_config->subblock_view(proc_name);

    cluster->add_process(proc_name, proc_type, proc_config);
  }

  // Add input ports.
  {
    process::port_flags_t const input_flags;

    BOOST_FOREACH (cluster_input_t const& input, info.m_inputs)
    {
      process::port_description_t const& description = input.description;
      process::port_t const& port = input.from;

      cluster->declare_input_port(
        port,
        /// \todo How to declare a port's type?
        process::type_any,
        input_flags,
        description);

      process::port_addrs_t const& addrs = input.targets;

      BOOST_FOREACH (process::port_addr_t const& addr, addrs)
      {
        process::name_t const& mapped_name = addr.first;
        process::port_t const& mapped_port = addr.second;

        cluster->map_input(
          port,
          mapped_name,
          mapped_port);
      }
    }
  }

  // Add output ports.
  {
    process::port_flags_t const output_flags;

    BOOST_FOREACH (cluster_output_t const& output, info.m_outputs)
    {
      process::port_description_t const& description = output.description;
      process::port_t const& port = output.to;

      cluster->declare_output_port(
        port,
        /// \todo How to declare a port's type?
        process::type_any,
        output_flags,
        description);

      process::port_addr_t const& addr = output.from;

      process::name_t const& mapped_name = addr.first;
      process::port_t const& mapped_port = addr.second;

      cluster->map_output(
        port,
        mapped_name,
        mapped_port);
    }
  }

  // Add connections.
  BOOST_FOREACH (process::connection_t const& connection, m_bakery.m_connections)
  {
    process::port_addr_t const& upstream_addr = connection.first;
    process::port_addr_t const& downstream_addr = connection.second;

    process::name_t const& upstream_name = upstream_addr.first;
    process::port_t const& upstream_port = upstream_addr.second;
    process::name_t const& downstream_name = downstream_addr.first;
    process::port_t const& downstream_port = downstream_addr.second;

    cluster->connect(upstream_name, upstream_port,
                     downstream_name, downstream_port);
  }

  return cluster;
}

provider_dereferencer
::provider_dereferencer()
  : m_providers()
{
  m_providers[provider_system] = boost::make_shared<system_provider>();
  m_providers[provider_environment] = boost::make_shared<environment_provider>();
}

provider_dereferencer
::provider_dereferencer(config_t const conf)
  : m_providers()
{
  m_providers[provider_config] = boost::make_shared<config_provider>(conf);
}

provider_dereferencer
::~provider_dereferencer()
{
}

bakery_base::config_reference_t
provider_dereferencer
::operator () (config::value_t const& value) const
{
  return value;
}

bakery_base::config_reference_t
provider_dereferencer
::operator () (bakery_base::provider_request_t const& request) const
{
  config_provider_t const& provider_name = request.first;
  provider_map_t::const_iterator const i = m_providers.find(provider_name);

  if (i == m_providers.end())
  {
    return request;
  }

  provider_t const& provider = i->second;
  config::value_t const& value = request.second;

  return (*provider)(value);
}

ensure_provided
::ensure_provided()
{
}

ensure_provided
::~ensure_provided()
{
}

config::value_t
ensure_provided
::operator () (config::value_t const& value) const
{
  return value;
}

config::value_t
ensure_provided
::operator () (bakery_base::provider_request_t const& request) const
{
  config_provider_t const& provider = request.first;
  config::value_t const& value = request.second;

  throw unrecognized_provider_exception("(unknown)", provider, value);
}

void
set_config_value(config_t conf, bakery_base::config_info_t const& flags, config::key_t const& key, config::value_t const& value)
{
  config::value_t val = value;

  if (flags.comma_append || flags.append)
  {
    config::value_t const cur_val = conf->get_value(key, config::value_t());

    if (flags.comma_append && !cur_val.empty())
    {
      val = ',' + val;
    }

    val = cur_val + val;
  }

  conf->set_value(key, val);

  if (flags.read_only)
  {
    conf->mark_read_only(key);
  }
}

config_provider_sorter
::config_provider_sorter()
  : m_vertex_map()
  , m_graph()
{
}

config_provider_sorter
::~config_provider_sorter()
{
}

config::keys_t
config_provider_sorter
::sorted() const
{
  vertices_t vertices;

  try
  {
    boost::topological_sort(m_graph, std::back_inserter(vertices));
  }
  catch (boost::not_a_dag const&)
  {
    throw circular_config_provide_exception();
  }

  config::keys_t keys;

  BOOST_FOREACH (vertex_t const& vertex, vertices)
  {
    node_t const& node = m_graph[vertex];

    if (node.deref)
    {
      keys.push_back(node.name);
    }
  }

  return keys;
}

void
config_provider_sorter
::operator () (config::key_t const& /*key*/, config::value_t const& /*value*/) const
{
}

void
config_provider_sorter
::operator () (config::key_t const& key, bakery_base::provider_request_t const& request)
{
  config_provider_t const& provider = request.first;
  config::value_t const& value = request.second;

  if (provider != provider_config)
  {
    return;
  }

  config::key_t const& target_key = config::key_t(value);

  typedef std::pair<vertex_map_t::iterator, bool> insertion_t;

  insertion_t from_iter = m_vertex_map.insert(vertex_entry_t(key, vertex_t()));
  insertion_t to_iter = m_vertex_map.insert(vertex_entry_t(target_key, vertex_t()));

  bool const& from_inserted = from_iter.second;
  bool const& to_inserted = to_iter.second;

  vertex_map_t::iterator& from = from_iter.first;
  vertex_map_t::iterator& to = to_iter.first;

  vertex_t& from_vertex = from->second;
  vertex_t& to_vertex = to->second;

  if (from_inserted)
  {
    from_vertex = boost::add_vertex(m_graph);
    m_graph[from_vertex].name = key;
  }

  if (to_inserted)
  {
    to_vertex = boost::add_vertex(m_graph);
    m_graph[to_vertex].name = target_key;
  }

  m_graph[from_vertex].deref = true;

  boost::add_edge(from_vertex, to_vertex, m_graph);
}

config_provider_sorter::node_t
::node_t()
  : deref(false)
  , name()
{
}

config_provider_sorter::node_t
::~node_t()
{
}

config::key_t
flatten_keys(config::keys_t const& keys)
{
  return boost::join(keys, config::block_sep);
}

cluster_splitter
::cluster_splitter(cluster_bakery::cluster_component_info_t& info)
  : m_info(info)
{
}

cluster_splitter
::~cluster_splitter()
{
}

void
cluster_splitter
::operator () (cluster_config_t const& config_block)
{
  m_info.m_configs.push_back(config_block);
}

void
cluster_splitter
::operator () (cluster_input_t const& input_block)
{
  process::port_t const& port = input_block.from;
  unique_ports_t::const_iterator const i = m_input_ports.find(port);

  if (i != m_input_ports.end())
  {
    throw duplicate_cluster_input_port_exception(port);
  }

  m_input_ports.insert(port);

  m_info.m_inputs.push_back(input_block);
}

void
cluster_splitter
::operator () (cluster_output_t const& output_block)
{
  process::port_t const& port = output_block.to;
  unique_ports_t::const_iterator const i = m_output_ports.find(port);

  if (i != m_output_ports.end())
  {
    throw duplicate_cluster_output_port_exception(port);
  }

  m_output_ports.insert(port);

  m_info.m_outputs.push_back(output_block);
}

loaded_cluster
::loaded_cluster(config_t const& config)
  : process_cluster(config)
{
}

loaded_cluster
::~loaded_cluster()
{
}

}
