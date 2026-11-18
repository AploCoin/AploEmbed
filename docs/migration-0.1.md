# Migration to 0.1.0

AploEmbed remains the base Arduino SDK: transport/TLS, Crypto, generic ABI including bytes32, canonical GAplo at 0x1234, APLO balances, ordinary transactions, staking and native sessions.

| Previous entry point | Destination |
|---|---|
| Web3::AploGetMinerParams | MiningEmbed::readParams with owner/session context |
| Web3::AploMine | MiningEmbed::submit with restricted session, live domain/nonce, estimate and receipt tracking |
| examples/Aplo Mining | FirmwareEmbed standalone projects under Firmware/ |
| mining-loop/source regressions | MiningEmbed portable/facade tests and firmware verification |
| APLO_MINING_CONTRACT / getAploMiningContract | Retained compatibility address aliases for GAplo at 0x1234; prefer APLO_GAPLO_CONTRACT |

The migration intentionally requires a new mining authorization model. Import a restricted session key into firmware RAM, retain public owner/domain/budget/expiry, and keep owner registration/revocation signing on a separate device. There is no silent owner-signing fallback.

Use the separate MiningEmbed checkout and the desired board directory in FirmwareEmbed. Each board project pins the AploEmbed and MiningEmbed GitHub revisions in platformio.ini. Its README covers library setup, RPC trust, build and upload.
