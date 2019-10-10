/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * Curl.hpp
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

#include "Config.hpp"

#include "../Helper/FileSystem.hpp"
#include "../Helper/Utf8.hpp"
#include "../Main/Exception.hpp"
#include "../Wrapper/Curl.hpp"
#include "../Wrapper/CurlList.hpp"

#include <curl/curl.h>

#include <algorithm>	// std::find, std::remove_if, std::transform
#include <cctype>		// ::isspace, ::tolower
#include <chrono>		// std::chrono
#include <exception>	// std::exception
#include <queue>		// std::queue
#include <string>		// std::string, std::to_string
#include <thread>		// std::this_thread
#include <vector>		// std::vector

namespace crawlservpp::Network {

	class Curl {
	public:
		Curl(const std::string& cookieDirectory);
		virtual ~Curl();

		// setters
		void setConfigGlobal(const Config& globalConfig, bool limited, std::queue<std::string>& warningsTo);
		void setConfigCurrent(const Config& currentConfig);
		void setCookies(const std::string& cookies);
		void setHeaders(const std::vector<std::string>& customHeaders);
		void setVerbose(bool isVerbose);
		void unsetCookies();
		void unsetHeaders();

		// getters
		void getContent(const std::string& url, bool usePost, std::string& contentTo, const std::vector<unsigned int>& errors);
		unsigned int getResponseCode() const;
		std::string getContentType() const;
		CURLcode getCurlCode() const;

		// resetter
		void resetConnection(unsigned long sleep);

		// URL escaping
		std::string escape(const std::string& stringToEscape, bool usePlusForSpace);
		std::string unescape(const std::string& escapedString, bool usePlusForSpace);
		std::string escapeUrl(const std::string& urlToEscape);

		// class for cURL exceptions
		MAIN_EXCEPTION_CLASS();

		// not moveable, not copyable
		Curl(Curl&) = delete;
		Curl(Curl&&) = delete;
		Curl& operator=(Curl&) = delete;
		Curl& operator=(Curl&&) = delete;

	private:
		const std::string cookieDir;
		CURLcode curlCode;
		std::string content;
		std::string contentType;
		unsigned int responseCode;
		bool limitedSettings;
		bool post;
		std::string tmpCookies;
		std::string oldCookies;

		// const pointer to network configuration
		const Network::Config * config;

		// cURL object
		Wrapper::Curl curl;

		// cURL lists
		Wrapper::CurlList dnsResolves;
		Wrapper::CurlList headers;
		Wrapper::CurlList tmpHeaders;
		Wrapper::CurlList http200Aliases;
		Wrapper::CurlList proxyHeaders;

		// helper function for cURL strings
		static std::string curlStringToString(char * curlString);

		// static and in-class writer functions
		static int writer(char * data, unsigned long size, unsigned long nmemb, void * thisPointer);
		int writerInClass(char * data, unsigned long size, unsigned long nmemb);
	};

} /* crawlservpp::Network */

#endif /* NETWORK_CURL_HPP_ */
