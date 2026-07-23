# Aplo mining example

This example shows how to call the AploCoin mining contract from an embedded board.

Mining and staking use different units:

- mining rewards are accounted in Gaplo, the base unit of APLO
- staking locks APLO in the staking contract
- staking is only the level gate/multiplier; the base reward amount is calculated from gas spent

```text
1 APLO = 10^18 Gaplo
```

## Flow

1. Connect to WiFi.
2. Query the miner state with `miner_params(address)`.
3. Query staking state with `getStake(address)` and `getMultiplier(address)`.
4. Generate a random 32-byte mining nonce.
5. Hash the nonce with the current miner parameters.
6. If the hash is below the current difficulty, submit `mine(bytes32)`.
7. If the transaction succeeds, AploNode calculates the base Gaplo reward from gas spent, then applies the staking multiplier.

The `mine(bytes32)` call is a normal contract transaction. AploCoin uses two balances: native APLO for staking/value and Gaplo as the gas token. AploNode patches `eth_getBalance` to return Gaplo, so the example prints both native APLO (`balanceOf` on `0x...1235`) and Gaplo gas balance (`eth_getBalance` / `0x...1234`).

## Staking requirement

A wallet needs at least 1,000 APLO staked to receive Gaplo mining rewards.

| Staked APLO | Multiplier |
|-------------|------------|
| < 1,000     | 0x         |
| 1,000-1,999 | 1.0x       |
| 2,000-2,999 | 1.1x       |
| 3,000-3,999 | 1.2x       |
| 4,000-4,999 | 1.3x       |
| 5,000-5,999 | 1.4x       |
| 6,000-6,999 | 1.5x       |
| 7,000-7,999 | 1.6x       |
| 8,000+      | 1.7x       |

The base reward is not calculated from the staked amount. In `core/state_transition.go`, AploNode computes `gaploUsed = gasUsed * min(gasPrice, baseFee)`, `baseReward = gaploUsed / GAploRewardCoef`, then scales by the staking multiplier (`mult / 10`). Stake is therefore a level gate/multiplier only.

## Configuration

Edit this example source:

```text
examples/Aplo Mining/src/main.cpp
```

```cpp
const char *ssid = "YourNetworkName";
const char *password = "YourWiFiPassword";

// Public address is derived from PRIVATE_KEY at runtime.
const char *PRIVATE_KEY = "your64characterhexprivatekeywithout0xprefix";
```

Do not commit a real private key. The examples derive the sender address from `PRIVATE_KEY` with `Crypto::PrivateKeyToAddress()`, so you do not need to paste a separate public wallet address.

## Build

```bash
cd "examples/Aplo Mining"
platformio run -e esp32dev  # or esp32-c3-devkitm-1 / esp12e
platformio run --target upload
platformio device monitor
```

This is a standalone example: `platformio.ini` selects ESP32, ESP32-C3, or ESP8266, and all mining plus board-specific WiFi logic is visible in `src/main.cpp`. AploEmbed is installed from GitHub by PlatformIO.

## Output

A healthy run shows:

```text
=== AploEmbed Mining Example ===
WiFi connected
Mining Contract: 0x...
Staking Contract: 0x...
Staked Amount: 3000.00 APLO
Mining Multiplier: 1.2x
Miner Parameters:
  Last Block: ...
  Total Mined: ...
  Difficulty: ...
Current Block: ...
Cooldown complete, attempting to mine...
```

Finding a valid nonce is probabilistic. With the common `0x00ff...` target, roughly 1 in 256 random hashes should be valid. The example tries 128 nonces per cycle, then fetches fresh miner state on the next cycle so it does not spend long periods hashing an obsolete challenge.

Before broadcasting a found nonce, the example re-reads `miner_params(address)`, verifies that `last_block`, difficulty, `prev_hash`, and `total_mined` still match, and recomputes the proof locally. If another miner already changed the challenge, the stale nonce is discarded without spending gas. This is a best-effort client-side guard: the challenge can still change after the final check and before block inclusion, so the contract may still reject a raced transaction. A successful RPC broadcast is only pending execution; verify the transaction receipt before treating the mine as successful.

## Contract calls used

- `miner_params(address)` reads miner state
- `getStake(address)` reads staked APLO, returned in Gaplo units
- `getMultiplier(address)` reads the scaled multiplier, for example `12` means `1.2x`
- `mine(bytes32)` submits a valid mining nonce

`Web3::AploMine()` handles ABI encoding, transaction nonce lookup, gas price lookup, signing, and submission.

## Notes

- Mining has a per-address block cooldown.
- Mining consumes gas because it submits a transaction.
- The example is for embedded testing and integration, not high-throughput mining.
- Keep private keys out of source control.
