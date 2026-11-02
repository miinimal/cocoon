# Running COCOON on TON Testnet

COCOON components already support TON testnet internally (`is_testnet` in every runner config,
testnet-aware address handling and tonlib initialisation). This document describes the launcher
support that makes it usable without editing spec files by hand.

Testnet is allowed only together with test mode (`--test`). The runners enforce this: the key manager
refuses `is_testnet` outside of test mode.

## 1. Get the testnet TON config

```bash
./scripts/fetch-testnet-ton-config
```

This downloads the official TON testnet global config and writes:

- `spec/testnet-base-ton-config.json` — network constants (DHT, zero state), without liteservers
- `spec/testnet-full-ton-config.json` — full config including liteservers

Override the source with `TON_TESTNET_CONFIG_URL` if you use your own liteservers.

## 2. Launch components on testnet

```bash
./scripts/cocoon-launch --test --testnet scripts/proxy.conf
./scripts/cocoon-launch --test --testnet scripts/worker.conf
./scripts/cocoon-launch --test --testnet scripts/client.conf
```

`--testnet` can also be set in the config file (`testnet = true` in the `[node]` section).

What `--testnet` changes:

| Setting | Mainnet (default) | `--testnet` |
|---|---|---|
| `is_testnet` in runner configs | `0` | `1` (via `IS_TESTNET` runtime var) |
| Base TON config | `spec/mainnet-base-ton-config.json` | `spec/testnet-base-ton-config.json` |
| Full TON config | `spec/mainnet-full-ton-config.json` | `spec/testnet-full-ton-config.json` |

Explicit `--ton-config-base` / `--ton-config` still take precedence.

## 3. Root contract

The mainnet root contract address is the default and does not exist on testnet. Deploy your own root
contract to testnet with [cocoon-contracts](https://github.com/TelegramMessenger/cocoon-contracts),
list your proxy addresses in it, and pass it with `--root-contract-address`.

Fund the proxy, worker and client wallets with testnet TON (see each component's HTTP stats for the
wallet addresses).

## 4. Prebuilt binaries

The `testnet-build` workflow builds `cocoon-all` on a GitHub-hosted runner, validates the testnet
launcher path and publishes the binaries as a prerelease. This is useful for machines without enough
memory to compile TON.

## Notes

- Test mode does not verify TEE image hashes. Never expose a test-mode deployment publicly as if it
  were production.
- Fake-TON (`--fake-ton`) and `--local-all` are unchanged.

## Checking a client stake before a refund

`scripts/client-stake-check <client_contract_address> [--testnet]` prints the client contract state,
stake and prepaid balance from public chain data and warns before actions that can lock the stake
(issue #77). It is read-only: nothing is signed or sent.
