/*
 * Curl.h
 *
 * Using the cURL library to provide networking functionality.
 *  This class is used by both the crawler and the extractor.
 *  NOT THREAD-SAFE! Use multiple instances for multiple threads.
 *
 *  Created on: Oct 8, 2018
 *      Author: ans
 */

#ifndef NETWORK_CURL_HPP_
#define NETWORK_CURL_HPP_

#include "../Helper/Utf8.hpp"
#include "../Main/Exception.hpp"
#include "../Wrapper/Curl.hpp"
#include "../Wrapper/CurlList.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdio>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "Config.hpp"

namespace crawlservpp::Network {

	class Curl {
	public:
		Curl();
		virtual ~Curl();

		// setters
		void setConfigGlobal(const Config& globalConfig, bool limited, std::queue<std::string> * warningsTo);
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
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

		// not moveable, not copyable
		Curl(Curl&) = delete;
		Curl(Curl&&) = delete;
		Curl& operator=(Curl&) = delete;
		Curl& operator=(Curl&&) = delete;

	private:
		CURLcode curlCode;
		std::string content;
		std::string contentType;
		unsigned int responseCode;
		bool limitedSettings;

		// const pointer to network configuration
		const Network::Config * config;

		Wrapper::Curl curl;

		Wrapper::CurlList dnsResolves, headers, http200Aliases, proxyHeaders;

		// helper function for cURL strings
		static std::string curlStringToString(char * curlString);

		// static and in-class writer functions
		static int writer(char * data, unsigned long size, unsigned long nmemb, void * thisPointer);
		int writerInClass(char * data, unsigned long size, unsigned long nmemb);
	};

} /* crawlservpp::Network */

#endif /* NETWORK_CURL_HPP_ */
