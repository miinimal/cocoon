# A complete COCOON network node for TON testnet in one container:
# proxy + worker + client + key manager (local, no VMs) + testnet-backend (OpenAI-compatible stub, no GPU).
#
#   docker build -t cocoon-testnet-node --build-arg RELEASE_URL=<testnet-build release tarball URL> docker/testnet-node
#   docker run -d --name cocoon-testnet --env-file node.env \
#     -p 11001:11001 -p 11002:11002 -p 127.0.0.1:10100:10000 cocoon-testnet-node
#
# node.env: ROOT_CONTRACT_ADDRESS, OWNER_ADDRESS and COCOON_LOCAL_{PROXY,WORKER,CLIENT,KEY_MANAGER}_WALLET_KEY.
# The proxy address registered in the root contract must point at this host's ports 11001/11002.
FROM ubuntu:24.04

ARG RELEASE_URL
RUN test -n "$RELEASE_URL" || (echo "RELEASE_URL build arg is required" && exit 1)

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates curl jq python3 xz-utils openssl \
      libatomic1 libboost-thread1.83.0 libboost-atomic1.83.0 zlib1g \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/cocoon
RUN curl -fsSL "$RELEASE_URL" | tar -xJ -C /opt/cocoon \
 && ./scripts/fetch-testnet-ton-config \
 && ./scripts/fetch-sev-pki /etc/tee/sev

COPY entrypoint.sh /usr/local/bin/cocoon-testnet-node
RUN chmod +x /usr/local/bin/cocoon-testnet-node

# 11001/11002: proxy (workers/clients), 10000: client OpenAI-compatible API, 11000/12000/10000/13000: /stats pages
EXPOSE 11001 11002 10000 11000 12000 13000
ENTRYPOINT ["/usr/local/bin/cocoon-testnet-node"]
