#include "SessionKeys.h"
#include "trezor/sha3.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace aplo;
using std::string;
static string bridge(const string& method, const string& params, void*) {
    std::cout<<"{\"method\":\""<<method<<"\",\"params\":"<<params<<"}"<<std::endl;
    string response;std::getline(std::cin,response);return response;
}
static void require(SessionResult r) {if(!r){std::cerr<<"SDK failure: "<<int(r.code)<<" "<<r.message<<std::endl;abort();}}
static string selector(const char* sig) {uint8_t hash[32];keccak_256((const uint8_t*)sig,strlen(sig),hash);const char* h="0123456789abcdef";string s="0x";for(int i=0;i<4;++i){s+=h[hash[i]>>4];s+=h[hash[i]&15];}return s;}
static void confirmed(SessionKeys& rpc,const string& hash) {
    for(int i=0;i<100;++i){ReceiptState state;require(rpc.Receipt(hash,state));if(state!=ReceiptState::Pending){assert(state==ReceiptState::Succeeded);return;}}
    assert(!"receipt timeout");
}
static string ownerSend(SessionKeys& rpc,SessionKey& owner,const string& to,const string& data,const uint256_t& value=0) {
    NativeTransaction tx;tx.to=to;tx.data=data;tx.value=value;tx.gasLimit=1200000;require(rpc.ChainId(tx.chainId));require(rpc.GasPrice(tx.gasPrice));require(rpc.PendingNonce(owner.Address(),tx.nonce));string raw,hash;require(SessionKeys::SignTransaction(tx,SessionKey::SignDigest,&owner,owner.Address(),raw));require(rpc.SendRaw(raw,hash));confirmed(rpc,hash);return hash;
}
int nativeIntegration() {
    SessionKeys rpc(bridge,nullptr);SessionKey owner,key;assert(owner.Generate());assert(key.Generate());
    string accounts;require(rpc.Rpc("eth_accounts","[]",accounts));size_t q=accounts.find('"');assert(q!=string::npos);string faucet=accounts.substr(q+1,42);
    string result;auto fund=[&](const string& to,const string& data,const string& value){require(rpc.Rpc("eth_sendTransaction","[{\"from\":\""+faucet+"\",\"to\":\""+to+"\",\"gas\":\"0x100000\",\"value\":\""+value+"\",\"data\":\""+data+"\"}]",result));assert(result.size()==68);confirmed(rpc,result.substr(1,66));};
    const string aplo="0x0000000000000000000000000000000000001235",gaplo="0x0000000000000000000000000000000000001234";
    const string recipient="0x5555555555555555555555555555555555555555";
    fund(owner.Address(),"0x","0x3635c9adc5dea00000");
    fund(gaplo,selector("transfer(address,uint256)")+string(24,'0')+owner.Address().substr(2)+uint256_t("0x3635c9adc5dea00000").str(16,64),"0x0");
    uint256_t gasBalance;require(rpc.Balance(owner.Address(),gasBalance));assert(gasBalance>0);
    string balance;require(rpc.Call(owner.Address(),aplo,selector("balanceOf(address)")+string(24,'0')+owner.Address().substr(2),balance));assert(uint256_t(balance)>0);
    // Ordinary native transaction remains usable before/after session registration.
    ownerSend(rpc,owner,recipient,"0x",7);
    SessionPolicy p;p.key=key.Address();p.target=aplo;p.selectors.push_back({{0xa9,0x05,0x9c,0xbb}});p.aplo=100;p.gaplo=uint256_t("0x3635c9adc5dea00000");
    uint256_t chain;require(rpc.ChainId(chain));uint64_t head;require(rpc.BlockNumber(head));p.expiry=head+200;
    string proof;require(SessionKeys::AcceptanceProof(p,owner.Address(),chain,key,proof));
    NativeTransaction registration;require(rpc.PrepareCreate(p,owner.Address(),proof,1200000,registration));assert(registration.value==0);assert(registration.chainId==chain);
    string raw,hash;require(SessionKeys::SignTransaction(registration,SessionKey::SignDigest,&owner,owner.Address(),raw));require(rpc.SendRaw(raw,hash));confirmed(rpc,hash);
    SessionStatus status;require(rpc.Read(p.key,owner.Address(),status));assert(status.active);assert(status.owner==owner.Address());assert(status.nonce==0);assert(status.aplo==100);
    NativeTransaction tx;tx.to=aplo;tx.chainId=chain;require(rpc.GasPrice(tx.gasPrice));tx.gasLimit=200000;tx.data=selector("transfer(address,uint256)")+string(24,'0')+recipient.substr(2)+uint256_t(11).str(16,64);
    NativeTransaction wrong=tx;wrong.to=gaplo;assert(rpc.SendDelegated(p,owner.Address(),chain,key,wrong,hash).code==SessionError::InvalidArgument);
    wrong=tx;wrong.data="0xffffffff";assert(rpc.SendDelegated(p,owner.Address(),chain,key,wrong,hash).code==SessionError::Unauthorized);
    require(rpc.SendDelegated(p,owner.Address(),chain,key,tx,hash));confirmed(rpc,hash);
    require(rpc.Read(p.key,owner.Address(),status));assert(status.nonce==1);assert(status.aplo==89);assert(status.gaplo<p.gaplo);
    require(rpc.Call(owner.Address(),aplo,selector("balanceOf(address)")+string(24,'0')+recipient.substr(2),balance));assert(uint256_t(balance)>=18);
    // Direct owner staking with a zero-value transaction and exact ABI amount.
    ownerSend(rpc,owner,aplo,selector("stake(uint256)")+(uint256_t(UINT64_C(1000000000000000000))*100).str(16,64));
    string stake;require(rpc.Call(owner.Address(),aplo,selector("getStake(address)")+string(24,'0')+owner.Address().substr(2),stake));assert(uint256_t(stake)>0);
    NativeTransaction revoke;require(rpc.PrepareRevoke(p.key,owner.Address(),500000,revoke));require(SessionKeys::SignTransaction(revoke,SessionKey::SignDigest,&owner,owner.Address(),raw));require(rpc.SendRaw(raw,hash));confirmed(rpc,hash);
    require(rpc.Read(p.key,owner.Address(),status));assert(!status.active);
    assert(rpc.SendDelegated(p,owner.Address(),chain,key,tx,hash).code==SessionError::Unauthorized);
    ownerSend(rpc,owner,recipient,"0x",3);
    std::cerr<<"native devnet PASS: separate balances, ordinary value, owner signer callback, register/read, delegated APLO allowance+session nonce, stake, revoke, inactive rejection; chain="<<chain.str()<<std::endl;
    return 0;
}
