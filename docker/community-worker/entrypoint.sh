#!/bin/bash
# Community worker: relay to the operator's model + one COCOON worker joining the proxies of a testnet root contract.
set -euo pipefail

: "${ROOT_CONTRACT_ADDRESS:?ROOT_CONTRACT_ADDRESS is required (testnet root contract of the network you join)}"
: "${OWNER_ADDRESS:?OWNER_ADDRESS is required (your TON address; worker earnings are paid to it)}"
: "${UPSTREAM_URL:?UPSTREAM_URL is required (your OpenAI-compatible model server, e.g. http://host.docker.internal:11434/v1)}"
MODEL="${MODEL:-Qwen/Qwen3-8B}"

# Worker wallet key: given, or generated once and kept in /data (back it up: it controls the worker wallet).
mkdir -p /data && chmod 700 /data
if [[ -z "${COCOON_LOCAL_WORKER_WALLET_KEY:-}" ]]; then
  if [[ ! -s /data/worker-wallet.key ]]; then
    umask 077; openssl rand -base64 32 > /data/worker-wallet.key
    echo "NEW worker wallet key saved to /data/worker-wallet.key (back it up)."
  fi
  COCOON_LOCAL_WORKER_WALLET_KEY="$(tr -d '\r\n' < /data/worker-wallet.key)"
fi
export COCOON_LOCAL_WORKER_WALLET_KEY

cd /opt/cocoon
python3 ./scripts/community-backend --port 8000 --model "$MODEL" --upstream "$UPSTREAM_URL" \
  ${UPSTREAM_MODEL:+--upstream-model "$UPSTREAM_MODEL"} &
RELAY=$!

for _ in $(seq 1 30); do
  if curl -fs -m 5 http://127.0.0.1:8000/health >/dev/null; then break; fi
  sleep 2
done
curl -fs -m 5 http://127.0.0.1:8000/health >/dev/null \
  || echo "warning: your model server at $UPSTREAM_URL does not answer yet; the worker starts anyway"

./scripts/fetch-testnet-ton-config || echo "warning: using bundled testnet config"

echo "Worker wallet address and balance: http://127.0.0.1:12000/stats (fund it with at least 2 testnet GRAM)."
python3 ./scripts/cocoon-launch --local-worker --testnet --skip-build --build-dir /opt/cocoon/build \
  --root-contract-address "$ROOT_CONTRACT_ADDRESS" --owner-address "$OWNER_ADDRESS" --model "$MODEL" &
WORKER=$!
wait -n "$RELAY" "$WORKER"
echo "a component exited; stopping"; kill "$RELAY" "$WORKER" 2>/dev/null || true; exit 1
