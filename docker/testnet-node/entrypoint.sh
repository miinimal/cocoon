#!/bin/bash
# Starts the testnet backend stub and the whole local network against your TON testnet root contract.
set -euo pipefail

: "${ROOT_CONTRACT_ADDRESS:?ROOT_CONTRACT_ADDRESS is required (your testnet root contract)}"
: "${OWNER_ADDRESS:?OWNER_ADDRESS is required}"
for c in PROXY WORKER CLIENT; do
  v="COCOON_LOCAL_${c}_WALLET_KEY"
  [[ -n "${!v:-}" ]] || { echo "$v is required (base64 32-byte key; fund its wallet with testnet coins)"; exit 1; }
done

# test GRAM comes from a faucet (2 per request): lower the node wallet thresholds (mainnet default 2.1 / 5.0)
export COCOON_MIN_WALLET_BALANCE_NANO="${COCOON_MIN_WALLET_BALANCE_NANO:-80000000}"
export COCOON_WARNING_WALLET_BALANCE_NANO="${COCOON_WARNING_WALLET_BALANCE_NANO:-200000000}"

cd /opt/cocoon
python3 ./scripts/testnet-backend --port 8000 --model "${MODEL:-Qwen/Qwen3-8B}" &

# refresh liteservers on start; keep the baked-in copy if the download fails
./scripts/fetch-testnet-ton-config || echo "warning: using bundled testnet config"

exec python3 ./scripts/cocoon-launch --local-all --testnet --skip-build --build-dir /opt/cocoon/build \
  --root-contract-address "$ROOT_CONTRACT_ADDRESS" --owner-address "$OWNER_ADDRESS"
