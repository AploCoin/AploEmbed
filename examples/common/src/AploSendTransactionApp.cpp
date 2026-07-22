#include <Arduino.h>
#include <AploPlatform.h>
#include "ExampleWifi.h"
#include <Web3.h>
#include <Contract.h>
#include <Util.h>
#include <Crypto.h>

using std::string;

static void beginSerial()
{
    Serial.begin(115200);
    delay(1000);
}

// ============================================================================
// IMPORTANT SAFETY NOTES - READ BEFORE USE
// ============================================================================
//
// 1. NEVER commit your real private key to version control
// 2. Use TEST amounts only - start with small values (e.g., 0.01 APLO)
// 3. Verify recipient address carefully - transactions are irreversible
// 4. Test on a development/testnet environment first if available
// 5. Ensure sufficient balance for amount + gas fees
//
// ============================================================================

// WiFi credentials - replace with your network details
const char *ssid = "<YOUR_SSID>";
const char *password = "<YOUR_WIFI_PASSWORD>";

// Wallet configuration
// SECURITY: Replace with your actual private key. The sender address is derived
// from it at runtime, so there is no separate address value to keep in sync.
// WARNING: Keep your private key secret! Never share or commit it.
#define RECIPIENT_ADDRESS "0x007bEe82BDd9e866b2bd114780a47f2261C684E3"  // Recipient address

// CRITICAL: Replace with your actual 64-character hex private key (without 0x prefix)
// Demo key for documentation builds. Set a real 64-character private key from secure storage in production.
const char *PRIVATE_KEY = "0000000000000000000000000000000000000000000000000000000000000000";
string myAddress;

// Transaction parameters
// Amount to send in APLO (will be converted to Gaplo/wei internally)
#define SEND_AMOUNT_APLO "0.01"  // Exact decimal string; avoids floating-point money errors

// Gas parameters for AploCoin network
// Adjust these based on network conditions
#define GAS_PRICE 1000000000ULL    // 1 Gwei in wei
#define GAS_LIMIT 21000            // Standard transfer gas limit

Web3 web3Instance;
Web3 *web3 = &web3Instance;

void sendAploToAddress(const char *aplo, const char *destination);
uint256_t queryBalance(const char *address);

void AploSendTransactionAppSetup()
{
    beginSerial();
    Serial.println("\n\n=== AploEmbed Send Transaction Example ===\n");

    while (!exampleWifiConnect(ssid, password, 3, 20000)) {
        Serial.println("WiFi unavailable; retrying in 5 seconds...");
        delay(5000);
    }

    // Initialize Web3 with default AploCoin RPC endpoints
    // Uses pub1.aplocoin.com as primary, pub2.aplocoin.com as fallback
    // Web3 auto-selects the bundled root CA for HTTPS RPC endpoints.


    Serial.println("Web3 initialized with AploCoin RPC endpoints");
    Serial.println("Primary: pub1.aplocoin.com");
    Serial.println("Fallback: pub2.aplocoin.com");
    Serial.println("TLS: auto root CA resolution enabled\n");

    myAddress = Crypto::PrivateKeyToAddress(PRIVATE_KEY);

    uint256_t balance = queryBalance(myAddress.c_str());
    string balanceText = Util::ConvertWeiToEthString(&balance, 18);
    Serial.print("My Address: ");
    Serial.println(myAddress.c_str());
    Serial.print("Current Balance: ");
    Serial.print(balanceText.c_str());
    Serial.println(" APLO\n");

    const uint256_t sendAmount = Util::ConvertDecimalToWei(SEND_AMOUNT_APLO, 18);
    const uint256_t gasBuffer = Util::ConvertDecimalToWei("0.001", 18);
    const uint256_t requiredBalance = sendAmount + gasBuffer;

    Serial.print("Attempting to send: ");
    Serial.print(SEND_AMOUNT_APLO);
    Serial.println(" APLO");
    Serial.print("To Address: ");
    Serial.println(RECIPIENT_ADDRESS);
    Serial.println();

    // Safety check: ensure sufficient balance
    if (balance >= requiredBalance)
    {
        sendAploToAddress(SEND_AMOUNT_APLO, RECIPIENT_ADDRESS);
    }
    else
    {
        Serial.println("ERROR: Insufficient balance!");
        Serial.print("Required: ");
        string requiredText = Util::ConvertWeiToEthString(&requiredBalance, 18);
        Serial.print(requiredText.c_str());
        Serial.print(" APLO (");
        Serial.print(SEND_AMOUNT_APLO);
        Serial.println(" + 0.001 gas buffer)");
        Serial.print("Available: ");
        Serial.print(balanceText.c_str());
        Serial.println(" APLO");
        Serial.println("\nTransaction aborted for safety.");
    }
}

void AploSendTransactionAppLoop()
{
    // Transaction is sent once in setup()
    // Add periodic logic here if needed
    delay(10000);
}

/**
 * Query account balance in APLO
 * Returns balance as double for easy comparison
 */
uint256_t queryBalance(const char *address)
{
    string addr = address;
    return web3->AploGetBalance(&addr);
}

/**
 * Send APLO to a destination address
 *
 * @param aplo Amount in APLO (human-readable units)
 * @param destination Recipient address (0x...)
 */
void sendAploToAddress(const char *aplo, const char *destination)
{
    Serial.println("--- Preparing Transaction ---\n");

    // Create contract object for transaction signing
    // Empty address "" means this is a simple value transfer, not a contract call
    Contract contract(web3, "");
    contract.SetPrivateKey(PRIVATE_KEY);

    // Convert APLO to Gaplo (wei) - 18 decimals
    uint256_t valueGaplo = Util::ConvertDecimalToWei(aplo, 18);

    Serial.print("Amount: ");
    Serial.print(aplo);
    Serial.println(" APLO");
    Serial.print("Amount in Gaplo (wei): ");
    Serial.println(valueGaplo.str().c_str());
    Serial.println();

    // Get current nonce (transaction count) for the sender
    string myAddr = myAddress;
    Serial.print("Fetching nonce for: ");
    Serial.println(myAddr.c_str());
    uint32_t nonce = (uint32_t)web3->EthGetTransactionCount(&myAddr);
    Serial.print("Nonce: ");
    Serial.println(nonce);
    Serial.println();

    // Prepare transaction parameters
    string destinationAddr = destination;
    string emptyData = "";  // No data for simple transfers

    Serial.println("Transaction Parameters:");
    Serial.print("  From: ");
    Serial.println(myAddress.c_str());
    Serial.print("  To: ");
    Serial.println(destination);
    Serial.print("  Value: ");
    Serial.print(aplo);
    Serial.println(" APLO");
    Serial.print("  Gas Price: ");
    Serial.print((unsigned long)(GAS_PRICE / 1000000000ULL));
    Serial.println(" Gwei");
    Serial.print("  Gas Limit: ");
    Serial.println(GAS_LIMIT);
    Serial.print("  Nonce: ");
    Serial.println(nonce);
    Serial.println();

    // Sign and send transaction
    Serial.println("Signing and sending transaction...");
    string txResult = contract.SendTransaction(
        nonce,
        GAS_PRICE,
        GAS_LIMIT,
        &destinationAddr,
        &valueGaplo,
        &emptyData
    );

    // Parse transaction hash from result
    string txHash = web3->getString(&txResult);

    Serial.println("\n--- Transaction Result ---\n");

    if (txHash.length() > 0 && txHash != "0x")
    {
        Serial.println("SUCCESS! Transaction sent.");
        Serial.print("Transaction Hash: ");
        Serial.println(txHash.c_str());
        Serial.println();
        Serial.println("You can track this transaction on the AploCoin block explorer");
        Serial.println("(if available) using the transaction hash above.");
    }
    else
    {
        Serial.println("ERROR: Transaction failed!");
        Serial.println("Possible reasons:");
        Serial.println("  - Insufficient balance for amount + gas");
        Serial.println("  - Invalid recipient address");
        Serial.println("  - Network connectivity issues");
        Serial.println("  - RPC endpoint unavailable");
        Serial.println();
        Serial.print("Raw result: ");
        Serial.println(txResult.c_str());
    }

    Serial.println();
}
