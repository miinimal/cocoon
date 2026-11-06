// Tests for json_stats_to_prometheus (runners/helpers/PrometheusMetrics.cpp), which serves /metrics
// from the same JSON as /jsonstats.
#include "runners/helpers/PrometheusMetrics.h"

#include <cstdio>
#include <string>

namespace {

int failures = 0;

void expect_line(const char *name, const std::string &text, const std::string &line) {
  bool found = text.find(line + "\n") != std::string::npos;
  std::printf("%s %s\n", found ? "ok  " : "FAIL", name);
  if (!found) {
    std::printf("     missing line: %s\n", line.c_str());
    failures++;
  }
}

void expect_absent(const char *name, const std::string &text, const std::string &fragment) {
  bool found = text.find(fragment) != std::string::npos;
  std::printf("%s %s\n", found ? "FAIL" : "ok  ", name);
  if (found) {
    failures++;
  }
}

}  // namespace

int main() {
  const std::string json = R"({
    "status": {"wallet_balance": 1500000000, "actual_image_hash": true, "enabled": false,
               "git_commit": "541432c\"x", "git_commit_data": "2026-05-21"},
    "stats": {"queries": [12, 0.5, 0.4, 0.2, 0.1, 0.01, 0.001], "bytes_sent": [123456789012, 0, 0, 0, 0, 0, 0]},
    "wallet": {"address": "EQ...", "seqno": 7},
    "workers": [{"x": 1}],
    "1st": 2
  })";
  auto m = cocoon::json_stats_to_prometheus(json, "cocoon_worker");
  std::printf("%s---\n", m.c_str());

  expect_line("number is a gauge", m, "cocoon_worker_status_wallet_balance 1500000000");
  expect_line("gauge has a TYPE line", m, "# TYPE cocoon_worker_status_wallet_balance gauge");
  expect_line("true is 1", m, "cocoon_worker_status_actual_image_hash 1");
  expect_line("false is 0", m, "cocoon_worker_status_enabled 0");
  expect_line("counter list total is a counter", m, "# TYPE cocoon_worker_stats_queries_total counter");
  expect_line("counter list total value", m, "cocoon_worker_stats_queries_total 12");
  expect_line("rate per window", m, "cocoon_worker_stats_queries_rate{window=\"10s\"} 0.4");
  expect_line("last window", m, "cocoon_worker_stats_queries_rate{window=\"1d\"} 0.001");
  expect_line("large totals stay exact", m, "cocoon_worker_stats_bytes_sent_total 123456789012");
  expect_line("build info with escaped label", m, "cocoon_worker_build_info{git_commit=\"541432c\\\"x\"} 1");
  expect_line("nested object path", m, "cocoon_worker_wallet_seqno 7");
  expect_line("key starting with a digit", m, "cocoon_worker_1st 2");
  expect_absent("other strings are skipped", m, "git_commit_data");
  expect_absent("arrays of objects are skipped", m, "workers");

  auto bad = cocoon::json_stats_to_prometheus("not json", "cocoon_proxy");
  expect_line("unparsable stats are reported", bad, "cocoon_proxy_metrics_error 1");

  std::printf(failures ? "%d test(s) FAILED\n" : "all tests passed\n", failures);
  return failures ? 1 : 0;
}
