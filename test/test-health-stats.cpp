// Tests for StatsCollector::retain (tee/cocoon/health-stats.cpp): trackers of disks and network interfaces that are
// no longer reported are dropped, so devices that come and go (veth pairs, dynamic volumes) do not accumulate.
#include "tee/cocoon/health-stats.h"

#include <cstdio>
#include <map>
#include <string>
#include <utility>

namespace {

int failures = 0;

void expect(const char *name, bool ok) {
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", name);
  failures += ok ? 0 : 1;
}

using Io = std::map<std::string, std::pair<uint64_t, uint64_t>>;

}  // namespace

int main() {
  cocoon::StatsCollector s;

  // a long-lived disk and interface plus 1000 short-lived veth interfaces
  for (int i = 0; i < 1000; i++) {
    Io net{{"eth0", {100u + i, 200u + i}}, {"veth" + std::to_string(i), {1, 1}}};
    Io disk{{"vda", {10u + i, 20u + i}}};
    for (const auto &[k, v] : disk) {
      s.update_disk(k, v.first, v.second);
    }
    for (const auto &[k, v] : net) {
      s.update_network(k, v.first, v.second);
    }
    s.retain(disk, net);
  }

  expect("current interface is kept", s.has_network("veth999"));
  expect("vanished interfaces are dropped", !s.has_network("veth0") && !s.has_network("veth998"));
  expect("long-lived interface is kept", s.has_network("eth0"));
  expect("long-lived disk is kept", s.has_disk("vda"));

  s.retain(Io{}, Io{{"eth0", {1, 1}}});
  expect("vanished disk is dropped", !s.has_disk("vda"));
  expect("interface still reported is kept", s.has_network("eth0"));

  std::printf(failures ? "%d test(s) FAILED\n" : "all tests passed\n", failures);
  return failures ? 1 : 0;
}
