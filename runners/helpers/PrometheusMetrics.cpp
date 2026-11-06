#include "PrometheusMetrics.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>

namespace cocoon {

namespace {

const char *const kWindows[] = {"1s", "10s", "1m", "10m", "1h", "1d"};

std::string sanitize(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    out += ok ? c : '_';
  }
  return out;
}

std::string escape_label(const std::string &s) {
  std::string out;
  for (char c : s) {
    if (c == '\\' || c == '"') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else {
      out += c;
    }
  }
  return out;
}

std::string format_value(double v) {
  if (std::isnan(v)) {
    return "NaN";
  }
  if (std::isinf(v)) {
    return v > 0 ? "+Inf" : "-Inf";
  }
  // shortest of %.15g / %.17g that reads back exactly, so 0.4 stays 0.4 and large balances stay exact
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.15g", v);
  if (std::strtod(buf, nullptr) != v) {
    std::snprintf(buf, sizeof(buf), "%.17g", v);
  }
  return buf;
}

bool is_amort_counter_list(const nlohmann::json &j) {
  if (!j.is_array() || j.size() != 7) {
    return false;
  }
  for (const auto &x : j) {
    if (!x.is_number()) {
      return false;
    }
  }
  return true;
}

struct Writer {
  std::string out;
  std::set<std::string> typed;

  void type(const std::string &name, const char *kind) {
    if (typed.insert(name).second) {
      out += "# TYPE " + name + " " + kind + "\n";
    }
  }

  void sample(const std::string &name, const std::string &labels, double value) {
    out += name;
    if (!labels.empty()) {
      out += "{" + labels + "}";
    }
    out += " " + format_value(value) + "\n";
  }

  void walk(const nlohmann::json &j, const std::string &prefix, const std::string &path) {
    const std::string name = path.empty() ? prefix : prefix + "_" + path;
    if (j.is_object()) {
      for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string key = sanitize(it.key());
        if (it.key() == "git_commit" && it.value().is_string()) {
          type(prefix + "_build_info", "gauge");
          sample(prefix + "_build_info", "git_commit=\"" + escape_label(it.value().get<std::string>()) + "\"", 1);
          continue;
        }
        walk(it.value(), prefix, path.empty() ? key : path + "_" + key);
      }
    } else if (is_amort_counter_list(j)) {
      type(name + "_total", "counter");
      sample(name + "_total", "", j[0].get<double>());
      type(name + "_rate", "gauge");
      for (size_t i = 0; i < 6; i++) {
        sample(name + "_rate", std::string("window=\"") + kWindows[i] + "\"", j[i + 1].get<double>());
      }
    } else if (j.is_boolean()) {
      type(name, "gauge");
      sample(name, "", j.get<bool>() ? 1 : 0);
    } else if (j.is_number()) {
      type(name, "gauge");
      sample(name, "", j.get<double>());
    }
    // strings, null and other arrays carry no numeric sample
  }
};

}  // namespace

std::string json_stats_to_prometheus(td::Slice json, td::Slice prefix) {
  auto j = nlohmann::json::parse(json.begin(), json.end(), nullptr, false);
  Writer w;
  std::string p = sanitize(prefix.str());
  if (p.empty() || (p[0] >= '0' && p[0] <= '9')) {
    p = "cocoon_" + p;
  }
  if (j.is_discarded()) {
    w.type(p + "_metrics_error", "gauge");
    w.sample(p + "_metrics_error", "", 1);
    return w.out;
  }
  w.walk(j, p, "");
  return w.out;
}

}  // namespace cocoon
