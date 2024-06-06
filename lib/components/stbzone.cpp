#include "lib/components/stbzone.h"
#include "lib/components/socket_client.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include <iostream>
#include <lib/base/nconfig.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <fstream>
#include <map>

STBZone& STBZone::GetInstance() {
	static STBZone instance;  // Static instance of STBZone
	return instance;
}
 
STBZone::STBZone()
	: brand_name(""),
	model_name(""),
	mac_address(""),
	code(""),
	source_language(""),
	translation_language(""),
	translation_result(""),
	latest_visible_translation(""),
	subtitle_type("0"),
	subtitle_data(""),	
	pid("0"),
	page("0"),
	magazine("0"),
	service_id(""),
	ai_socket_available(false),
	initialized(false),
	valid_subscription(false),
	translation_received(false),
	url(""),
	jsonData(""),
	activation_response("")
{
	// Initialize other necessary members
}


int STBZone::getSTBInfo()
{
	int sockfd;
	struct ifreq ifr;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1) {
		perror("socket");
		return 1;
	}

	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);

	// Get MAC address
	if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == -1) {
		perror("ioctl");
		close(sockfd);
		return 1;
	}
	// Convert MAC address to string
	unsigned char* mac = (unsigned char*)ifr.ifr_hwaddr.sa_data;
	char mac_addr[18];
	sprintf(mac_addr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	mac_address = mac_addr;
	close(sockfd);

	//Get Boxinfo
	std::string filename = "/usr/lib/enigma.info";
	std::map<std::string, std::string> settings;
	std::ifstream file(filename);
	if (file.is_open()) {
		std::string line;
		while (std::getline(file, line)) {
			// Extract variable name and value
			size_t pos = line.find('=');
			if (pos != std::string::npos) {
				std::string variable = line.substr(0, pos);
				std::string value = line.substr(pos + 1);
				settings[variable] = value;
			}
		}
		file.close();
	}
	else {
		return 1;
	}
	brand_name = settings["displaybrand"];
	model_name = settings["displaymodel"];
	return 0;
}


size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
	response->append((char*)contents, size * nmemb);
	return size * nmemb;
}

std::string STBZone::httpPostJson() {
	//eDebug("[STBZone] URL: %s" , url.c_str());
	//eDebug("[STBZone] URL: %s" , jsonData.c_str());
	curl_global_init(CURL_GLOBAL_ALL);
	CURL* curl = curl_easy_init();
	std::string response;
	std::string errorMessage;

	if (curl) {
		curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);
		// Set the URL to make the request to
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		// Set the request method to POST
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		// Set the JSON content type header
		struct curl_slist* headers = NULL;
		headers = curl_slist_append(headers, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		// Set the POST data
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
		// Set the callback function to write received data into response string
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

		// Perform the HTTP POST request
		CURLcode res = curl_easy_perform(curl);

		// Check for errors
		if (res != CURLE_OK) {
			errorMessage = "HTTP request failed: " + std::string(curl_easy_strerror(res));
			//eDebug("[STBZone] CURL: %s" , std::string(curl_easy_strerror(res)).c_str());
		}

		// Clean up and release resources
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
	}
	else {
		//eDebug("[STBZone] CURL: Faild to initialize libcurl");
		errorMessage = "Failed to initialize libcurl";
	}

	// Handle error message
	if (!errorMessage.empty()) {
		return errorMessage;

	}
	else {

		return response;
	}
}

std::string STBZone::getFirstLine(const std::string& str) {
	std::string firstLine;
	std::size_t pos = str.find('\n');
	if (pos != std::string::npos) {
		firstLine = str.substr(0, pos);
	}
	else {
		firstLine = str;
	}
	return firstLine;
}

std::string STBZone::base64Encode(unsigned char* data, size_t len) {
	const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::string base64;
	base64.reserve(((len + 2) / 3) * 4);

	for (size_t i = 0; i < len; i += 3) {
		unsigned int triplet = (data[i] << 16) + ((i + 1 < len) ? (data[i + 1] << 8) : 0) + ((i + 2 < len) ? data[i + 2] : 0);

		for (int j = 0; j < 4; j++) {
			if (i + j * 6 <= len * 8) {
				base64.push_back(base64_chars[(triplet >> (6 * (3 - j))) & 0x3F]);
			}
			else {
				base64.push_back('=');
			}
		}
	}
	return base64;
}

std::vector<std::string> STBZone::parseJsonArray(const std::string& json) {
	std::vector<std::string> result;

	// Find the start of the array
	size_t start = json.find_first_of("[");
	if (start == std::string::npos) {
		return result; // Invalid JSON format
	}

	// Find the end of the array
	size_t end = json.find_last_of("]");
	if (end == std::string::npos) {
		return result; // Invalid JSON format
	}

	// Extract the contents of the array
	std::string arrayContents = json.substr(start + 1, end - start - 1);

	// Parse individual elements (assuming elements are quoted strings)
	size_t pos = 0;
	while (pos < arrayContents.length()) {
		// Find the start of the string element
		size_t elementStart = arrayContents.find_first_of("\"", pos);
		if (elementStart == std::string::npos) {
			break; // No more elements
		}

		// Find the end of the string element
		size_t elementEnd = arrayContents.find_first_of("\"", elementStart + 1);
		if (elementEnd == std::string::npos) {
			break; // Invalid JSON format
		}

		// Extract the string element
		std::string element = arrayContents.substr(elementStart + 1, elementEnd - elementStart - 1);
		result.push_back(element);

		// Move to the next element
		pos = elementEnd + 1;
	}

	return result;
}


//Initialize the 
int STBZone::initiate()
{
	//Check first if socket is available
	std::ifstream aiSocket("/etc/init.d/aisocket");
	ai_socket_available =  aiSocket.is_open();
	if (!ai_socket_available)
	{
		return 1;
	}
	//Check if it is already initiated, and if it is, just return.
	if (initialized && valid_subscription) {
		return 1;
	}
	//Verify the activation code is entered, has 12 digits, and is not the trial code 15.
	if (eConfigManager::getConfigValue("config.subtitles.ai_code") != "15")
	{
		if (eConfigManager::getConfigValue("config.subtitles.ai_code").size() != 12)
		{
			activation_response = "Invalid AI-Powered translation subscription code!";
			return 1;
		}
	}

	//Check if there was a previous invalid initialization, and if so, re-initialize only if the code is different
	if (code == eConfigManager::getConfigValue("config.subtitles.ai_code"))
	{
		//Return because the same invalid code was used.
		return 1;
	}
	//Check if the MAC address and other information are already present.
	if (mac_address == "" || brand_name == "" || model_name == "")
	{
		getSTBInfo();
	}
	//Send the activation request and update the control fields based on the result
	code = eConfigManager::getConfigValue("config.subtitles.ai_code");
	url = "https://ai.stbzone.com/e2/activate/v1";
	jsonData = "{\"mac_address\": \"" + mac_address + "\",\"model_name\": \"" + model_name + "\",\"brand_name\": \"" + brand_name + "\",\"code\": \"" + code + "\"}";
	std::string activationResult = httpPostJson();
	if (activationResult.find("Welcome") != std::string::npos) {
		initialized = true;
		valid_subscription = true;
		activation_response = activationResult;
		return 0;
	}
	else
	{
		initialized = true;
		valid_subscription = false;
		activation_response = activationResult;
		return 0;
	}
	return 0;
}

void STBZone::translateTeletext(std::string& translation)
{
	translation_language = eConfigManager::getConfigValue("config.subtitles.ai_translate_to");	 
	url = "https://ai.stbzone.com/e2/translate/v1";
	jsonData = "{\"subtitle_type\": \"" + subtitle_type + "\","
		"\"mac_address\": \"" + mac_address + "\","
		"\"subtitle_data\": \"" + subtitle_data + "\","
		"\"source_language\": \"" + source_language + "\","
		"\"translation_language\": \"" + translation_language + "\"}";	
	translation = httpPostJson();
}

void STBZone::sendTranslationRequest()
{
	translation_language = eConfigManager::getConfigValue("config.subtitles.ai_translate_to");
	jsonData = "{\"subtitle_type\": \"" + subtitle_type + "\","
		"\"service_name\": \"" + service_id + "\","
		"\"pid\": \"" + pid + "\","
		"\"page\": \"" + page + "\","
		"\"magazine\": \"" + magazine + "\","
		"\"mac_address\": \"" + mac_address + "\","
		"\"subtitle_data\": \"" + subtitle_data + "\","
		"\"source_language\": \"" + source_language + "\","
		"\"translation_language\": \"" + translation_language + "\"}";
	UnixSocketClient& client = UnixSocketClient::getInstance("/tmp/ai.socket");
	client.sendJsonData(jsonData);
}


