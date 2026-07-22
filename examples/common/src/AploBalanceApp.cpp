#include <Arduino.h>
#include <AploPlatform.h>
#include "ExampleWifi.h"
#include <Web3.h>
#include <AploContracts.h>
#include <Util.h>

using std::string;

static void beginSerial()
{
    Serial.begin(115200);
    delay(1000);
}

// WiFi credentials - replace with your network details
const char *ssid = "<YOUR_SSID>";
const char *password = "<YOUR_WIFI_PASSWORD>";

// Example AploCoin address - replace with your address
#define APLO_ADDRESS "0x742d35Cc6634C0532925a3b844Bc9e7595f0bEb"

Web3 web3Instance;
Web3 *web3 = &web3Instance;

void queryAploBalance();
void queryGaploBalance();

void AploBalanceAppSetup()
{
    beginSerial();
    Serial.println("\n\n=== AploEmbed Balance Query Example ===\n");
    
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
    
    // Query balance in both units
    queryAploBalance();
    queryGaploBalance();
}

void AploBalanceAppLoop()
{
    // Balance queries are typically done once or on-demand
    // You can add periodic queries here if needed
    delay(10000);
}

/**
 * Query balance in APLO (human-readable format)
 * 1 APLO = 10^18 Gaplo (similar to ETH/wei relationship)
 */
void queryAploBalance()
{
    Serial.println("--- Querying Balance in APLO ---");
    
    string address = APLO_ADDRESS;
    
    // Get balance as human-readable APLO string (18 decimals)
    string balanceAplo = web3->AploGetBalanceInAplo(&address);
    
    Serial.print("Address: ");
    Serial.println(address.c_str());
    Serial.print("Balance: ");
    Serial.print(balanceAplo.c_str());
    Serial.println(" APLO");
    Serial.println();
}

/**
 * Query balance in Gaplo (raw blockchain units / wei equivalent)
 * This is the native unit stored on the blockchain
 */
void queryGaploBalance()
{
    Serial.println("--- Querying Balance in Gaplo (wei) ---");
    
    string address = APLO_ADDRESS;
    
    // Get balance in Gaplo (wei) - raw blockchain units
    uint256_t balanceGaplo = web3->AploGetBalance(&address);
    
    Serial.print("Address: ");
    Serial.println(address.c_str());
    Serial.print("Balance: ");
    Serial.print(balanceGaplo.str().c_str());
    Serial.println(" Gaplo");
    
    // You can also convert manually using Util helper
    string balanceAplo = Util::ConvertWeiToEthString(&balanceGaplo, 18);
    Serial.print("Converted: ");
    Serial.print(balanceAplo.c_str());
    Serial.println(" APLO");
    Serial.println();
}
