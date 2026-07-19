from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(rel):
    return (ROOT / rel).read_text()


class StaticRegressionTests(unittest.TestCase):
    def test_no_ambiguous_arduino_byte_casts(self):
        self.assertNotIn('byte(', read('src/KeyID.cpp'))
        self.assertNotIn('byte extract', read('src/Util.cpp'))

    def test_json_rpc_payloads_are_not_backslash_escaped(self):
        web3 = read('src/Web3.cpp')
        self.assertNotIn('\\\\\\\"jsonrpc', web3)
        self.assertNotIn('string p = "[\\\\\\\""', web3)
        self.assertIn('return "{\\\"jsonrpc\\\":\\\"2.0', web3)
        self.assertIn('[\\\"" + *address + "\\\",\\\"latest\\\"]', web3)

    def test_aplo_builtin_helpers_use_generic_abi_encoding(self):
        web3 = read('src/Web3.cpp')
        for selector in ['0x7a766460', '0xa9d637e1', '0xe31305ae', '0x70a08231', '0x2fdc505e']:
            self.assertNotIn(selector, web3)
        for signature in [
            'balanceOf(address)',
            'getStake(address)',
            'getMultiplier(address)',
            'miner_params(address)',
            'mine(bytes32)',
        ]:
            self.assertIn(f'SetupContractData("{signature}"', web3)

    def test_trezor_includes_match_lowercase_repository_case(self):
        self.assertTrue((ROOT / 'src' / 'trezor' / 'rand.h').exists())
        self.assertFalse((ROOT / 'src' / 'Trezor').exists())
        self.assertIn('#include "trezor/rand.h"', read('src/KeyID.h'))
        self.assertIn('#include "trezor/secp256k1.h"', read('src/Crypto.cpp'))
        self.assertIn('#include "trezor/ecdsa.h"', read('src/Crypto.cpp'))

    def test_trezor_rfc6979_is_compiled_once(self):
        self.assertTrue((ROOT / 'src' / 'trezor' / 'rfc6979.c').exists())
        self.assertNotIn('#include "rfc6979.c"', read('src/trezor/ecdsa.c'))
        self.assertIn('#include "rfc6979.h"', read('src/trezor/ecdsa.c'))

    def test_cert_bundle_size_overload_exists_for_esp32_core_3(self):
        header = read('src/Web3.h')
        impl = read('src/Web3.cpp')
        self.assertIn('setCertificateBundle(const uint8_t* bundle_start, size_t bundle_size)', header)
        self.assertIn('certBundleSize', header)
        self.assertIn('setCACertBundle(certBundle, certBundleSize)', impl)

    def test_esp32_and_http_runtime_regressions(self):
        web3 = read('src/Web3.cpp')
        self.assertIn('client->print(data->c_str())', web3)
        self.assertIn('if (line == "\\r")', web3)
        self.assertIn('node.rfind("https://", 0)', web3)
        self.assertIn('uint256_t zeroValue = 0;', web3)
        self.assertIn('&zeroValue,', web3)
        self.assertIn('architectures=esp32', read('library.properties'))

    def test_json_result_parsing_is_centralized_in_tag_reader(self):
        tag_reader = read('src/TagReader/TagReader.cpp')
        web3 = read('src/Web3.cpp')
        util = read('src/Util.cpp')
        self.assertIn('size_t TagReader::parse(char *jsonBuffer)', tag_reader)
        self.assertIn("*p = '\\0';", tag_reader)
        self.assertIn('TagReader reader;', web3)
        self.assertIn('return reader.getTag(json, "result");', web3)
        self.assertIn('TagReader reader;', util)

    def test_web3e_issue_regressions_are_hardened(self):
        web3 = read('src/Web3.cpp')
        contract = read('src/Contract.cpp')
        keyid = read('src/KeyID.cpp')
        self.assertIn('if (lengthIndex <= 32)', web3)
        self.assertIn('index < static_cast<int>(v->size())', web3)
        self.assertIn('isSizedUintType', contract)
        self.assertIn('GenerateBytesForUintPointer', contract)
        self.assertIn('uintBits / 8', contract)
        self.assertIn('isFixedBytesType', contract)
        self.assertIn('GenerateBytesForFixedBytes', contract)
        self.assertIn('esp_fill_random', keyid)
        self.assertIn('os_get_random', keyid)
        self.assertNotIn('random_buffer(privateKeyBytes', keyid)

    def test_mining_hash_and_balance_regressions(self):
        util = read('src/Util.cpp')
        web3 = read('src/Web3.cpp')
        mining = read('examples/Aplo Mining/main.cpp')
        self.assertIn('return ConvertBytesToHex(hash, 32);', util)
        self.assertNotIn('return "0x" + ConvertBytesToHex(hash, 32);', util)
        self.assertIn('stripHexPrefix(&h);', util)
        self.assertIn('AploGetAploBalance', web3)
        self.assertIn('AploGetGasBalance', web3)
        self.assertIn('SetupContractData("mine(bytes32)"', web3)
        self.assertIn('APLO Balance:', mining)
        self.assertIn('Gas Balance (GAPLO):', mining)
        self.assertIn('HASH_ATTEMPTS_PER_CYCLE 2000', mining)

    def test_examples_keep_only_default_esp32_platformio_env(self):
        for rel in [
            'examples/Aplo Mining/platformio.ini',
            'examples/Aplo Balance Query/platformio.ini',
            'examples/Aplo Send Transaction/platformio.ini',
            'examples/Aplo Staking/platformio.ini',
        ]:
            ini = read(rel)
            self.assertIn('[env:esp32dev]', ini)
            self.assertNotIn('[env:esp32-c3-devkitm-1]', ini)
            self.assertNotIn('[env:esp12e]', ini)

    def test_platform_specific_docs_exist(self):
        for rel in [
            'docs/platformio-esp32.md',
            'docs/platformio-esp32-c3.md',
            'docs/platformio-esp8266.md',
        ]:
            self.assertTrue((ROOT / rel).exists())
        self.assertIn('[env:esp32-c3-devkitm-1]', read('docs/platformio-esp32-c3.md'))
        self.assertIn('[env:esp12e]', read('docs/platformio-esp8266.md'))

    def test_examples_derive_sender_address_from_private_key(self):
        crypto_header = read('src/Crypto.h')
        crypto_impl = read('src/Crypto.cpp')
        self.assertIn('PrivateKeyToAddress(const char *privateKey)', crypto_header)
        self.assertIn('PrivateKeyToPublic(privateKeyBytes, publicKey)', crypto_impl)
        self.assertIn('PublicKeyToAddress(publicKey, address)', crypto_impl)
        for rel in [
            'examples/Aplo Mining/main.cpp',
            'examples/Aplo Send Transaction/main.cpp',
            'examples/Aplo Staking/main.cpp',
        ]:
            example = read(rel)
            self.assertNotIn('#define MY_ADDRESS', example)
            self.assertIn('Crypto::PrivateKeyToAddress(PRIVATE_KEY)', example)
            self.assertIn('string myAddress', example)


if __name__ == '__main__':
    unittest.main()
