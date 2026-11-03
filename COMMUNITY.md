# COCOON community workers (testnet)

Serve a real open model to the [MINIMALCULT COCOON testnet](TESTNET.md) from your own machine: a laptop with
Ollama, a desktop with LM Studio, or a GPU server with vLLM. No confidential VM, no image build: one container
joins the network and relays requests to the model server you already run.

```
client ──▶ proxy (165.227.61.128) ◀── your worker ──▶ community-backend ──▶ your Ollama / llama.cpp / vLLM
                                     (container)       (127.0.0.1:8000)     (your model)
```

## Why

Public COCOON on mainnet has had no worker supply since August 2026: running a mainnet worker requires an
Intel TDX host with a GPU and an image Telegram signs. This mode lets the protocol, contracts and payment flow
run with real models today, on hardware people already have, and gives builders a live network to integrate
against.

## Trust model — read this first

- **No TEE.** A community worker is not confidential: its operator can read the prompts it serves. Do not send
  private data through community workers. Mainnet COCOON keeps its TDX guarantees; this mode never touches it.
- The network runs on **TON testnet**: worker earnings are test GRAM. Any real-money layer on top must be
  announced separately by whoever runs it.
- The worker connects outwards to the proxy; nothing on your machine needs to be reachable from the internet.

## Join in four steps

1. Run a model server with an OpenAI-compatible API, for example `ollama pull qwen3:8b && ollama serve`
   (API at `http://127.0.0.1:11434/v1`). The network model is `Qwen/Qwen3-8B`; serve that model or the closest
   quantized build of it.
2. Build the container from a release of this fork (Releases page, `testnet-build-*` tarball):

   ```bash
   docker build -t cocoon-community-worker --build-arg RELEASE_URL=<release tarball URL> docker/community-worker
   ```

3. Start it:

   ```bash
   docker run -d --name cocoon-worker --restart unless-stopped \
     --add-host host.docker.internal:host-gateway -v cocoon-worker:/data -p 127.0.0.1:12000:12000 \
     -e ROOT_CONTRACT_ADDRESS=kQDw1ylF_89nG60ustm4vxrxMPx_chvPStTQs_0OqI1PunDa \
     -e OWNER_ADDRESS=<your TON testnet address> \
     -e UPSTREAM_URL=http://host.docker.internal:11434/v1 -e UPSTREAM_MODEL=qwen3:8b \
     cocoon-community-worker
   ```

   The first start creates a worker wallet key in the `cocoon-worker` volume (`/data/worker-wallet.key`).
4. Open `http://127.0.0.1:12000/stats`, copy the worker wallet address and send it at least 2 test GRAM
   ([@testgiver_ton_bot](https://t.me/testgiver_ton_bot)). The worker registers with the proxy and starts
   serving.

Without Docker: `scripts/community-backend --upstream <url> --upstream-model <name> &` and
`scripts/cocoon-launch --local-worker --testnet --root-contract-address ... --owner-address ... --model Qwen/Qwen3-8B`
with `COCOON_LOCAL_WORKER_WALLET_KEY` set.

## community-backend

`scripts/community-backend` listens on 127.0.0.1:8000 (where the worker forwards inference) and relays to any
OpenAI-compatible server:

- rewrites the model name both ways, so clients always see the network model name;
- passes chat, completions, streaming and function calling through;
- always returns `usage` (asks the upstream for it in streams, estimates if the upstream has none), so the
  worker's token accounting and payments always have numbers;
- refuses bodies over 4 MB, never follows upstream redirects, answers 502/503 instead of hanging when the
  model server is down.

Tests: `python3 test/test_tools.py` (relay against a stand-in upstream).

---
Maintained by [MINIMALCULT](https://github.com/miinimal); not affiliated with or endorsed by Telegram.
COCOON is Telegram's project, licensed under Apache-2.0.
