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
		CURLcode curlCode;
		std::string content;
		std::string contentType;
		unsigned int responseCode;
		bool limitedSettings;

		// const pointer to network configuration
		const Network::Config * config;

		// RAII wrapper sub-class for cURL pointer, also handles global instance if necessary (does NOT have ownership of the pointer!)
		class CurlWrapper {
		public:
			// constructor: set pointer to NULL
			CurlWrapper() {
				// initialize global instance if necessary
				if(CurlWrapper::globalInit) this->localInit = false;
				else {
					CurlWrapper::globalInit = true;
					this->localInit = true;
					curl_global_init(CURL_GLOBAL_ALL);
				}

				// initialize cURL
				this->ptr = NULL;
				this->init();
			}

			// destructor: cleanup cURL if necessary
			virtual ~CurlWrapper() {
				this->reset();

				// cleanup global instance if necessary
				if(CurlWrapper::globalInit && this->localInit) {
					curl_global_cleanup();
					CurlWrapper::globalInit = false;
					this->localInit = false;
				}
			}

			// get const pointer to query list
			const CURL * get() const { return this->ptr; }

			// get non-const pointer to query list
			CURL * get() { return this->ptr; }

			// get non-const pointer to pointer to query list
			CURL ** getPtr() { return &(this->ptr); }

			// control functions
			void init() { this->ptr = curl_easy_init();	if(!(this->ptr)) throw Curl::Exception("Could not initialize cURL"); }
			void reset() { if(this->ptr) curl_easy_cleanup(this->ptr); this->ptr = NULL; }

			// rule of five
			CurlWrapper(CurlWrapper const&) = delete;
			CurlWrapper(CurlWrapper&&) = delete;
			CurlWrapper& operator=(CurlWrapper const&) = delete;
			CurlWrapper& operator=(CurlWrapper&&) = delete;

		private:
			CURL * ptr;
			bool localInit;
			static bool globalInit;
		} curl;

		// RAII wrapper sub-class for cURL list (does NOT have ownership of the pointer!)
		class CurlListWrapper {
		public:
			// constructor: set pointer to NULL
			CurlListWrapper() { this->ptr = NULL; }

			// destructor: free query list if necessary
			virtual ~CurlListWrapper() { this->reset(); }

			// get const pointer to query list
			const struct curl_slist * get() const { return this->ptr; }

			// get non-const pointer to query list
			struct curl_slist * get() { return this->ptr; }

			// get non-const pointer to pointer to query list
			struct curl_slist ** getPtr() { return &(this->ptr); }

			// list functions
			void append(const std::string& newElement) { this->ptr = curl_slist_append(this->ptr, newElement.c_str()); }
			void reset() { if(this->ptr) curl_slist_free_all(this->ptr); this->ptr = NULL; }

			// rule of five
			CurlListWrapper(CurlListWrapper const&) = delete;
			CurlListWrapper(CurlListWrapper&&) = delete;
			CurlListWrapper& operator=(CurlListWrapper const&) = delete;
			CurlListWrapper& operator=(CurlListWrapper&&) = delete;

		private:
			struct curl_slist * ptr;
		} dnsResolves, headers, http200Aliases, proxyHeaders;

		// helper function for cURL strings
		static std::string curlStringToString(char * curlString);

		// static and in-class writer functions
		static int writer(char * data, unsigned long size, unsigned long nmemb, void * thisPointer);
		int writerInClass(char * data, unsigned long size, unsigned long nmemb);
	};
}

#endif /* NETWORK_CURL_H_ */
