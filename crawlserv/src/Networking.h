/*
 * Networking.h
 *
 * A class using the cURL library to provide networking functionality.
 * NOT THREAD-SAFE! Use multiple instances for multiple threads.
 *
 *  Created on: Oct 8, 2018
 *      Author: ans
 */

#ifndef NETWORKING_H_
#define NETWORKING_H_

#include "ConfigCrawler.h"
#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include "namespaces/Helpers.h"

class Networking {
public:
	Networking();
	virtual ~Networking();

	bool setCrawlingConfigGlobal(const ConfigCrawler& config, bool limited, std::vector<std::string> * warningsTo);
	bool setCrawlingConfigCurrent(const ConfigCrawler& config);

	bool getContent(const std::string& url, std::string& contentTo, const std::vector<unsigned int>& errors);
	unsigned int getResponseCode() const;
	std::string getContentType() const;
	void resetConnection(unsigned long sleep);

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
	const ConfigCrawler * configCrawler;
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

#endif /* NETWORKING_H_ */
