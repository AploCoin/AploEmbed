#include "Contract.h"
#include "Crypto.h"
#include "Util.h"
#include <cassert>
#include <algorithm>
#include <cctype>
static std::string lowerAbi(std::string s) {std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return std::tolower(c);});return s;}
// Only network boundary is stubbed; ABI, transaction RLP and crypto are actual sources.
Web3::Web3(long long id) {chainId=id;}
Web3::~Web3() {}
long long Web3::getChainId() const {return chainId;}
std::string Web3::EthViewCall(const std::string*,const char*) {return "";}
std::string Web3::EthCall(const std::string*,const char*,long,long,const std::string*,const std::string*) {return "";}
std::string Web3::EthSendSignedTransaction(const std::string*,uint32_t) {return "";}
void legacyTests() {
    Contract abi(nullptr,"");
    std::string account="0x1111111111111111111111111111111111111111";
    std::string balance=abi.SetupContractData("balanceOf(address)",&account);
    assert(lowerAbi(balance)=="0x70a08231"+std::string(24,'0')+account.substr(2));
    uint256_t amount=Util::ConvertDecimalToWei("1.5",18);
    assert(amount==uint256_t(UINT64_C(1500000000000000000)));
    std::string stake=abi.SetupContractData("stake(uint256)",&amount);
    assert(stake.size()==74);assert(lowerAbi(stake.substr(10))==amount.str(16,64));
    std::string fixed="0x"+std::string(64,'a');assert(abi.SetupContractData("f(bytes32)",&fixed).substr(10)==fixed.substr(2));
    Web3 web3(28282);Contract ordinary(&web3,"");ordinary.SetPrivateKey("1111111111111111111111111111111111111111111111111111111111111111");
    uint256_t zero=0;std::string data="0x";std::string raw=ordinary.SignTransaction(0,1,21000,&account,&zero,&data);assert(raw.size()>100);
    // Legacy Sign bool semantics are unchanged; verify its actual signature bytes.
    Crypto crypto(nullptr);crypto.SetPrivateKey("1111111111111111111111111111111111111111111111111111111111111111");uint8_t digest[32]={1},signature[65];crypto.Sign(digest,signature);
    uint8_t secret[32];memset(secret,0x11,32);uint8_t publicKey[64];Crypto::PrivateKeyToPublic(secret,publicKey);assert(Crypto::Verify(publicKey,digest,signature));
}
