// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
//
// 2011-12-10 GONG Chen <chen.sst@gmail.com>
//
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <rime_version.h>
#include <rime/common.h>
#include <rime/config.h>
#include <rime/dict/dictionary.h>
#include <rime/dict/dict_compiler.h>
#include <rime/expl/customizer.h>
#include <rime/expl/deployment_tasks.h>

namespace fs = boost::filesystem;

namespace rime {

bool InstallationUpdate::Run(Deployer* deployer) {
  EZLOGGERPRINT("updating rime installation.");
  fs::path shared_data_path(deployer->shared_data_dir);
  fs::path user_data_path(deployer->user_data_dir);
  fs::path installation_info(user_data_path / "installation.yaml");
  rime::Config config;
  std::string installation_id;
  std::string last_distro_code_name;
  std::string last_distro_version;
  std::string last_rime_version;
  if (config.LoadFromFile(installation_info.string())) {
    if (config.GetString("installation_id", &installation_id)) {
      EZLOGGERPRINT("installation info exists. installation id: %s",
                    installation_id.c_str());
      // for now:
      deployer->user_id = installation_id;
    }
    if (config.GetString("distribution_code_name", &last_distro_code_name)) {
      EZLOGGERPRINT("previous distribution: %s",
                    last_distro_code_name.c_str());
    }
    if (config.GetString("distribution_version", &last_distro_version)) {
      EZLOGGERPRINT("previous distribution version: %s",
                    last_distro_version.c_str());
    }
    if (config.GetString("rime_version", &last_rime_version)) {
      EZLOGGERPRINT("previous Rime version: %s", last_rime_version.c_str());
    }
  }
  if (!installation_id.empty() &&
      last_distro_code_name == deployer->distribution_code_name &&
      last_distro_version == deployer->distribution_version &&
      last_rime_version == RIME_VERSION) {
    return true;
  }
  EZLOGGERPRINT("creating installation.");
  time_t now = time(NULL);
  std::string time_str(ctime(&now));
  boost::trim(time_str);
  if (installation_id.empty()) {
    installation_id =
        boost::uuids::to_string(boost::uuids::random_generator()());
    EZLOGGERPRINT("generated installation id: %s", installation_id.c_str());
    // for now:
    deployer->user_id = installation_id;
    config.SetString("installation_id", installation_id);
    config.SetString("install_time", time_str);
  }
  else {
    config.SetString("update_time", time_str);
  }
  if (!deployer->distribution_name.empty()) {
    config.SetString("distribution_name", deployer->distribution_name);
    EZLOGGERPRINT("distribution: %s", deployer->distribution_name.c_str());
  }
  if (!deployer->distribution_code_name.empty()) {
    config.SetString("distribution_code_name",
                     deployer->distribution_code_name);
    EZLOGGERPRINT("distribution code name: %s",
                  deployer->distribution_code_name.c_str());
  }
  if (!deployer->distribution_version.empty()) {
    config.SetString("distribution_version",
                     deployer->distribution_version);
    EZLOGGERPRINT("distribution version: %s",
                  deployer->distribution_version.c_str());
  }
  config.SetString("rime_version", RIME_VERSION);
  EZLOGGERPRINT("rime version: %s", RIME_VERSION);
  return config.SaveToFile(installation_info.string());
}

bool WorkspaceUpdate::Run(Deployer* deployer) {
  EZLOGGERPRINT("updating workspace.");
  fs::path shared_data_path(deployer->shared_data_dir);
  fs::path user_data_path(deployer->user_data_dir);
  fs::path default_config_path(user_data_path / "default.yaml");
  {
    scoped_ptr<DeploymentTask> t;
    t.reset(new ConfigFileUpdate("default.yaml", "config_version"));
    t->Run(deployer);
  }

  Config config;
  if (!config.LoadFromFile(default_config_path.string())) {
    EZLOGGERPRINT("Error loading default config from '%s'.",
                  default_config_path.string().c_str());
    return false;
  }
  ConfigListPtr schema_list = config.GetList("schema_list");
  if (!schema_list) {
    EZLOGGERPRINT("Warning: schema list not defined.");
    return false;
  }

  EZLOGGERPRINT("updating schemas.");
  int success = 0;
  int failure = 0;
  ConfigList::Iterator it = schema_list->begin();
  for (; it != schema_list->end(); ++it) {
    ConfigMapPtr item = As<ConfigMap>(*it);
    if (!item) continue;
    ConfigValuePtr schema_property = item->GetValue("schema");
    if (!schema_property) continue;
    const std::string &schema_id(schema_property->str());
    fs::path schema_path = shared_data_path / (schema_id + ".schema.yaml");
    if (!fs::exists(schema_path)) {
      schema_path = user_data_path / (schema_id + ".schema.yaml");
    }
    scoped_ptr<DeploymentTask> t(new SchemaUpdate(schema_path.string()));
    if (t->Run(deployer))
      ++success;
    else
      ++failure;
  }
  EZLOGGERPRINT("finished updating schemas: %d success, %d failure.",
                success, failure);
  return failure != 0;
}

bool SchemaUpdate::Run(Deployer* deployer) {
  fs::path source_path(schema_file_);
  if (!fs::exists(source_path)) {
    EZLOGGERPRINT("Error updating schema: nonexistent file '%s'.",
                  schema_file_.c_str());
    return false;
  }
  Config config;
  std::string schema_id;
  if (!config.LoadFromFile(schema_file_) ||
      !config.GetString("schema/schema_id", &schema_id) ||
      schema_id.empty()) {
    EZLOGGERPRINT("Error: invalid schema definition in '%s'.",
                  schema_file_.c_str());
    return false;
  }
  
  fs::path shared_data_path(deployer->shared_data_dir);
  fs::path user_data_path(deployer->user_data_dir);
  fs::path destination_path(user_data_path / (schema_id + ".schema.yaml"));
  Customizer customizer(source_path, destination_path, "schema/version");
  if (customizer.UpdateConfigFile()) {
    EZLOGGERPRINT("schema '%s' is updated.", schema_id.c_str());
  }
  
  if (!config.LoadFromFile(destination_path.string())) {
    EZLOGGERPRINT("Error loading schema file '%s'.",
                  destination_path.string().c_str());
    return false;
  }
  std::string dict_name;
  if (!config.GetString("translator/dictionary", &dict_name)) {
    // not requiring a dictionary
    return true;
  }
  std::string dict_file_name(dict_name + ".dict.yaml");
  fs::path dict_path(source_path.parent_path() / dict_file_name);
  if (!fs::exists(dict_path)) {
    dict_path = shared_data_path / dict_file_name;
    if (!fs::exists(dict_path)) {
      EZLOGGERPRINT("Error: source file for dictionary '%s' does not exist.",
                    dict_name.c_str());
      return false;
    }
  }
  DictionaryComponent component;
  scoped_ptr<Dictionary> dict;
  dict.reset(component.CreateDictionaryFromConfig(&config, "translator"));
  if (!dict) {
    EZLOGGERPRINT("Error creating dictionary '%s'.", dict_name.c_str());
    return false;
  }
  EZLOGGERPRINT("preparing dictionary '%s'.", dict_name.c_str());
  DictCompiler dict_compiler(dict.get());
  dict_compiler.Compile(dict_path.string(), destination_path.string());
  EZLOGGERPRINT("dictionary '%s' is ready.", dict_name.c_str());
  return true;
}

bool ConfigFileUpdate::Run(Deployer* deployer) {
  fs::path shared_data_path(deployer->shared_data_dir);
  fs::path user_data_path(deployer->user_data_dir);
  fs::path source_config_path(shared_data_path / file_name_);
  fs::path dest_config_path(user_data_path / file_name_);
  if (!fs::exists(source_config_path)) {
    EZLOGGERPRINT("Warning: '%s' is missing from shared data directory.",
                  file_name_.c_str());
    source_config_path = dest_config_path;
  }
  Customizer customizer(source_config_path, dest_config_path, version_key_);
  return customizer.UpdateConfigFile();
}

}  // namespace rime
