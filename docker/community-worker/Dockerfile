# A COCOON community worker for TON testnet: worker + router (no VM, no GPU needed in the container) and
# scripts/community-backend, which relays inference to the operator's own OpenAI-compatible model server
# (Ollama, llama.cpp server, LM Studio, vLLM, SGLang). See COMMUNITY.md.
#
#   docker build -t cocoon-community-worker --build-arg RELEASE_URL=<testnet-build release tarball URL> docker/community-worker
#   docker run -d --name cocoon-worker --restart unless-stopped --add-host host.docker.internal:host-gateway \
#     -v cocoon-worker:/data -p 127.0.0.1:12000:12000 \
#     -e ROOT_CONTRACT_ADDRESS=<testnet root> -e OWNER_ADDRESS=<your TON address> \
#     -e UPSTREAM_URL=http://host.docker.internal:11434/v1 -e UPSTREAM_MODEL=qwen3:8b \
#     cocoon-community-worker
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

COPY entrypoint.sh /usr/local/bin/cocoon-community-worker
RUN chmod +x /usr/local/bin/cocoon-community-worker

VOLUME /data
# 12000: worker /stats page (wallet address, balance, served requests). Nothing needs to be reachable from outside:
# the worker dials the proxy.
EXPOSE 12000
ENTRYPOINT ["/usr/local/bin/cocoon-community-worker"]
