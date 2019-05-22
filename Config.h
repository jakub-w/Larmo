#ifndef LRM_CONFIG_H
#define LRM_CONFIG_H

#include <string>
#include <map>

class Config {
  static const std::string default_conf_file_;
  static std::map<std::string, std::string> config_;

  static enum State {
    NOT_LOADED,
    LOADED,
    ERROR
  } state_;

public:
  static void Load(std::string_view filename = default_conf_file_);

  static const std::string& Get(std::string_view variable);
  static inline State GetState() {
    return state_;
  }
  static void Set(std::string_view variable, std::string_view value);
};

#endif // LRM_CONFIG_H
