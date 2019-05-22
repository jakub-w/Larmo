#include "Config.h"

#include <fstream>
#include <iostream>
#include <regex>

const std::string Config::default_conf_file_ = "lrm.conf";
std::map<std::string, std::string> Config::config_ = {};
Config::State Config::state_ = NOT_LOADED;

// Define methods
void Config::Load(std::string_view filename) {
  if (LOADED == state_) {
    return;
  }

  std::ifstream fs(filename.data());
  if (!fs) {
    state_ = ERROR;
    throw std::logic_error(std::string("Config file '")
                           + filename.data() + "' could not be loaded.");
  }

  for (std::string line; std::getline(fs, line);) {
    std::smatch smatch;
    if (false == std::regex_match(line, smatch,
                                  std::regex("(\\w+)\\s?=\\s?(\\S+)"))) {
      std::cerr << "Line: '" << line << "' could not be parsed.\n";
      continue;
    }

    config_[smatch[1]] = smatch[2];
  }

  state_ = LOADED;
}

const std::string& Config::Get(std::string_view variable) {
  switch(state_) {
    case LOADED:
      // ensure that it won't add unnecessary entries to a map
      try {
        return config_.at(variable.data());
      } catch (const std::out_of_range& e) {
        return config_[""];
      }
    case NOT_LOADED:
      Load();
      return Get(variable);
    default:
      // if someone chooses to ignore error thrown by Load(), he will get an
      // empty string every time
      return config_[""];
  }
}

void Config::Set(std::string_view variable, std::string_view value) {
  config_[variable.data()] = value;
}
