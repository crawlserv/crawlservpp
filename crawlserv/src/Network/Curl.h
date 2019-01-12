/*
 * Curl.h
 *
 * A class using the cURL library to provide networking functionality.
 * This class is used by both the crawler and the extractor.
 * NOT THREAD-SAFE! Use multiple instances for multiple threads.
 *
 *  Created on: Oct 8, 2018
 *      Author: ans
 */

#ifndef NETWORK_CURL_H_
#define NETWORK_CURL_H_

#include "Config.h"

#include "../Helper/Utf8.h"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdio>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace crawlservpp::Network {
	class Curl {
	public:
		Curl();
		virtual ~Curl();

		bool setConfigGlobal(const crawlservpp::Network::Config& globalConfig, bool limited, std::vector<std::string> * warningsTo);
		bool setConfigCurrent(const crawlservpp::Network::Config& currentConfig);

		bool getContent(const std::string& url, std::string& contentTo, const std::vector<unsigned int>& errors);
		unsigned int getResponseCode() const;
		std::string getContentType() const;
		void resetConnection(unsigned long sleep);

		CURLcode getCurlCode() const;
		const std::string& getErrorMessage() const;

		std::string escape(const std::string& stringToEscape, bool usePlusForSpace);
		std::string unescape(const std::string& escapedString, bool usePlusForSpace);
		std::string escapeUrl(const std::string& urlToEscape);

	private:
		CURL * curl;
		CURLcode curlCode;
		std::string content;
		std::string contentType;
		unsigned int responseCode;
		const Config * config;
		bool limitedSettings;

		static int cURLWriter(char * data, unsigned long size, unsigned long nmemb, void * thisPointer);
		int cURLWriterInClass(char * data, unsigned long size, unsigned long nmemb);

		std::string errorMessage;
		static bool globalInit;
		bool localInit;

		struct curl_slist * dnsResolves;
		struct curl_slist * headers;
		struct curl_slist * http200Aliases;
		struct curl_slist * proxyHeaders;
	};
}

#endif /* NETWORK_CURL_H_ */
