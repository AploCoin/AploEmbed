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
        self.assertIn('headerState == 0x0d0a0d0aUL', web3)
        self.assertIn('node.rfind("https://", 0)', web3)
        self.assertIn('uint256_t zeroValue = 0;', web3)
        self.assertIn('&zeroValue,', web3)
        self.assertIn('architectures=esp32,esp8266', read('library.properties'))

    def test_esp8266_transport_uses_bounded_response_storage(self):
        header = read('src/Web3.h')
        web3 = read('src/Web3.cpp')
        self.assertIn('APLO_ESP8266_RPC_RESPONSE_MAX', header)
        self.assertIn('static WiFiClientSecure& getEsp8266Client()', web3)
        self.assertNotIn('esp8266ResponseBuffer', header)
        self.assertIn('result.reserve(APLO_ESP8266_RPC_RESPONSE_MAX);', web3)
        self.assertIn('HTTP response exceeds ESP8266 buffer', web3)
        self.assertIn('client = &getEsp8266Client();', web3)
        self.assertNotIn('WiFiClientSecure scopedClient;', web3.split('#if defined(ESP8266)', 1)[1].split('#else', 1)[0])
        self.assertIn('#else\n            result += c;\n#endif', web3)

    def test_rpc_endpoint_state_is_owned_and_does_not_use_strdup(self):
        header = read('src/Web3.h')
        web3 = read('src/Web3.cpp')
        self.assertIn('struct RpcEndpoint', header)
        self.assertIn('RpcEndpoint primaryEndpoint', header)
        self.assertIn('RpcEndpoint fallbackEndpoint', header)
        self.assertNotIn('strdup(', web3)
        self.assertNotIn('primaryRpcUrl = fallbackRpcUrl', web3)

    def test_web3_resources_have_raii_lifecycle(self):
        header = read('src/Web3.h')
        web3 = read('src/Web3.cpp')
        self.assertIn('~Web3();', header)
        self.assertIn('Web3::~Web3()', web3)
        self.assertNotIn('new (mem) WiFiClientSecure', web3)
        self.assertNotIn('BYTE *mem;', header)

    def test_contract_signing_path_has_no_per_transaction_leaks(self):
        header = read('src/Contract.h')
        contract = read('src/Contract.cpp')
        util_header = read('src/Util.h')
        util = read('src/Util.cpp')
        web3 = read('src/Web3.cpp')
        self.assertIn('~Contract();', header)
        self.assertIn('Contract::~Contract()', contract)
        self.assertIn('uint8_t hash[ETHERS_KECCAK256_LENGTH];', contract)
        self.assertIn('Crypto::Keccak256(encoded.data(), encoded.size(), hash);', contract)
        self.assertNotIn('string t = Util::VectorToString(&encoded);', contract)
        self.assertNotIn('vector<uint8_t> bytes(encodedTxBytesLength);', contract)
        self.assertNotIn('new uint8_t[', contract)
        self.assertNotIn('new string("0")', contract)
        self.assertIn('string zeroStr = "0";', contract)
        self.assertIn('RlpEncodeItemWithVector(const std::vector<uint8_t>& input)', util_header)
        self.assertIn('RlpEncodeItemWithVector(const vector<uint8_t>& input)', util)
        self.assertIn('output.reserve(input_len + 5);', util)
        self.assertIn('string paramStr;', contract)
        self.assertIn('vector<uint8_t> param = RlpEncodeForRawTransaction', contract)
        self.assertIn('paramStr = Util::VectorToString(&param);', contract)
        send_fn = web3[web3.index('string Web3::EthSendSignedTransaction'):web3.index('// -------------------------------\n// Private')]
        self.assertIn('string input;', send_fn)
        self.assertIn('input.reserve(data->size() + 80);', send_fn)
        self.assertNotIn('string p = "[\\\"" + *data + "\\\"]";', send_fn)
        self.assertIn('return getResult(&output);', send_fn)

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
        self.assertIn('Crypto::RandomBytes', keyid)
        self.assertNotIn('random_buffer(privateKeyBytes', keyid)

    def mining_sources(self):
        return [
            read('examples/Aplo Mining/ESP32/src/main.cpp'),
            read('examples/Aplo Mining/ESP32-C3/src/main.cpp'),
            read('examples/Aplo Mining/ESP8266/src/main.cpp'),
        ]

    def test_mining_hash_and_balance_regressions(self):
        util = read('src/Util.cpp')
        web3 = read('src/Web3.cpp')
        mining = read('examples/Aplo Mining/src/main.cpp')
        crypto_header = read('src/Crypto.h')
        crypto_impl = read('src/Crypto.cpp')
        self.assertIn('return ConvertBytesToHex(hash, 32);', util)
        self.assertNotIn('return "0x" + ConvertBytesToHex(hash, 32);', util)
        self.assertIn('stripHexPrefix(&h);', util)
        self.assertIn('AploGetAploBalance', web3)
        self.assertIn('AploGetGasBalance', web3)
        self.assertIn('SetupContractData("mine(bytes32)"', web3)
        self.assertIn('APLO Balance:', mining)
        self.assertIn('Gas Balance (GAPLO):', mining)
        self.assertIn('HASH_ATTEMPTS_PER_CYCLE 2000', mining)
        self.assertIn('Web3 web3Instance;', mining)
        self.assertIn('Web3 *web3 = &web3Instance;', mining)
        self.assertNotIn('web3 = new Web3();', mining)
        self.assertIn('static bool RandomBytes(uint8_t *buffer, size_t length);', crypto_header)
        self.assertIn('bool Crypto::RandomBytes(uint8_t *buffer, size_t length)', crypto_impl)
        self.assertIn('uint8_t packed[148];', mining)
        self.assertIn('uint8_t nonce[32];', mining)
        self.assertIn('uint8_t hash[32];', mining)
        self.assertIn('Crypto::Keccak256(packed, sizeof(packed), hash);', mining)
        self.assertIn('uint8_t totalMined[32];', mining)
        self.assertIn('memcpy(packed + 116, params.totalMined, 32);', mining)
        self.assertNotIn('(uint32_t)totalMined', mining)
        self.assertIn('bool decodeAddress(const char *address, uint8_t output[20])', mining)
        self.assertIn('if (!decodeAddress(address, addressBytes))', mining)
        self.assertIn('Util::ConvertHexToBytes(output, address + 2, 20);', mining)
        self.assertNotIn('Util::ConvertHexToBytes(output, address, 20);', mining)
        hot_loop = mining[mining.index('bool attemptMining'):mining.index('bool submitMineTransaction')]
        self.assertNotIn('String nonce', hot_loop)
        self.assertNotIn('String hash', hot_loop)
        self.assertNotIn('string packed', hot_loop)

    def test_esp8266_mining_serial_literals_stay_in_flash(self):
        mining = read('examples/Aplo Mining/src/main.cpp')
        self.assertNotIn('Serial.print("', mining)
        self.assertNotIn('Serial.println("', mining)
        self.assertIn('Serial.println(F("', mining)

    def test_esp8266_mining_defers_web3_until_after_wifi(self):
        mining = read('examples/Aplo Mining/src/main.cpp')
        self.assertIn('#if defined(ESP8266)\nWeb3 *web3 = nullptr;', mining)
        self.assertIn('static Web3 esp8266Web3Instance;', mining)
        wifi_connect = mining.index('while (!connectWifi(')
        web3_init = mining.index('static Web3 esp8266Web3Instance;')
        self.assertLess(wifi_connect, web3_init)
        self.assertNotIn('Heap before WiFi:', mining)
        self.assertNotIn('Heap after WiFi:', mining)
        self.assertNotIn('Heap after Web3:', mining)

    def test_esp8266_disables_ram_heavy_precomputed_curve_table(self):
        options = read('src/trezor/options.h')
        self.assertIn('#if defined(ESP8266)', options)
        self.assertIn('#define USE_PRECOMPUTED_CP 0', options)
        self.assertIn('#define USE_PRECOMPUTED_CP 1', options)

    def test_wei_string_conversion_is_bounds_safe(self):
        util = read('src/Util.cpp')
        conversion = util[util.index('string Util::ConvertWeiToEthString'):util.index('string Util::ConvertEthToWei')]
        self.assertNotIn('char buffer[65]', conversion)
        self.assertNotIn('memcpy(', conversion)
        self.assertIn('amount.insert(0,', conversion)
        self.assertIn('decimals < 0', conversion)
        self.assertIn('decimals > 77', conversion)

    def test_convert_to_wei_uses_bounded_formatting(self):
        util = read('src/Util.cpp')
        conversion = util[util.index('uint256_t Util::ConvertToWei'):util.index('string Util::ConvertWeiToEthString')]
        self.assertNotIn('sprintf(', conversion)
        self.assertIn('snprintf(', conversion)

    def test_examples_are_self_contained_and_use_github_dependency(self):
        self.assertFalse((ROOT / 'examples' / 'common').exists())
        expected_markers = {
            'Aplo Mining': ['attemptMining', 'submitMineTransaction', 'ensureWifiConnected'],
            'Aplo Balance Query': ['queryBalance'],
            'Aplo Send Transaction': ['sendAploToAddress', 'SEND_AMOUNT_APLO'],
            'Aplo Staking': ['queryStakingStatus', 'stakeAplo', 'STAKE_AMOUNT_APLO'],
        }
        for example, markers in expected_markers.items():
            root = ROOT / 'examples' / example
            self.assertTrue((root / 'platformio.ini').exists())
            self.assertTrue((root / 'src' / 'main.cpp').exists())
            ini = (root / 'platformio.ini').read_text()
            self.assertIn('[env:esp32dev]', ini)
            self.assertIn('[env:esp32-c3-devkitm-1]', ini)
            self.assertIn('[env:esp12e]', ini)
            self.assertIn('https://github.com/AploCoin/AploEmbed.git#master', ini)
            self.assertNotIn('file://', ini)
            source = (root / 'src' / 'main.cpp').read_text()
            self.assertIn('void setup()', source)
            self.assertIn('void loop()', source)
            self.assertNotIn('AploExampleApps', source)
            self.assertNotIn('BoardWifi.h', source)
            for marker in markers:
                self.assertIn(marker, source)

    def test_platform_specific_wifi_stays_inside_each_example(self):
        for example in [
            'Aplo Mining',
            'Aplo Balance Query',
            'Aplo Send Transaction',
            'Aplo Staking',
        ]:
            source = read(f'examples/{example}/src/main.cpp')
            self.assertIn('#if defined(ESP8266)', source)
            self.assertIn('onStationModeDisconnected', source)
            self.assertIn('wifiHandlerRegistered', source)
            self.assertIn('WiFi.config(IPAddress(0U), IPAddress(0U), IPAddress(0U))', source)
            self.assertIn('45000UL', source)
            self.assertNotIn('WiFi.disconnect(false)', source)
            self.assertIn('#else\n    WiFi.disconnect(true, true)', source)

    def test_examples_do_not_allocate_web3_dynamically_or_hang_forever(self):
        sources = [
            read('examples/Aplo Balance Query/src/main.cpp'),
            read('examples/Aplo Send Transaction/src/main.cpp'),
            read('examples/Aplo Staking/src/main.cpp'),
            read('examples/Aplo Mining/src/main.cpp'),
        ]
        for source in sources:
            self.assertNotIn('new Web3', source)
            self.assertNotIn('while (WiFi.status() != WL_CONNECTED)', source)

    def test_keyid_uses_raii_and_does_not_stall_on_eeprom_failure(self):
        header = read('src/KeyID.h')
        impl = read('src/KeyID.cpp')
        self.assertIn('BYTE privateKeyBytes[ETHERS_PRIVATEKEY_LENGTH];', header)
        self.assertIn('Crypto crypto;', header)
        self.assertNotIn('new BYTE[', impl)
        self.assertNotIn('new Crypto', impl)
        self.assertNotIn('delay(1000000)', impl)
        self.assertIn('if (!beginKeyStorage())', impl)
        self.assertIn('if (!EEPROM.commit())', impl)

    def test_rpc_url_parser_validates_ports_without_exceptions(self):
        web3 = read('src/Web3.cpp')
        self.assertNotIn('stoi(', web3)
        self.assertIn('parsePort', web3)
        self.assertIn('portValue > 65535UL', web3)
        self.assertIn("const size_t pathPos = node.find('/')", web3)
        self.assertIn("const size_t colonPos = authority.rfind(':')", web3)

    def test_get_string_has_no_debug_serial_output(self):
        web3 = read('src/Web3.cpp')
        get_string = web3[web3.index('string Web3::getString'):web3.index('/**', web3.index('string Web3::getString'))]
        self.assertNotIn('Serial.', get_string)

    def test_all_platform_examples_compile_against_local_sources(self):
        """Optional integration gate: APLO_RUN_PLATFORMIO_MATRIX=1 enables all 12 builds."""
        import os
        import subprocess

        if os.environ.get('APLO_RUN_PLATFORMIO_MATRIX') != '1':
            self.skipTest('set APLO_RUN_PLATFORMIO_MATRIX=1 to run PlatformIO build matrix')

        for example in [
            'Aplo Mining',
            'Aplo Balance Query',
            'Aplo Send Transaction',
            'Aplo Staking',
        ]:
            project_dir = ROOT / 'examples' / example
            for environment in ['esp32dev', 'esp32-c3-devkitm-1', 'esp12e']:
                with self.subTest(example=example, environment=environment):
                    subprocess.run(
                        ['uvx', '--from', 'platformio', 'platformio', 'run', '-e', environment],
                        cwd=project_dir,
                        check=True,
                        timeout=600,
                    )

    def test_remaining_runtime_audit_regressions_are_hardened(self):
        web3 = read('src/Web3.cpp')
        script = read('src/ScriptClient.cpp')
        contract = read('src/Contract.cpp')
        util = read('src/Util.cpp')
        crypto_header = read('src/Crypto.h')

        self.assertIn('HTTP RPC endpoints are not supported', web3)
        self.assertIn('if (parseIndex == std::string::npos', script)
        self.assertIn('if (endParseIndex == std::string::npos', script)
        self.assertIn('stripHexPrefix', contract)
        self.assertIn('isHexString', contract)
        self.assertIn('if (result == nullptr || result->empty())', util)
        self.assertIn('size_t input_len = input.size();', util)
        self.assertIn('size_t length', crypto_header)

    def test_money_examples_use_exact_decimal_strings(self):
        util_header = read('src/Util.h')
        util = read('src/Util.cpp')
        send = read('examples/Aplo Send Transaction/src/main.cpp')
        staking = read('examples/Aplo Staking/src/main.cpp')
        self.assertIn('ConvertDecimalToWei(const std::string& amount, int decimals)', util_header)
        self.assertIn('ConvertDecimalToWei(const string& amount, int decimals)', util)
        self.assertNotIn('ConvertToWei(aplo, 18)', send)
        self.assertNotIn('ConvertToWei(aplo, 18)', staking)
        self.assertIn('SEND_AMOUNT_APLO "0.01"', send)
        self.assertIn('STAKE_AMOUNT_APLO "1000"', staking)

    def test_hex_formatting_avoids_unbounded_alloca(self):
        util = read('src/Util.cpp')
        for function in ['PlainVectorToString', 'ConvertBytesToHex', 'ConvertBase']:
            start = util.index(function)
            end = util.find('\n}', start) + 2
            self.assertNotIn('alloca(', util[start:end])

    def test_package_metadata_exports_examples_without_build_artifacts(self):
        import json
        metadata = json.loads(read('library.json'))
        self.assertIn('espressif8266', metadata['platforms'])
        self.assertIn('examples', metadata['export']['include'])
        self.assertIn('docs', metadata['export']['include'])
        self.assertIn('examples/**/.pio/**', metadata['export']['exclude'])

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
            'examples/Aplo Mining/src/main.cpp',
            'examples/Aplo Send Transaction/src/main.cpp',
            'examples/Aplo Staking/src/main.cpp',
        ]:
            example = read(rel)
            self.assertNotIn('#define MY_ADDRESS', example)
            self.assertIn('Crypto::PrivateKeyToAddress(PRIVATE_KEY)', example)
            self.assertIn('string myAddress', example)


if __name__ == '__main__':
    unittest.main()
