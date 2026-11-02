# MINIMALCULT testnet for COCOON (community, unofficial)

A public network running the COCOON protocol on **TON testnet**: build and test clients, agents and payment flows against the
real COCOON protocol and contracts with free test GRAM, before touching mainnet.

Telegram's official testnet root contract is not deployed on TON testnet, so this network runs its own
root contract with mainnet-like parameters. Maintained by [MINIMALCULT](https://github.com/miinimal);
not affiliated with or endorsed by Telegram. COCOON is Telegram's project, licensed under Apache-2.0.

## Status

Root contract deployed and proxy #1 registered; proxy #1 has deployed its contract with stake and its ports
11001/11002 are reachable from the internet. All node components (proxy, worker, client, key manager, router)
run and are synced with TON testnet; the worker, client and key manager wallets are being funded with test GRAM
(each needs at least 2 GRAM, a limit of the wallet contract). Every build of the `testnet` branch now passes an
end-to-end request (client, router, proxy, worker, backend, with token accounting) on a CI runner. Live state:
[Cocoon Explorer — Testnet](https://miinimal.github.io/cocoon-explorer/).

## Network

| | |
|---|---|
| Root contract | [`kQDw1ylF_89nG60ustm4vxrxMPx_chvPStTQs_0OqI1PunDa`](https://testnet.tonviewer.com/kQDw1ylF_89nG60ustm4vxrxMPx_chvPStTQs_0OqI1PunDa) |
| Proxy #1 | `165.227.61.128:11001` (workers), `165.227.61.128:11002` (clients) |
| Mode | test (`is_test`), TEE image hashes not enforced |
| Price | 0.00000002 GRAM per token (prompt x1, completion x8) — same as mainnet |
| Worker share | 95% |
| Min client stake / min proxy stake | 0.5 GRAM / 0.5 GRAM |
| Close delay (client and proxy) | 1 hour (mainnet: 1 day) — refund paths can be tested quickly |
| Models | test backend (`scripts/testnet-backend`), replies are labelled as testnet output |

Test GRAM: [@testgiver_ton_bot](https://t.me/testgiver_ton_bot) or `acton wallet airdrop` (2 GRAM per request).
Every node wallet (proxy, worker, client, key manager) needs at least 2 GRAM: the COCOON wallet contract
refuses to send below that (exit code 1035).

## Connect a client

```bash
# portable x86-64 binaries from this fork's releases
curl -fsSL <release tarball URL from the Releases page> | tar -xJ
./scripts/fetch-testnet-ton-config
./scripts/cocoon-launch --type client --test --testnet --skip-build --build-dir build \
  --root-contract-address kQDw1ylF_89nG60ustm4vxrxMPx_chvPStTQs_0OqI1PunDa \
  --owner-address <your testnet address> --node-wallet_key <base64 32-byte key>
# fund the client wallet shown on http://127.0.0.1:10000/stats, then:
curl http://127.0.0.1:10000/v1/chat/completions \
  -d '{"model":"Qwen/Qwen3-8B","messages":[{"role":"user","content":"hello"}]}'
```

Or run a whole node (proxy + worker + client) yourself with `docker/testnet-node`.

## What this fork adds

- `--testnet` for `cocoon-launch`, `--local-all --testnet` against your own root contract
- function calling (`tools`, `tool_choice`) and `response_format` accepted by the worker validator (#71)
- `scripts/client-stake-check`: read-only client stake inspection, warns before actions that lock the stake (#77)
- `scripts/testnet-backend`: OpenAI-compatible stub so GPU-less test workers complete requests end to end
- portable x86-64-v2 release builds, client and node containers, CI smoke test of the whole local network

Details: [docs/testnet.md](docs/testnet.md).

## Notes

Testnet only. Test GRAM has no value.
