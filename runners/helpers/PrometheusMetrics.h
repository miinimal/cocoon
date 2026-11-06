#pragma once

#include "td/utils/Slice.h"

#include <string>

namespace cocoon {

// Converts the JSON produced by a runner's http_generate_json_stats() into the Prometheus
// text exposition format (version 0.0.4), so /metrics always reports the same data as /jsonstats.
//
// Mapping, with every name prefixed by `prefix` (for example "cocoon_worker") and the JSON path
// joined with '_':
//  - number or boolean            -> gauge  <prefix>_<path> <value>   (true = 1, false = 0)
//  - AmortCounterList array        -> counter <prefix>_<path>_total <cumulative value>
//    [total, 1s, 10s, 1m, 10m, 1h, 1d]   gauge <prefix>_<path>_rate{window="1s"|...} <rate per second>
//  - "git_commit" string           -> gauge  <prefix>_build_info{git_commit="..."} 1
//  - other strings, null, arrays of objects are skipped.
std::string json_stats_to_prometheus(td::Slice json, td::Slice prefix);

}  // namespace cocoon
