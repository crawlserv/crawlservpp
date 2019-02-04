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

		// setters
		void setConfigGlobal(const Config& globalConfig, bool limited, std::vector<std::string> * warningsTo);
		void setConfigCurrent(const Config& currentConfig);

		// getters
		void getContent(const std::string& url, std::string& contentTo, const std::vector<unsigned int>& errors);
		unsigned int getResponseCode() const;
		std::string getContentType() const;
		CURLcode getCurlCode() const;

		// resetter
		void resetConnection(unsigned long sleep);

		// URL escaping
		std::string escape(const std::string& stringToEscape, bool usePlusForSpace);
		std::string unescape(const std::string& escapedString, bool usePlusForSpace);
		std::string escapeUrl(const std::string& urlToEscape);

		// sub-class for cURL exceptions
		class Exception : public std::exception {
		public:
			Exception(const std::string& description) { this->_description = description; }
			const char * what() const throw() { return this->_description.c_str(); }
			const std::string& whatStr() const throw() { return this->_description; }
		private:
			std::string _description;
		};

	private:
		CURL * curl;
		CURLcode curlCode;
		std::string content;
		std::string contentType;
		unsigned int responseCode;
		const Config * config;
		bool limitedSettings;

		static int writer(char * data, unsigned long size, unsigned long nmemb, void * thisPointer);
		int writerInClass(char * data, unsigned long size, unsigned long nmemb);

		static bool globalInit;
		bool localInit;

		struct curl_slist * dnsResolves;
		struct curl_slist * headers;
		struct curl_slist * http200Aliases;
		struct curl_slist * proxyHeaders;
	};
}

#endif /* NETWORK_CURL_H_ */
