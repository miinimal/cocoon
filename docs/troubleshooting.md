# Troubleshooting worker and client setup

Common errors reported in issues, with the cause and the fix given there by the maintainers or confirmed by the reporter.
Each entry links the issue for details.

## Worker

**`Check has_worker_hash(local_image_hash_unverified_) failed`** ([#69](https://github.com/TelegramMessenger/cocoon/issues/69), [#61](https://github.com/TelegramMessenger/cocoon/issues/61))
The TEE image hash of your worker is not in the root contract's allow-list, so the worker refuses to start. Only the
root owner can add a hash. Compare your `TEE_IMAGE_HASH` (printed at start, and by `health-client status`) with the
allowed ones from the chain; building from the `dev` branch and contacting the maintainers is the path described in #69.

**`Missing image.cmdline` / traceback when starting with `--test`** ([#27](https://github.com/TelegramMessenger/cocoon/issues/27))
The distributed worker release contains only the production image (`images/prod`). A test image has to be built:
`./scripts/build-image test`.

**`INVALID_CONFIG: no lite clients`, or TON sync never finishes** ([#30](https://github.com/TelegramMessenger/cocoon/issues/30), [#35](https://github.com/TelegramMessenger/cocoon/issues/35))
Use the Cocoon TON config: <https://cocoon.org/resources/mainnet.cocoon.global.config.json>.

**`nvidia-tdx.service` failed; OCSP request returns `403 Forbidden`** ([#30](https://github.com/TelegramMessenger/cocoon/issues/30))
GPU attestation needs the guest to reach `https://ocsp.ndis.nvidia.com/`. In #30 the same request failed from one network
and worked from another machine; check access from the guest with the test request given there. Inspect the service with
`./bin/health-client --instance worker logs nvidia-tdx.service`. Pulling the inference container after attestation can
take about half an hour.

**Which models can a worker serve?** ([#32](https://github.com/TelegramMessenger/cocoon/issues/32))
The root contract stores SHA-256 of the full model string `name@commit:verity-hash` (the `model` line of `worker.conf`,
printed by `scripts/build-model`). A model is accepted only if that hash is allowed (`model_hash_is_valid` get method).

## Host

**`./bin/seal-server: error while loading shared libraries: libsgx_dcap_ql.so.1`** ([#14](https://github.com/TelegramMessenger/cocoon/issues/14))
Install the Intel SGX DCAP packages from the Intel repository: `libsgx-dcap-ql libsgx-quote-ex libsgx-dcap-quote-verify
libtdx-attest-dev sgx-aesm-service` (Intel guide:
<https://cc-enabling.trustedservices.intel.com/intel-sgx-sw-installation-guide-linux/02/installation_instructions/>).

**`Failed to create SGX enclave : 0x2006`** ([#20](https://github.com/TelegramMessenger/cocoon/issues/20))
`SGX_ERROR_NO_DEVICE`: SGX is not enabled. Enable it in the BIOS and in Linux.

**`Failed to get SGX target info : 0xe019`** ([#31](https://github.com/TelegramMessenger/cocoon/issues/31))
`SGX_QL_NETWORK_ERROR` while retrieving PCK certificates: PCCS is not configured. See the Intel TDX enabling guide,
host OS setup: <https://cc-enabling.trustedservices.intel.com/intel-tdx-enabling-guide/05/host_os_setup/>.

**`mkosi was forbidden to unshare namespaces`** ([#10](https://github.com/TelegramMessenger/cocoon/issues/10))
The distribution restricts unprivileged user namespaces (Ubuntu 24.04). Run
`sysctl kernel.apparmor_restrict_unprivileged_userns=0` before building the image.

**`./scripts/build-image: venv/bin/activate: No such file or directory`** ([#9](https://github.com/TelegramMessenger/cocoon/issues/9))
A failed first attempt left a broken virtual environment. Remove `reprodebian/venv` and run `./scripts/build-image prod`
again (after `apt install python3-venv`).

## Hardware

- An NVIDIA GPU with Confidential Computing (Hopper, e.g. H100/H200) is required; consumer RTX cards do not support it
  ([#8](https://github.com/TelegramMessenger/cocoon/issues/8), [#19](https://github.com/TelegramMessenger/cocoon/issues/19)).
- AMD SEV-SNP hosts are supported, see [amd-tee.md](amd-tee.md) ([#6](https://github.com/TelegramMessenger/cocoon/issues/6)).
