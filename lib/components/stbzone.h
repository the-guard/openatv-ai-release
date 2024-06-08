#ifndef __lib_components_stbzone_h
#define __lib_components_stbzone_h
#include <lib/base/estring.h>
#include <iostream>



class STBZone {
public:
	static STBZone& GetInstance();	
	std::string brand_name;	
	std::string model_name; 	
	std::string mac_address;
	std::string code;	
	std::string source_language;
	std::string translation_language;
	std::string translation_result;
	std::string latest_visible_translation;
	std::string subtitle_type;
	std::string subtitle_data;
	std::string pid;
	std::string page;
	std::string magazine;
	std::string service_id;	
	std::string activation_response;
	bool initialized;
	bool valid_subscription;
	bool translation_received;
	
	std::string base64Encode(unsigned char* data, size_t len);
	std::vector<std::string> parseJsonArray(const std::string& json);
	std::string getFirstLine(const std::string& str);
	int initiate();
	void translate();
	void sendTranslationRequest();
	void translateTeletext(std::string& translation);
	int getSTBInfo();
	STBZone();

private:	
	std::string httpPostJson();
	std::string url;
	std::string jsonData;
};

#endif
