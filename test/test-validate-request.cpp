// Regression tests for OpenAI-compatible request validation (runners/helpers/ValidateRequest.cpp).
//
// The worker validates every request with validate_decrypt_request(); with a zero private key and
// an unencrypted body it checks the full schema, so these tests exercise the same path without keys.
// Each case states the behaviour the code has today; a change in behaviour must update the case.
#include "runners/helpers/ValidateRequest.h"

#include <cstdio>
#include <string>

namespace {

int failures = 0;

struct Out {
  td::Status status;
  std::string model;
  td::int64 max_tokens = -1;
};

Out validate(const std::string &url, const std::string &body) {
  Out out;
  td::Bits256 client_public_key;
  auto r = cocoon::validate_decrypt_request(url, "application/json", td::BufferSlice(body), &out.model,
                                            &out.max_tokens, td::Bits256::zero(), &client_public_key);
  out.status = r.is_ok() ? td::Status::OK() : r.move_as_error();
  return out;
}

void expect(const char *name, const std::string &url, const std::string &body, bool ok) {
  auto o = validate(url, body);
  if (o.status.is_ok() != ok) {
    std::printf("FAIL %s: expected %s, got %s\n", name, ok ? "ok" : "error",
                o.status.is_ok() ? "ok" : o.status.message().str().c_str());
    failures++;
  } else {
    std::printf("ok   %s%s%s\n", name, o.status.is_ok() ? "" : " -> ",
                o.status.is_ok() ? "" : o.status.message().str().c_str());
  }
}

void expect_chat(const char *name, const std::string &body, bool ok) {
  expect(name, "/v1/chat/completions", body, ok);
}

void expect_eq(const char *name, td::int64 got, td::int64 want) {
  if (got != want) {
    std::printf("FAIL %s: expected %lld, got %lld\n", name, (long long)want, (long long)got);
    failures++;
  } else {
    std::printf("ok   %s\n", name);
  }
}

const std::string kModel = R"("model":"Qwen/Qwen3-8B")";
const std::string kMsgs = R"("messages":[{"role":"user","content":"hello"}])";
const std::string kBase = kModel + "," + kMsgs;

}  // namespace

int main() {
  // accepted requests
  expect_chat("plain request", "{" + kBase + "}", true);
  expect_chat("system + user + assistant history",
              "{" + kModel +
                  R"(,"messages":[{"role":"system","content":"be brief"},{"role":"user","content":"hi"},)"
                  R"({"role":"assistant","content":"hello"},{"role":"user","content":"again"}]})",
              true);
  expect_chat("content parts", "{" + kModel + R"(,"messages":[{"role":"user","content":[{"type":"text","text":"hi"}]}]})",
              true);
  expect_chat("sampling parameters",
              "{" + kBase + R"(,"temperature":0.7,"top_p":0.9,"seed":1,"stop":["\n"],"n":1,"presence_penalty":0.1})",
              true);
  expect_chat("stream with usage", "{" + kBase + R"(,"stream":true,"stream_options":{"include_usage":true}})", true);
  expect_chat("reasoning_effort", "{" + kBase + R"(,"reasoning_effort":"low"})", true);
  expect("completions endpoint", "/v1/completions", "{" + kModel + R"(,"prompt":"hello"})", true);

  // values returned to the caller
  {
    auto o = validate("/v1/chat/completions", "{" + kBase + R"(,"max_tokens":64})");
    std::printf(o.model == "Qwen/Qwen3-8B" ? "ok   model is returned\n" : "FAIL model is returned: got %s\n",
                o.model.c_str());
    failures += o.model == "Qwen/Qwen3-8B" ? 0 : 1;
    expect_eq("max_tokens is returned", o.max_tokens, 64);
  }
  {
    auto o = validate("/v1/chat/completions", "{" + kBase + R"(,"max_completion_tokens":32})");
    expect_eq("max_completion_tokens is returned", o.max_tokens, 32);
  }

  // rejected requests
  expect_chat("not json", "hello", false);
  expect_chat("json array instead of object", "[1,2]", false);
  expect_chat("missing messages", "{" + kModel + "}", false);
  expect_chat("missing model", "{" + kMsgs + "}", false);
  expect_chat("messages not an array", "{" + kModel + R"(,"messages":{"role":"user"}})", false);
  expect_chat("unknown role", "{" + kModel + R"(,"messages":[{"role":"robot","content":"hi"}]})", false);
  expect_chat("message without role", "{" + kModel + R"(,"messages":[{"content":"hi"}]})", false);
  expect_chat("unknown top-level field", "{" + kBase + R"(,"definitely_not_openai":1})", false);
  expect_chat("temperature of wrong type", "{" + kBase + R"(,"temperature":"hot"})", false);
  expect_chat("stream of wrong type", "{" + kBase + R"(,"stream":"yes"})", false);
  expect("unsupported endpoint", "/v1/embeddings", "{" + kModel + R"(,"input":"x"})", false);

  std::printf(failures ? "%d test(s) FAILED\n" : "all tests passed\n", failures);
  return failures ? 1 : 0;
}
