// AploEmbed Utilities
//
// Specialized for AploCoin blockchain
// Based on Web3E by James Brown (@JamesSmartCell, @AlphaWallet)
// Original Web3 Arduino by Okada, Takahiro
//

#include <Util.h>
#include <Crypto.h>
#include "TagReader/TagReader.h"
#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <vector>

using std::string;
using std::vector;


static string getJsonResultValue(const string* json) {
    if (json == nullptr) return string("");
    TagReader reader;
    return reader.getTag(json, "result");
}
static const char * _aploembed_hexStr = "0123456789ABCDEF";
// returns output (header) length
uint32_t Util::RlpEncodeWholeHeader(uint8_t* header_output, uint32_t total_len) {
    if (total_len < 55) {
        header_output[0] = (uint8_t)0xc0 + (uint8_t)total_len;
        return 1;
    } else {
        uint8_t tmp_header[8];
        memset(tmp_header, 0, 8);
        uint32_t hexdigit = 1;
        uint32_t tmp = total_len;
        while ((uint32_t)(tmp / 256) > 0) {
            tmp_header[hexdigit] = (uint8_t)(tmp % 256);
            tmp = (uint32_t)(tmp / 256);
            hexdigit++;
        }
        tmp_header[hexdigit] = (uint8_t)(tmp);
        tmp_header[0] = (uint8_t)0xf7 + (uint8_t)hexdigit;

        // fix direction for header
        uint8_t header[8];
        memset(header, 0, 8);
        header[0] = tmp_header[0];
        for (int i=0; i<hexdigit; i++) {
            header[i+1] = tmp_header[hexdigit-i];
        }

        memcpy(header_output, header, (size_t)hexdigit+1);
        return hexdigit+1;
    }
}

vector<uint8_t> Util::RlpEncodeWholeHeaderWithVector(uint32_t total_len) {
    vector<uint8_t> header_output;
    if (total_len < 55) {
        header_output.push_back((uint8_t)0xc0 + (uint8_t)total_len);
    } else {
        vector<uint8_t> tmp_header;
        uint32_t hexdigit = 1;
        uint32_t tmp = total_len;
        while ((uint32_t)(tmp / 256) > 0) {
            tmp_header.push_back((uint8_t)(tmp % 256));
            tmp = (uint32_t)(tmp / 256);
            hexdigit++;
        }
        tmp_header.push_back((uint8_t)(tmp));
        tmp_header.insert(tmp_header.begin(), 0xf7 + (uint8_t)hexdigit);

        // fix direction for header
        vector<uint8_t> header;
        header.push_back(tmp_header[0]);
        for (int i=0; i<tmp_header.size()-1; i++) {
            header.push_back(tmp_header[tmp_header.size()-1-i]);
        }

        header_output.insert(header_output.end(), header.begin(), header.end());
    }
    return header_output;
}


// returns output length
uint32_t Util::RlpEncodeItem(uint8_t* output, const uint8_t* input, uint32_t input_len) {
    if (input_len==1 && input[0] == 0x00) {
        uint8_t c[1] = {0x80};
        memcpy(output, c, 1);
        return 1;
    } else if (input_len==1 && input[0] < 128) {
        memcpy(output, input, 1);
        return 1;
    } else if (input_len <= 55) {
        uint8_t _ = (uint8_t)0x80 + (uint8_t)input_len;
        uint8_t header[] = {_};
        memcpy(output, header, 1);
        memcpy(output+1, input, (size_t)input_len);
        return input_len+1;
    } else {
        uint8_t tmp_header[8];
        memset(tmp_header, 0, 8);
        uint32_t hexdigit = 1;
        uint32_t tmp = input_len;
        while ((uint32_t)(tmp / 256) > 0) {
            tmp_header[hexdigit] = (uint8_t)(tmp % 256);
            tmp = (uint32_t)(tmp / 256);
            hexdigit++;
        }
        tmp_header[hexdigit] = (uint8_t)(tmp);
        tmp_header[0] = (uint8_t)0xb7 + (uint8_t)hexdigit;

        // fix direction for header
        uint8_t header[8];
        memset(header, 0, 8);
        header[0] = tmp_header[0];
        for (int i=0; i<hexdigit; i++) {
            header[i+1] = tmp_header[hexdigit-i];
        }

        memcpy(output, header, hexdigit+1);
        memcpy(output+hexdigit+1, input, (size_t)input_len);
        return input_len+hexdigit+1;
    }
}

vector<uint8_t> Util::RlpEncodeItemWithVector(const vector<uint8_t>& input) {
    vector<uint8_t> output;
    size_t input_len = input.size();
    if (input_len > 0xffffffffUL) return output;
    output.reserve(input_len + 5);

    if (input_len==1 && input[0] == 0x00) {
        output.push_back(0x80);
    } else if (input_len==1 && input[0] < 128) {
        output.insert(output.end(), input.begin(), input.end());
    } else if (input_len <= 55) {
        uint8_t _ = (uint8_t)0x80 + (uint8_t)input_len;
        output.push_back(_);
        output.insert(output.end(), input.begin(), input.end());
    } else {
        vector<uint8_t> tmp_header;
        uint32_t tmp = input_len;
        while ((uint32_t)(tmp / 256) > 0) {
            tmp_header.push_back((uint8_t)(tmp % 256));
            tmp = (uint32_t)(tmp / 256);
        }
        tmp_header.push_back((uint8_t)(tmp));
        uint8_t len = tmp_header.size();// + 1;
        tmp_header.insert(tmp_header.begin(), 0xb7 + len);

        // fix direction for header
        vector<uint8_t> header;
        header.push_back(tmp_header[0]);
        uint8_t hexdigit = tmp_header.size() - 1;
        for (int i=0; i<hexdigit; i++) {
            header.push_back(tmp_header[hexdigit-i]);
        }

        output.insert(output.end(), header.begin(), header.end());
        output.insert(output.end(), input.begin(), input.end());
    }
    return output;
}

vector<uint8_t> Util::ConvertNumberToVector(unsigned long long val)
{
	vector<uint8_t> tmp;
	vector<uint8_t> ret;
	if ((unsigned long long)(val / 256) >= 0) {
		while ((unsigned long long)(val / 256) > 0) {
			tmp.push_back((uint8_t)(val % 256));
			val = (unsigned long long)(val / 256);
		}
		tmp.push_back((uint8_t)(val % 256));
		uint8_t len = tmp.size();
		for (int i = 0; i<len; i++) {
			ret.push_back(tmp[len - i - 1]);
		}
	}
	else {
		ret.push_back((uint8_t)val);
	}
	return ret;
}

vector<uint8_t> Util::ConvertNumberToVector(uint32_t val) {
    return ConvertNumberToVector((unsigned long long) val);
}

uint32_t Util::ConvertNumberToUintArray(uint8_t *str, uint32_t val) {
    uint32_t ret = 0;
    uint8_t tmp[8];
    memset(tmp,0,8);
    if ((uint32_t)(val / 256) >= 0) {
        while ((uint32_t)(val / 256) > 0) {
            tmp[ret] = (uint8_t)(val % 256);
            val = (uint32_t)(val / 256);
            ret++;
        }
        tmp[ret] = (uint8_t)(val % 256);
        for (int i=0; i<ret+1; i++) {
            str[i] = tmp[ret-i];
        }
    } else {
        str[0] = (uint8_t)val;
    }

    return ret+1;
}

uint8_t Util::ConvertCharToByte(const uint8_t* ptr)
{
	char c[3];
	c[0] = *(ptr);
	c[1] = *(ptr + 1);
	c[2] = 0x00;
	return strtol(c, nullptr, 16);
}

vector<uint8_t> Util::ConvertHexToVector(const uint8_t *in)
{
    const uint8_t *ptr = in;
    vector<uint8_t> out;
    if (ptr[0] == '0' && ptr[1] == 'x') ptr += 2;

	size_t lenstr = strlen((const char*)ptr);
	int i = 0;
	if ((lenstr % 2) == 1) //deal with odd sized hex strings
	{
		char c[2];
		c[0] = *ptr;
		c[1] = 0;
		out.push_back(ConvertCharToByte((const uint8_t*)c));
		i = 1;
	}
	for (; i<lenstr; i += 2)
	{
		out.push_back(ConvertCharToByte(ptr + i));
	}
	return out;
}

vector<uint8_t> Util::ConvertHexToVector(const string* str) {
    return ConvertHexToVector((uint8_t*)(str->c_str()));
}

uint32_t Util::ConvertCharStrToUintArray(uint8_t *out, const uint8_t *in) {
    uint32_t ret = 0;
    const uint8_t *ptr = in;
    // remove "0x"
    if (in[0] == '0' && in[1] == 'x') ptr += 2;

    size_t lenstr = strlen((const char*)ptr);
    for (int i=0; i<lenstr; i+=2) {
        char c[3];
        c[0] = *(ptr+i);
        c[1] = *(ptr+i+1);
        c[2] = 0x00;
        uint8_t val = strtol(c, nullptr, 16);
        out[ret] = val;
        ret++;
    }
    return ret;
};

uint8_t Util::HexToInt(uint8_t s) {
    uint8_t ret = 0;
    if(s >= '0' && s <= '9'){
        ret = uint8_t(s - '0');
    } else if(s >= 'a' && s <= 'f'){
        ret = uint8_t(s - 'a' + 10);
    } else if(s >= 'A' && s <= 'F'){
        ret = uint8_t(s - 'A' + 10);
    }
    return ret;
}

string Util::VectorToString(const vector<uint8_t> *buf)
{
    return ConvertBytesToHex((const uint8_t*)buf->data(), buf->size());
}

string Util::ConvertIntegerToBytes(const int32_t value)
{
    size_t hex_len = 4 << 1;
    std::string rc(hex_len, '0');
    for (size_t i = 0, j = (hex_len - 1) * 4; i<hex_len; ++i, j -= 4)
    {
        rc[i] = _aploembed_hexStr[(value >> j) & 0x0f];
    }
    return rc;
}

string Util::PlainVectorToString(const vector<uint8_t> *buf)
{
    if (buf == nullptr) return "";
    string output;
    output.reserve(buf->size() * 2);
    for (size_t i = 0; i < buf->size(); ++i) {
        output += _aploembed_hexStr[((*buf)[i] >> 4) & 0xF];
        output += _aploembed_hexStr[(*buf)[i] & 0xF];
    }
    return output;
}

string Util::ConvertBytesToHex(const uint8_t *bytes, int length)
{
    if (bytes == nullptr || length < 0) return "";
    string output = "0x";
    output.reserve(static_cast<size_t>(length) * 2 + 2);
    for (int i = 0; i < length; ++i) {
        output += _aploembed_hexStr[(bytes[i] >> 4) & 0xF];
        output += _aploembed_hexStr[bytes[i] & 0xF];
    }
    return output;
}

void Util::ConvertHexToBytes(uint8_t *_dst, const char *_src, int length)
{
    if (_src[0] == '0' && _src[1] == 'x') _src += 2; //chop off 0x

    for (int i = 0; i < length; i++)
    {
        char a = _src[2 * i];
        char b = _src[2 * i + 1];
        uint8_t extract = static_cast<uint8_t>((HexToInt(a) << 4) | HexToInt(b));
        _dst[i] = extract;
    }
}

string Util::ConvertBase(int from, int to, const char *s)
{
    if (s == nullptr || from < 2 || from > 36 || to < 2 || to > 36) return "";
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;

    const size_t length = strlen(s);
    if (length == 0 || length > 1024) return "";

    vector<uint8_t> digits;
    digits.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        int digit = -1;
        if (s[i] >= '0' && s[i] <= '9') digit = s[i] - '0';
        else if (s[i] >= 'A' && s[i] <= 'Z') digit = 10 + s[i] - 'A';
        else if (s[i] >= 'a' && s[i] <= 'z') digit = 10 + s[i] - 'a';
        if (digit < 0 || digit >= from) return "";
        digits.push_back(static_cast<uint8_t>(digit));
    }

    string output;
    output.reserve(length + 1);
    size_t first = 0;
    while (first < digits.size()) {
        unsigned remainder = 0;
        for (size_t i = first; i < digits.size(); ++i) {
            const unsigned value = remainder * static_cast<unsigned>(from) + digits[i];
            digits[i] = static_cast<uint8_t>(value / static_cast<unsigned>(to));
            remainder = value % static_cast<unsigned>(to);
        }
        output += (remainder < 10)
            ? static_cast<char>('0' + remainder)
            : static_cast<char>('A' + remainder - 10);
        while (first < digits.size() && digits[first] == 0) ++first;
    }

    if (output.empty()) return "0";
    std::reverse(output.begin(), output.end());
    return output;
}

string Util::ConvertDecimal(int decimals, string *result)
{
    int decimalLocation = result->length() - decimals;
	string newValue = "";
	if (decimalLocation <= 0)
	{
		newValue += "0.";
		for (; decimalLocation < 0; decimalLocation++)
		{
			newValue += "0";
		}
		newValue += *result;
	}
	else
	{
		//need to insert the point within the string
		newValue = result->substr(0, decimalLocation);
		newValue += ".";
		newValue += result->substr(decimalLocation);
	}

    return newValue;
}

string Util::ConvertHexToASCII(const char *result, size_t length)
{
	//convert hex to string.
	//first trim all the zeros
	int index = 0;
	string converted = "";
	char reader;
	int state = 0;
	bool endOfString = false;

	//No ASCII is less than 16 so this is safe
	while (index < length && (result[index] == '0' || result[index] == 'x')) index++;

	while (index < length && endOfString == false)
	{
		// convert from hex to ascii
		char c = result[index];
		switch (state)
		{
		case 0:
			reader = (char)(Util::HexToInt(c) * 16);
			state = 1;
			break;
		case 1:
			reader += (char)Util::HexToInt(c);
			if (reader == 0)
			{
				endOfString = true;
			}
			else
			{
				converted += reader;
				state = 0;
			}
			break;
		}
		index++;
	}

	return converted;
}

/**
 * Build a std::vector of bytes32 as hex strings
 **/
vector<string>* Util::ConvertCharStrToVector32(const char *resultPtr, size_t resultSize, vector<string> *result)
{
	if (resultSize < 64) return result;
    if (resultPtr[0] == '0' && resultPtr[1] == 'x') resultPtr += 2;
	//estimate size of return
	int returnSize = resultSize / 64;
	result->reserve(returnSize);
    int index = 0;
    char element[65];
    element[64] = 0;

    while (index <= (resultSize - 64))
    {
        memcpy(element, resultPtr, 64);
        result->push_back(string(element));
        resultPtr += 64;
        index += 64;
    }

	return result;
}

/**
 * @brief Only use to handle Ethereum results
 *
 * @param result
 * @return vector<string>*
 */
vector<string>* Util::ConvertResultToArray(string *value)
{
    string result = getJsonResultValue(value);

    return ConvertStringHexToABIArray(&result);
}

vector<string>* Util::ConvertStringHexToABIArray(string *result)
{
    vector<string> *retVal = new vector<string>();
    if (result == nullptr || result->empty()) return retVal;

    size_t startIndex = 0;
    if (result->length() >= 2 && result->at(0) == '0' &&
        (result->at(1) == 'x' || result->at(1) == 'X')) {
        startIndex = 2;
    }

    const size_t resultLength = result->length();

    while (startIndex < resultLength)
    {
        string subStr = result->substr(startIndex, 64);
        //we should never need to pad, as this should always be multiple of 64 chars
        retVal->push_back(subStr);
        startIndex += 64;
    }

    return retVal;
}

string Util::InterpretStringResult(const char *result)
{
    //convert to vector bytes32
    string retVal = "";

    if (result != NULL && strlen(result) > 0)
    {
        vector<string> breakDown;
        Util::ConvertCharStrToVector32(result, strlen(result), &breakDown);

        if (breakDown.size() > 2)
        {
            //check first value
            auto itr = breakDown.begin();
            long dyn = strtol(itr++->c_str(), NULL, 16);
            if (dyn == 32) //array marker
            {
                long length = strtol(itr++->c_str(), NULL, 16);
                //now get a pointer to string immediately after the length marker
                const char *strPtr = result + 2 + (2*64);
                retVal = ConvertHexToASCII(strPtr, length*2);
            }
        }
    }

    return retVal;
}

vector<string> *Util::InterpretVectorResult(string *result)
{
    vector<string> *retVal = new vector<string>();
    string resultValue = getJsonResultValue(result);
    const char *value = resultValue.c_str();

    if (value != NULL && resultValue.length() > 0)
    {
        vector<string> breakDown;
        Util::ConvertCharStrToVector32(value, resultValue.length(), &breakDown);

        if (breakDown.size() > 2)
        {
            //check first value
            auto itr = breakDown.begin();
            long dyn = strtol(itr++->c_str(), NULL, 16);
            if (dyn == 32) //array marker
            {
                long length = strtol(itr++->c_str(), NULL, 16);

                //checksum
                if (breakDown.size() != (length + 2))
                {
                    Serial.println("Bad array result data.");
                    for (itr = breakDown.begin(); itr != breakDown.end(); itr++) Serial.println(*itr->c_str());
                }
                for (;itr != breakDown.end(); itr++)
                {
                    retVal->push_back(*itr);
                }
            }
        }
    }

    return retVal;
}

void Util::PadForward(string *target, int targetSize)
{
    int remain = (targetSize*2) - (target->length() % targetSize);
    char buffer[remain+1];
    memset(buffer, '0', remain);
    buffer[remain] = 0;
    *target = buffer + *target;
}

uint256_t Util::ConvertToWei(double val, int decimals)
{
    char buffer[48];
    if (val < 0) val = 0;
    if (decimals < 0) decimals = 0;
    if (decimals > 18) decimals = 18;
    const double scaled = val * pow(10.0, decimals);
    snprintf(buffer, sizeof(buffer), "%.0f", scaled);
    const string weiHex = ConvertBase(10, 16, buffer);
    return uint256_t(weiHex.c_str());
}

uint256_t Util::ConvertDecimalToWei(const string& amount, int decimals)
{
    if (decimals < 0 || decimals > 77 || amount.empty()) return uint256_t(0);

    const size_t dot = amount.find('.');
    if (dot != string::npos && amount.find('.', dot + 1) != string::npos) return uint256_t(0);

    string whole = (dot == string::npos) ? amount : amount.substr(0, dot);
    string fraction = (dot == string::npos) ? "" : amount.substr(dot + 1);
    if (whole.empty()) whole = "0";
    if (fraction.length() > static_cast<size_t>(decimals)) return uint256_t(0);

    for (size_t i = 0; i < whole.length(); ++i) {
        if (!isdigit(static_cast<unsigned char>(whole[i]))) return uint256_t(0);
    }
    for (size_t i = 0; i < fraction.length(); ++i) {
        if (!isdigit(static_cast<unsigned char>(fraction[i]))) return uint256_t(0);
    }

    while (whole.length() > 1 && whole[0] == '0') whole.erase(0, 1);
    fraction.append(static_cast<size_t>(decimals) - fraction.length(), '0');
    string scaled = whole + fraction;
    size_t first = scaled.find_first_not_of('0');
    if (first == string::npos) return uint256_t(0);
    scaled.erase(0, first);
    if (scaled.length() > 78) return uint256_t(0);

    const string hex = ConvertBase(10, 16, scaled.c_str());
    if (hex.empty()) return uint256_t(0);
    return uint256_t(hex.c_str());
}

string Util::ConvertWeiToEthString(const uint256_t *weiVal, int decimals)
{
    if (weiVal == nullptr) return "0";
    if (decimals < 0) decimals = 0;
    if (decimals > 77) decimals = 77;

    vector<uint8_t> bytes = weiVal->export_bits_truncate();
    const string hex = ConvertBytesToHex(bytes.data(), bytes.size());
    string amount = ConvertBase(16, 10, hex.c_str());
    if (amount.empty()) amount = "0";

    const size_t decimalPlaces = static_cast<size_t>(decimals);
    if (decimalPlaces > 0) {
        if (amount.length() <= decimalPlaces) {
            amount.insert(0, decimalPlaces + 1 - amount.length(), '0');
        }
        amount.insert(amount.length() - decimalPlaces, 1, '.');

        while (amount.length() > 1 && amount.back() == '0') amount.pop_back();
        if (!amount.empty() && amount.back() == '.') amount.pop_back();
    }

    const size_t firstNonZero = amount.find_first_not_of('0');
    if (firstNonZero == string::npos) return "0";
    if (amount[firstNonZero] == '.') {
        return "0" + amount.substr(firstNonZero);
    }
    return amount.substr(firstNonZero);
}

string Util::ConvertEthToWei(double eth)
{
    char buffer[36]; //allow extra 4 chars for gcvt
    if (eth < 0) eth = 0;
    snprintf(buffer, sizeof(buffer), "%.0f", eth * pow(10.0, 18));
    std::string weiStr = std::string(buffer);
    std::size_t index = weiStr.find_last_of('.');
    if (index != std::string::npos) weiStr = weiStr.substr(0, index);
    return ConvertBase(10, 16, weiStr.c_str());
}

string Util::toString(int value)
{
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return string(buffer);
}

string Util::intToHex(int value)
{
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%x", static_cast<unsigned int>(value));
  return string(buffer);
}

// -------------------------------
// Mining Helper Functions
// -------------------------------

string Util::PackMiningData(const string* address, const string* nonce,
                             const string* difficulty, const string* prevHash,
                             uint32_t totalMined) {
    // Pack data for keccak256(abi.encodePacked(address, nonce, difficulty, prevHash, totalMined))
    // This matches Solidity's abi.encodePacked behavior

    string packed = "";

    // Remove 0x prefix from all hex strings
    string addr = *address;
    if (addr.substr(0, 2) == "0x") addr = addr.substr(2);

    string nonceHex = *nonce;
    if (nonceHex.substr(0, 2) == "0x") nonceHex = nonceHex.substr(2);

    string diffHex = *difficulty;
    if (diffHex.substr(0, 2) == "0x") diffHex = diffHex.substr(2);

    string prevHashHex = *prevHash;
    if (prevHashHex.substr(0, 2) == "0x") prevHashHex = prevHashHex.substr(2);

    // Pad address to 20 bytes (40 hex chars)
    while (addr.length() < 40) addr = "0" + addr;

    // Pad nonce to 32 bytes (64 hex chars)
    while (nonceHex.length() < 64) nonceHex = "0" + nonceHex;

    // Pad difficulty to 32 bytes (64 hex chars)
    while (diffHex.length() < 64) diffHex = "0" + diffHex;

    // Pad prevHash to 32 bytes (64 hex chars)
    while (prevHashHex.length() < 64) prevHashHex = "0" + prevHashHex;

    // Convert totalMined to 32-byte hex (big-endian)
    char totalMinedHex[65];
    snprintf(totalMinedHex, sizeof(totalMinedHex), "%064x", totalMined);

    // Concatenate all parts
    packed = addr + nonceHex + diffHex + prevHashHex + string(totalMinedHex);

    return packed;
}

static void stripHexPrefix(string *value) {
    while (value->length() >= 2 && value->at(0) == '0' &&
           (value->at(1) == 'x' || value->at(1) == 'X')) {
        *value = value->substr(2);
    }
}

string Util::ComputeKeccak256(const string* hexData) {
    // Convert hex string to bytes and compute keccak256
    vector<uint8_t> bytes = ConvertHexToVector(hexData);

    uint8_t hash[32];
    Crypto::Keccak256(bytes.data(), bytes.size(), hash);

    // ConvertBytesToHex already returns a canonical 0x-prefixed value.
    return ConvertBytesToHex(hash, 32);
}

bool Util::CompareUint256(const string* hash, const string* difficulty) {
    // Compare two uint256 hex strings. Returns true if hash < difficulty.
    string h = *hash;
    string d = *difficulty;

    // Be defensive: older builds briefly produced values like "0x0x...".
    stripHexPrefix(&h);
    stripHexPrefix(&d);

    if (h.length() > 64 || d.length() > 64) {
        return false;
    }

    // Pad to 64 hex chars (32 bytes)
    while (h.length() < 64) h = "0" + h;
    while (d.length() < 64) d = "0" + d;

    for (size_t i = 0; i < 64; ++i) {
        h[i] = static_cast<char>(tolower(static_cast<unsigned char>(h[i])));
        d[i] = static_cast<char>(tolower(static_cast<unsigned char>(d[i])));
        if (!isxdigit(static_cast<unsigned char>(h[i])) ||
            !isxdigit(static_cast<unsigned char>(d[i]))) {
            return false;
        }
    }

    // Lexicographic comparison works for same-length normalized hex strings.
    return h < d;
}
