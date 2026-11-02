#!/bin/bash
# Starts a COCOON client and exposes its OpenAI-compatible endpoint on :10000.
#
# Required env:
#   OWNER_ADDRESS     TON address that owns the client contract (receives refunds/withdrawals)
#   NODE_WALLET_KEY   base64 Ed25519 private key of the client wallet (pays for requests)
# Optional env:
#   COCOON_NET              mainnet (default) | testnet
#   ROOT_CONTRACT_ADDRESS   required on testnet (your own root contract)
#   MODEL                   default model name for the client
#   TON_CONFIG_URL          override the TON global config source
set -euo pipefail

: "${OWNER_ADDRESS:?OWNER_ADDRESS is required}"
: "${NODE_WALLET_KEY:?NODE_WALLET_KEY is required}"

cd /opt/cocoon
args=(--type client --owner-address "$OWNER_ADDRESS" --node-wallet_key "$NODE_WALLET_KEY"
      --skip-build --build-dir /opt/cocoon/build --instance "${CLIENT_INSTANCE:-0}")

if [[ "${COCOON_NET:-mainnet}" == "testnet" ]]; then
  : "${ROOT_CONTRACT_ADDRESS:?ROOT_CONTRACT_ADDRESS is required on testnet}"
  [[ -f spec/testnet-full-ton-config.json ]] || ./scripts/fetch-testnet-ton-config
  args+=(--test --testnet)
else
  if [[ ! -f spec/mainnet-full-ton-config.json ]]; then
    curl -fsSL "${TON_CONFIG_URL:-https://cocoon.org/resources/mainnet.cocoon.global.config.json}" \
      -o spec/mainnet-full-ton-config.json
  fi
fi
[[ -n "${ROOT_CONTRACT_ADDRESS:-}" ]] && args+=(--root-contract-address "$ROOT_CONTRACT_ADDRESS")
[[ -n "${MODEL:-}" ]] && args+=(--model "$MODEL")

echo "Starting COCOON client on ${COCOON_NET:-mainnet}, owner ${OWNER_ADDRESS}"
exec python3 ./scripts/cocoon-launch "${args[@]}"
