# Aplo mining example

This example shows how to call the AploCoin mining contract from an embedded board.

Mining and staking use different units:

- mining rewards are accounted in Gaplo, the base unit of APLO
- staking locks APLO in the staking contract
- the staked APLO amount controls the mining reward multiplier

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
7. If the transaction succeeds, the mining contract applies the Gaplo reward using the miner's staking multiplier.

The `mine(bytes32)` call is a normal contract transaction. The wallet still needs APLO for gas.

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

The reward itself is still a Gaplo-denominated reward. The multiplier only changes how much the mining contract credits for a successful mine.

## Configuration

Edit `main.cpp`:

```cpp
const char *ssid = "YourNetworkName";
const char *password = "YourWiFiPassword";

#define MY_ADDRESS "0xYourWalletAddress"
const char *PRIVATE_KEY = "your64characterhexprivatekeywithout0xprefix";
```

Do not commit a real private key.

## Build

```bash
cd "examples/Aplo Mining"
platformio run
platformio run --target upload
platformio device monitor
```

For ESP32-C3 serial settings, see `../../docs/platforms.md`.

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

Finding a valid nonce is probabilistic. Most cycles will not find one.

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
