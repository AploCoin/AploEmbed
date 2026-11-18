#ifndef APLOEMBED_CONTRACTS_H
#define APLOEMBED_CONTRACTS_H

// AploCoin Chain ID (from aplonode/params/config.go)
#define APLO_ID 28282

// AploCoin Smart Contract Addresses
// Native Aplo builtin addresses; verify the selected chain genesis.

// Canonical GAplo token (also exposes mining methods consumed by MiningEmbed).
#define APLO_GAPLO_CONTRACT "0x0000000000000000000000000000000000001234"
// Compatibility address alias; the token remains part of the base SDK.
#define APLO_MINING_CONTRACT APLO_GAPLO_CONTRACT
#define APLO_SESSION_REGISTRY "0x0000000000000000000000000000000000001237"

// Staking contract address (handles stake/unstake/getStake/getMultiplier)
#define APLO_STAKING_CONTRACT "0x0000000000000000000000000000000000001235"

// Minimum stake amount in APLO (expressed in APLO base units at runtime)
#define APLO_MIN_STAKE_AMOUNT 1000

// Helper function to get mining contract address
inline const char* getAploMiningContract() {
    return APLO_MINING_CONTRACT;
}

// Helper function to get staking contract address
inline const char* getAploStakingContract() {
    return APLO_STAKING_CONTRACT;
}

#endif // APLOEMBED_CONTRACTS_H
