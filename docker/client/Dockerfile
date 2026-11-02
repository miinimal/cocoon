# COCOON client in one container: OpenAI-compatible HTTP endpoint backed by the COCOON network.
#
#   docker build -t cocoon-client --build-arg RELEASE_URL=<testnet-build release tarball URL> docker/client
#   docker run -d -p 10000:10000 -e OWNER_ADDRESS=... -e NODE_WALLET_KEY=... cocoon-client
#
# The client needs no TEE hardware: it verifies proxy attestations and pays from its own wallet.
FROM ubuntu:24.04

ARG RELEASE_URL
RUN test -n "$RELEASE_URL" || (echo "RELEASE_URL build arg is required" && exit 1)

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates curl jq python3 xz-utils \
      libatomic1 libboost-thread1.83.0 libboost-atomic1.83.0 zlib1g \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/cocoon
RUN curl -fsSL "$RELEASE_URL" | tar -xJ -C /opt/cocoon

COPY entrypoint.sh /usr/local/bin/cocoon-client
RUN chmod +x /usr/local/bin/cocoon-client

ENV COCOON_NET=mainnet \
    CLIENT_INSTANCE=0
EXPOSE 10000
ENTRYPOINT ["/usr/local/bin/cocoon-client"]
