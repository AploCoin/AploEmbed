# Native session keys (AploEmbed 0.1.0)

`#include <SessionKeys.h>`; namespace `aplo`. Protocol source: AploNode `builtin/sessionkeys/registry.go`, ABI in `docs/session-keys.md`; compatibility checked against AploNode `addcc444e0934246d8fc8f73eaeb0efda7a2c050` and WebMiner's Go proof vector. This SDK changes no node protocol.

## Addresses, assets and numbers

Registry is `0x0000000000000000000000000000000000001237`. APLO is native value/stake, GAplo is an independent fee asset at `0x1234`; each uses 18 decimal places. `eth_getBalance`/`SessionKeys::Balance` reports GAplo. Query APLO `balanceOf(owner)` at `0x1235` for APLO.

`NativeTransaction` keeps chain ID, gas price and value as `uint256_t`; nonce/gas limit are `uint64_t`. `SessionPolicy` uses uint256 APLO/GAplo allowances and uint64 inclusive expiry. Zero APLO or GAplo budgets are valid protocol inputs; applications must choose useful limits. **The inherited uint256_t string constructor reads hexadecimal** (including strings without `0x`). Use a numeric constructor, validated RPC hex, or `Util::ConvertDecimalToWei(decimal,18)` for decimal amounts. Never pass decimal text directly to uint256_t.

Session nonce belongs to the session account, not the owner and not the mining challenge. `PendingNonce(signerAddress,nonce)` explicitly requests `pending`. Serialize submissions per signer. After uncertain network submission retain the signed transaction hash/nonce and reconcile receipt and pending nonce before retrying. The registry view exposes the mined account nonce; it may lag pending transactions.

## Key and signer ownership

`SessionKey` is noncopyable, zero-initialized, wiped on destruction/Clear, and RAM-only. `Generate()` uses existing `Crypto::RandomBytes`: ESP32 `esp_fill_random` or ESP8266 `os_get_random`, then rejection-samples valid secp256k1 scalars. Enable the WiFi radio before generation so the hardware entropy source is active. No micros/TinyMT key-generation fallback exists; unsupported platforms fail. The existing Trezor signing code uses RFC6979 and low-S signatures.

`Import(secret[32])` rejects zero/out-of-range scalars and clears the old key on failure. `Export(secret[32])` is explicit: the caller owns and must wipe that copy. There is no automatic EEPROM, NVS, logging or encrypted storage. A production device must decide whether reboot requires a fresh registration or uses authenticated secure persistence. A lost secret cannot be regenerated; revoke its authorization with the owner.

The owner secret need never reach the delegated device. `DigestSigner` is:

```cpp
bool signDigest(const uint8_t digest[32], uint8_t signature[65], void* context);
```

An external signer must review the transaction fields and sign raw Keccak digest, without personal-message or typed-data prefixes. Return `r || low-s || v`, v=0/1. `SignTransaction(tx,signer,context,expectedSigner,raw)` recovers the result and rejects a different signer, malformed/high-S signature or exhausted uint64 nonce. Proof hashing and transaction signing support the complete uint256 domain; EIP155 v is encoded as up to 33 bytes. Legacy EIP155 transaction format is used; type-2/access-list transactions are not added.

## Lifecycle

1. Select one trusted native endpoint: `Web3 rpc("https://native.example", "")`, then `SessionKeys sessions(rpc)`. An empty fallback avoids sending an isolated devnet request to a public chain. The legacy `Web3::getChainId()` remains fixed at 28282 for source compatibility; this new API uses live `eth_chainId`, including an active alternate domain. Retain the domain with the exact public policy.
2. Enable WiFi, generate a fresh `SessionKey`, then populate `SessionPolicy {key,target,selectors,aplo,gaplo,expiry}`. Selectors are ordered, distinct `std::array<uint8_t,4>`, 1..32. Validate current<=expiry<=current+1000; no overflow. The node additionally rejects used/funded/nonce-used/contract/reserved keys and non-EOA owners and enforces owner/expiry-bucket capacity.
3. `AcceptanceProof(policy,owner,chainId,key,proof)` signs exact raw `keccak256("APLO_SESSION_KEYS_ACCEPT_V1" || registry[20] || chainId[32] || owner[20] || abi.encode(key,target,selectors,aplo,gaplo,expiry))`. The ABI is the first six creation arguments; bytes4 are left aligned. `ProofHash` exposes the digest for tests/external key signers. `EncodeCreate(policy,proof,data)` encodes seven-argument `CreateSessionKey(address,address,bytes4[],uint256,uint256,uint256,bytes)`; no generic variadic bytes4[] support is implied.
4. `PrepareCreate(policy,owner,proof,gasLimit,tx)` reads head, live chain ID, pending OWNER nonce and gas price, validates the proof domain and constructs a direct zero-value registry transaction. Owner must sign it. Creation builtin gas is 400000 + 25000 per selector **plus intrinsic gas**. Estimate the owner call or use a justified bounded margin; 32 selectors need more than a one-selector example's 700000 limit.
5. Send the signed raw transaction with `SendRaw(raw,hash)`. An accepted hash is not success. `Receipt(hash,state)` returns Pending/Succeeded/Reverted only when SessionResult is Ok. On successful creation, `Read(key,owner,status)` verifies the six exact ABI words; active=false means absent/revoked/expiry cleanup (no distinction). Read simulations use OWNER as `from`; actual delegated execution uses SESSION as signer/caller, OWNER as origin/funder.
6. `SendDelegated(policy,owner,authorizedChainId,key,tx,hash)` checks local key/target/selector, pinned domain, live registry scope and remaining allowances, next-block expiry, maximum fee without uint256 multiplication overflow, calls estimateGas from SESSION (proves current selector permission), obtains pending SESSION nonce, signs and broadcasts. Fill tx.chainId with the retained domain, provide gas price/limit, value and calldata. The node remains authoritative; concurrent state changes can invalidate/revert a transaction after preflight. Rewards do not replenish allowances. Staking/miner metadata belong to the session caller; owner funds/reward multiplier remain separate.
7. `PrepareRevoke(key,owner,gasLimit,tx)` creates direct owner-signed zero-value `RevokeSessionKey(address)`. Revoke costs 300000 plus intrinsic gas. Sign/send/confirm as above. Expiry is inclusive, with cleanup after that block's rewards. Neither revoke nor expiry permits reuse; tombstones redirect later native APLO/canonical GAplo credits to owner. Incoming arbitrary ERC20/NFT storage does not follow that rule.

The external owner device can also use offline EncodeCreate/EncodeRevoke and SignTransaction, after independently checking chain ID, nonce, gas and policy. Owners can continue ordinary transfers and staking before/after delegation. No owner key is required for Read, proof generation, receipt polling or delegated signing.

## Checked RPC and transport

`SessionResult` carries `SessionError` plus a diagnostic. Errors distinguish InvalidArgument, TransportError, RpcError, InvalidResponse, SignerError and Unauthorized. Never interpret a failed result as zero balance, nonce zero, inactive session or a pending receipt. Output objects are usable only on success.

Typed methods: ChainId(uint256_t&), BlockNumber(uint64_t&), GasPrice(uint256_t&), PendingNonce(address,uint64_t&), Balance(address,uint256_t&), Call(from,to,data,hexOutput,blockTag="latest"), EstimateGas(from,to,data,value,uint64_t&), Read(key,owner,SessionStatus&), SendRaw(raw,hash), Receipt(hash,ReceiptState&). `Rpc`/`Request(method,params,rawResult)` validates JSON envelope; `Quantity(method,params,uint256_t&)` additionally requires canonical hex quantity. Low-level method/params are trusted caller JSON. Unknown node features surface as RpcError.

`SessionKeys(RpcTransport,void*)` supports host tests or an application transport. Callback `(method,params,context)` returns a complete JSON-RPC response; empty means transport failure. Existing Web3 TLS and CA behavior remain; there is no automatic insecure mode or HTTP enablement. For local HTTP devnet, host tests inject a loopback bridge, or firmware can use a trusted HTTPS reverse proxy.

## Memory and verification

No additional TLS client/crypto library or persistent secret store is introduced. Proof encoding streams one ABI word at a time into existing Keccak; it does not allocate a complete preimage. Creation calldata is 842 hex characters for one selector and 2826 for 32 selectors (including 0x). Separate unsigned/signed RLP allocations are released before transport; JSON/raw strings still consume dynamic heap. Registry view returns 192 bytes (386 hex characters), within the existing ESP8266 768-byte response cap. This is a design bound, not measured peak heap. Hardware WiFi/TLS/stack/fragmentation measurements remain necessary.

Run `python3 tests/run_native.py` for actual C++ session/Trezor/uint256 code with AddressSanitizer and UndefinedBehaviorSanitizer. `python3 tests/run_native.py --devnet` exercises a running disposable native loopback devnet with synthetic unlocked faucet and random ephemeral keys. No production wallet/device interaction is implied.

## JSON-RPC and ESP8266 receipt bounds

Buffered RPC and streamed receipts share `internal/JsonRpcParser`. It validates the complete JSON object, direct control fields and a maximum of 32 nested containers. Duplicate result/error/jsonrpc/id fields, including ASCII Unicode-escaped names, are invalid. If supplied, jsonrpc must be "2.0"; omitted version/id remain accepted for existing compact transport adapters. A non-null error object takes precedence over result. Escaped keys cannot bypass error or duplicate-status checks. Unknown log fields and long strings are skipped without retaining tokens. Numeric JSON fields support fraction/exponent grammar; RPC quantities still require canonical quoted hexadecimal integers.

Only direct result.status "0x0" or "0x1" confirms a receipt; result:null alone means Pending. Failed SessionResult leaves the caller's receipt state untouched and unusable as evidence. Invalid or truncated envelopes never mean Pending.

ESP8266 Web3 retains at most 768 response bytes in `RpcResponseBuffer`. Larger receipt bodies continue through the same parser and reduce to status/null, a generic RpcError, or an invalid envelope. Original error diagnostics are lost when oversized. Other oversized methods retain the existing empty-response transport failure. ESP32 keeps the ordinary full-body path. The reduction is only for transaction-receipt requests and cannot provide full receipt logs. The HTTP client expects a plain JSON body (not chunk framing); physical TLS, runtime heap and watchdog behavior require device testing.

Implementation ownership: `SessionKey.cpp` handles secret lifetime and secp256k1 signing/recovery; `SessionKeys.cpp` handles policy, proof/ABI and registry operations; `SessionTransaction.cpp` handles canonical EIP155 RLP; `SessionRpc.cpp` handles transport results and typed RPC; `SessionKeysWeb3.cpp` borrows Web3 and hardware entropy. Internal helpers carry no key storage, transport or additional crypto implementation.
