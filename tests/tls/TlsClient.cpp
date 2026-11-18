// Host boundary only. This does not emulate/certify ESP TLS implementations.
#include "WiFiClientSecure.h"
#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
unsigned liveAnchors = 0, maxAnchors = 0;
bool tlsMfln = true;
void WiFiClientSecure::receive() {
    if (received_ || !ssl_) {
        return;
    }
    received_ = true;
    if (SSL_write(ssl_, outgoing_.data(), int(outgoing_.size())) <= 0) {
        return;
    }
    char block[2048];
    int count;
    while ((count = SSL_read(ssl_, block, sizeof(block))) > 0) {
        incoming_.append(block, count);
    }
}
WiFiClientSecure::~WiFiClientSecure() {
    stop();
}
void WiFiClientSecure::setCACert(const char* pem) {
    pem_ = pem;
}
bool WiFiClientSecure::connect(const char* host, unsigned port) {
    tlsAttempts.push_back(host);
    context_ = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_verify(context_, SSL_VERIFY_PEER, nullptr);
    BIO* input = BIO_new_mem_buf(pem_, -1);
    X509* root;
    unsigned count = 0;
    while ((root = PEM_read_bio_X509(input, nullptr, nullptr, nullptr))) {
        X509_STORE_add_cert(SSL_CTX_get_cert_store(context_), root);
        X509_free(root);
        ++count;
    }
    BIO_free(input);
    if (count == 0) {
        return false;
    }
    ssl_ = SSL_new(context_);
    SSL_set_tlsext_host_name(ssl_, host);
    SSL_set1_host(ssl_, host);
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        return false;
    }
    SSL_set_fd(ssl_, socket_);
    return SSL_connect(ssl_) == 1;
}
void WiFiClientSecure::stop() {
    if (ssl_) {
        SSL_free(ssl_);
    }
    if (context_) {
        SSL_CTX_free(context_);
    }
    if (socket_ >= 0) {
        close(socket_);
    }
    outgoing_.clear();
    incoming_.clear();
    position_ = 0;
    received_ = false;
    ssl_ = nullptr;
    context_ = nullptr;
    socket_ = -1;
}
bool WiFiClientSecure::connected() {
    receive();
    return position_ < incoming_.size();
}
int WiFiClientSecure::available() {
    receive();
    return position_ < incoming_.size() ? int(incoming_.size() - position_) : -1;
}
int WiFiClientSecure::read() {
    return position_ < incoming_.size() ? static_cast<unsigned char>(incoming_[position_++]) : -1;
}
void WiFiClientSecure::print(const char* value) {
    outgoing_ += value;
}
void WiFiClientSecure::println(const char* value) {
    print(value);
    println();
}
void WiFiClientSecure::println() {
    outgoing_ += "\r\n";
}
