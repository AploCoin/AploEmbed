# ABI and AploCoin contract helpers

AploEmbed contract calldata should be produced through the generic ABI encoder, not by hardcoding function selectors plus manually padded arguments.

## Generic encoding

Use `Contract::SetupContractData(signature, ...)` for both read-only calls and transactions:

```cpp
Contract contract(&web3, APLO_STAKING_CONTRACT);
std::string account = "0x...";

std::string data = contract.SetupContractData("getStake(address)", &account);
std::string result = contract.ViewCall(&data);
```

The function selector is derived from the signature at runtime using Keccak-256. This keeps helper code consistent and avoids stale hardcoded selectors.

## Supported argument families

The encoder supports the argument families used by AploEmbed examples and helpers:

- `address`
- `uint`, `uint8` ... `uint256`
- `int`
- `bool`
- dynamic `bytes`
- fixed `bytes1` ... `bytes32`
- `string`
- simple `uint[]`
- `struct` hex payloads

## Fixed bytes

Fixed bytes types are static ABI words. For example:

```cpp
std::string nonce = "0xfaf4025dec6f00cee0bd1f890a9983247462e002ba8b282384c522b5a3dc784d";
std::string data = contract.SetupContractData("mine(bytes32)", &nonce);
```

This must encode to:

```text
keccak256("mine(bytes32)")[:4] + 32-byte nonce
```

No dynamic `bytes` length prefix is used for `bytes32`.

## AploCoin built-in helpers

The high-level AploCoin helpers in `Web3` use the generic encoder internally:

```cpp
web3.AploGetAploBalance(&address);          // balanceOf(address)
web3.AploGetStake(&staking, &address);      // getStake(address)
web3.AploGetStakeMultiplier(&staking, &address);
web3.AploGetMinerParams(&mining, &address, &lastBlock, &difficulty, &totalMined, &prevHash);
web3.AploMine(&mining, &nonce, privateKey, &address);
```

Keep new helpers on the same path: instantiate `Contract`, call `SetupContractData(...)`, then call `ViewCall(...)` or `SendTransaction(...)`.

## Why selectors are not hardcoded

Hardcoded selectors are easy to mistype and make future ABI changes harder to audit. A previous mining issue was fixed by adding proper fixed `bytesN` support to the generic encoder; the final `AploMine()` path intentionally uses `SetupContractData("mine(bytes32)", ...)` instead of direct selector concatenation.

Tests assert that AploCoin helper selectors are not hardcoded in `src/Web3.cpp`.
