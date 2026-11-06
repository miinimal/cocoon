# Answer receipts (draft RFC)

Status: draft, 2026-11-06. Author: Aleksandr Shcherbakov (@miinimal). Base: `dev` 541432c.

## Problem

A Cocoon client knows that its proxy runs an approved image: the RA-TLS handshake carries a TDX quote and the client
checks the image hash against the root contract. The client does not learn which worker produced an answer, and it has
nothing it can show to a third party. An application that serves Cocoon answers to its own users ("this reply was
computed confidentially on Cocoon") can only ask them to trust it.

## Goal

For any answer, the client can obtain a receipt that anyone can verify offline, without trusting the client, the proxy
or MINIMALCULT:

1. the answer bytes and the request bytes match the hashes in the receipt;
2. the receipt is signed by a key that lives inside a TEE;
3. the TEE runs a worker image that the root contract allows (`worker_hash_is_valid`) and a model it allows
   (`model_hash_is_valid`);
4. the quote is genuine (Intel DCAP / AMD SEV-SNP chain), so the key cannot be held outside the TEE.

No new trust is added: the receipt reuses the key the worker already proves in RA-TLS.

## What exists today (code references, `dev` 541432c)

- RA-TLS key: `tee/cocoon/Tee.cpp` `generate_cert_and_key()` creates an Ed25519 key (`tde2e_core::PrivateKey`) and
  binds its public key into the quote through `UserClaims` (`tee/cocoon/tdx/tdx.h`, `to_hash()` = SHA-512 of the
  serialized claims in `report_data`). Files: `<cert_base_name>_cert.pem`, `<cert_base_name>_key.pem`.
- Per-request final info: worker `create_final_info()` (`runners/worker/WorkerRunningRequest.cpp`) returns tokens,
  times and `worker_debug` (only when `enable_debug`); proxy forwards it (`ProxyRunningRequest::create_final_info`);
  the client exposes it in the body (`add_last_payload_part_with_debug`) and keeps it per `request_guid`
  (`ClientRunner::add_request_debug_info`, `http_get_request_debug_info`).
- Client-only request options are parsed and removed before forwarding: `enable_debug`, `request_guid`
  (`runners/helpers/ValidateRequest.cpp`).
- Root public keys (`root_add_public_key_signed`, `public_key_is_valid`) are key-manager keys for request encryption.
  On mainnet root EQCns7…U73k no key has been added (no such message in the root history on 2026-11-06), so receipts
  must not depend on them.

## Design

### Request

A new client-only option `"cocoon_receipt": true` (boolean). The client removes it from the body, as it does with
`enable_debug`, and sets a receipt bit in the query flags. The proxy forwards the bit. Old workers ignore the bit; the
client then reports `receipt: null` with reason `worker does not support receipts`.

### What the worker signs

```
receipt_v1 = {
  "v": 1,
  "model": "<model base name>",
  "request_sha256": "<hex: SHA-256 of the request body after decryption, exactly as the worker parsed it>",
  "answer_sha256": "<hex: SHA-256 of the concatenation of all answer payload parts the worker sent, in order>",
  "tokens": {"prompt": n, "cached": n, "completion": n, "reasoning": n},
  "worker_start": <unix float>, "worker_end": <unix float>,
  "root": "<root contract address from the runner config>",
  "image_hash": "<hex: the worker image hash>",
  "signer": "<hex: Ed25519 public key of the RA-TLS certificate>",
  "cert_sha256": "<hex: SHA-256 of the DER certificate>"
}
signature = Ed25519(RA-TLS private key, "cocoon-answer-receipt-v1\0" || canonical_json(receipt_v1))
```

Canonical JSON: keys sorted, no spaces, numbers as produced by nlohmann::json `dump()`. The domain prefix keeps the
signature from being valid for any other message signed with the same key.

### Delivery

- Worker: puts `{"receipt": receipt_v1, "signature": "<base64>", "cert": "<base64 DER, first answer on a connection
  only>"}` into `worker_debug` when the bit is set, independent of `enable_debug`.
- Client: stores it with the hash of the answer bytes it actually received from the proxy (before it adds its own
  debug object), served at `GET /request/receipt?request_guid=<hex>` next to the existing debug info. If the stored
  `answer_sha256` and the received bytes differ, the client marks the receipt `mismatch`.
- The body sent to the caller is unchanged, so existing integrations see no difference.

### Verification (`scripts/verify-receipt`, stdlib Python + dcap-qvl optional)

1. Recompute `request_sha256` and `answer_sha256` from the bytes the holder presents.
2. Check the Ed25519 signature with `signer`.
3. Parse the certificate (`cert_sha256` must match), extract the quote extension, verify it (Intel DCAP collateral or
   AMD chain), check that `report_data` equals SHA-512 of the serialized `UserClaims` with `signer`.
4. Compute the image hash from MRTD and RTMR0–3 (same rule as `scripts/image-hash-check`), compare with `image_hash`,
   and ask the root: `worker_hash_is_valid(image_hash)` and `model_hash_is_valid(model hash)`.
5. Report each check separately; a receipt is valid only if all pass.

## Open questions for the maintainers

- Should the worker sign with the RA-TLS key directly, or should the router sign on its behalf (the router holds the
  certificate)? Signing in the runner needs read access to `<cert_base_name>_key.pem` inside the guest.
- Is a query-flags bit acceptable, or should the option travel as a TL field in a new protocol version?
- Streaming: `answer_sha256` is over the parts as emitted by the worker after `AnswerPostprocessor`; the client must
  hash before it merges usage or debug into the stream (today it rewrites the last JSON object).

## Tests

- Unit: canonical JSON and signature round trip; tampered answer, request, tokens and signer are rejected.
- Network (`--local-all`, fake TEE): request with `cocoon_receipt`, fetch the receipt, verify signature and hashes;
  fake-TEE certificates are reported as `attestation: not a TEE` and never as valid.
- Quote path: verifier tested on recorded TDX quotes (dcap-qvl samples), as in Cocoon Explorer.
