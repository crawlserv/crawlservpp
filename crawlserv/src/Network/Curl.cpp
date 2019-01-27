/*
 * Curl.cpp
 *
 * A class using the cURL library to provide networking functionality.
 * This class is used by both the crawler and the extractor.
 * NOT THREAD-SAFE! Use multiple instances for multiple threads.
 *
 *  Created on: Oct 8, 2018
 *      Author: ans
 */

#include "Curl.h"

namespace crawlservpp::Network {

bool Curl::globalInit = false;

// constructor
Curl::Curl() {
	bool error = false;

	// set default values
	this->dnsResolves = NULL;
	this->headers = NULL;
	this->http200Aliases = NULL;
	this->proxyHeaders = NULL;
	this->curlCode = CURLE_OK;
	this->responseCode = 0;
	this->config = NULL;
	this->limitedSettings = false;

	// initialize networking if necessary
	if(Curl::globalInit) this->localInit = false;
	else {
		Curl::globalInit = true;
		this->localInit = true;
		curl_global_init(CURL_GLOBAL_ALL);
	}

	// initialize cURL
	this->curl = curl_easy_init();
	if(!(this->curl)) throw std::runtime_error("Could not initialize cURL");

	// configure cURL (global defaults)
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_NOSIGNAL, 1L);
	if(this->curlCode != CURLE_OK) error = true;
	else {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, Curl::writer);
		if(this->curlCode != CURLE_OK) error = true;
		else {
			// set pointer to instance
			this->curlCode = curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, this);
			if(this->curlCode != CURLE_OK) error = true;
		}
	}

	// error handling
	if(error) {
		if(this->curl) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			curl_easy_cleanup(this->curl);
			this->curl = NULL;
		}
		curl_global_cleanup();
		throw std::runtime_error("Could not configure cURL");
	}
}

// destructor
Curl::~Curl() {
	// cleanup cURL
	if(this->curl) {
		curl_easy_cleanup(this->curl);
		this->curl = NULL;
	}

	// cleanup lists
	if(this->dnsResolves) {
		curl_slist_free_all(this->dnsResolves);
		this->dnsResolves = NULL;
	}
	if(this->headers) {
		curl_slist_free_all(this->headers);
		this->headers = NULL;
	}
	if(this->http200Aliases) {
		curl_slist_free_all(this->http200Aliases);
		this->http200Aliases = NULL;
	}
	if(this->proxyHeaders) {
		curl_slist_free_all(this->proxyHeaders);
		this->proxyHeaders = NULL;
	}

	// cleanup global instance if necessary
	if(Curl::globalInit && this->localInit) {
		curl_global_cleanup();
		Curl::globalInit = false;
		this->localInit = false;
	}
}

// set global network options from crawling configuration
// if limited is set, cookie settings, custom headers, HTTP version and error responses will be ignored
bool Curl::setConfigGlobal(const Config& globalConfig, bool limited, std::vector<std::string> * warningsTo) {
	if(!(this->curl)) {
		this->errorMessage = "cURL not initialized";
		return false;
	}

	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_MAXCONNECTS, (long) globalConfig.connectionsMax);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_IGNORE_CONTENT_LENGTH, globalConfig.contentLengthIgnore ? 1L : 0L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	if(globalConfig.cookies && !limited) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_COOKIEFILE, globalConfig.cookiesLoad.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
		if(globalConfig.cookiesSave.c_str()) {
			this->curlCode = curl_easy_setopt(this->curl, CURLOPT_COOKIEJAR, globalConfig.cookiesSave.c_str());
			if(this->curlCode != CURLE_OK) {
				this->errorMessage = curl_easy_strerror(this->curlCode);
				return false;
			}
		}
	}
	if(!globalConfig.cookiesSession && !limited) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_COOKIESESSION, 1L);
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	if(!globalConfig.cookiesSet.empty() && !limited) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_COOKIE, globalConfig.cookiesSet.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_DNS_CACHE_TIMEOUT, globalConfig.dnsCacheTimeOut);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	if(!globalConfig.dnsDoH.empty()) {
#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 62)
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_DOH_URL, globalConfig.dnsDoH.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
#else
		if(warningsTo) warningsTo->push_back("DNS-over-HTTPS currently not supported, \'network.dns.doh\' ignored.");
#endif
	}
	if(!globalConfig.dnsInterface.empty()) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_DNS_INTERFACE, globalConfig.dnsInterface.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	if(!globalConfig.dnsResolves.empty()) {
		for(auto i = globalConfig.dnsResolves.begin(); i != globalConfig.dnsResolves.end(); ++i)
			this->dnsResolves = curl_slist_append(this->dnsResolves, i->c_str());
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_RESOLVE, this->dnsResolves);
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	if(!globalConfig.dnsServers.empty()) {
		std::string serverList;
		for(auto i = globalConfig.dnsServers.begin(); i != globalConfig.dnsServers.end(); ++i)
			serverList += *i + ",";
		serverList.pop_back();
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_DNS_SERVERS, serverList.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 60)
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_DNS_SHUFFLE_ADDRESSES, globalConfig.dnsShuffle ? 1L : 0L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
#else
	if(globalConfig.dnsShuffle && warningsTo)
		warningsTo->push_back("DNS shuffling currently not supported, \'network.dns.shuffle\' ignored.");
#endif
	if(globalConfig.encodingBr && globalConfig.encodingDeflate && globalConfig.encodingGZip && globalConfig.encodingIdentity) {
		this->curlCode = curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	else if(globalConfig.encodingBr || globalConfig.encodingDeflate || globalConfig.encodingGZip || globalConfig.encodingIdentity) {
		std::string encodingList;
		if(globalConfig.encodingBr) encodingList += "br,";
		if(globalConfig.encodingDeflate) encodingList += "deflate,";
		if(globalConfig.encodingGZip) encodingList += "gzip,";
		if(globalConfig.encodingIdentity) encodingList += "identity,";
		encodingList.pop_back();
		this->curlCode = curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, encodingList.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	if(globalConfig.encodingTransfer) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_TRANSFER_ENCODING, globalConfig.encodingTransfer ? 1L : 0L);
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	if(!globalConfig.headers.empty() && !limited) {
		for(auto i = globalConfig.headers.begin(); i != globalConfig.headers.end(); ++i)
			this->headers = curl_slist_append(this->headers, i->c_str());
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, this->dnsResolves);
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	if(!globalConfig.http200Aliases.empty() && !limited) {
		for(auto i = globalConfig.http200Aliases.begin(); i != globalConfig.http200Aliases.end(); ++i)
			this->http200Aliases = curl_slist_append(this->http200Aliases, i->c_str());
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_HTTP200ALIASES, this->http200Aliases);
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	if(!limited) {
		switch(globalConfig.httpVersion) {
		case Config::httpVersionAny:
			this->curlCode = curl_easy_setopt(this->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_NONE);
			break;
		case Config::httpVersionV1:
			this->curlCode = curl_easy_setopt(this->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
			break;
		case Config::httpVersionV11:
			this->curlCode = curl_easy_setopt(this->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
			break;
		case Config::httpVersionV2:
			this->curlCode = curl_easy_setopt(this->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
			break;
		case Config::httpVersionV2only:
			this->curlCode = curl_easy_setopt(this->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
			break;
		case Config::httpVersionV2tls:
			this->curlCode = curl_easy_setopt(this->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
			break;
		default:
			if(warningsTo) warningsTo->push_back("Enum value for HTTP version not recognized, \'network.http.version\' ignored.");
			this->curlCode = CURLE_OK;
		}
	}
	if(!globalConfig.localInterface.empty()) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_INTERFACE, globalConfig.localInterface.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_LOCALPORT, (long) globalConfig.localPort);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_LOCALPORTRANGE, (long) globalConfig.localPortRange);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	if(!globalConfig.proxy.empty()) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_PROXY, globalConfig.proxy.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	if(!globalConfig.proxyAuth.empty()) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_PROXYUSERPWD , globalConfig.proxyAuth.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	if(!globalConfig.proxyHeaders.empty()) {
		for(auto i = globalConfig.proxyHeaders.begin(); i != globalConfig.proxyHeaders.end(); ++i)
			this->headers = curl_slist_append(this->proxyHeaders, i->c_str());
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_PROXYHEADER, this->proxyHeaders);
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	if(!globalConfig.proxyPre.empty()) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_PRE_PROXY, globalConfig.proxyPre.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	if(!globalConfig.proxyTlsSrpPassword.empty() || !globalConfig.proxyTlsSrpUser.empty()) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_PROXY_TLSAUTH_USERNAME, globalConfig.proxyTlsSrpUser.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_PROXY_TLSAUTH_PASSWORD, globalConfig.proxyTlsSrpPassword.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_HTTPPROXYTUNNEL, globalConfig.proxyTunnelling ? 1L : 0L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_FOLLOWLOCATION, globalConfig.redirect ? 1L : 0L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_MAXREDIRS , (long) globalConfig.redirectMax);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	if(globalConfig.redirectPost301 && globalConfig.redirectPost302 && globalConfig.redirectPost303) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	else {
		long redirectPost = 0;
		if(globalConfig.redirectPost301) redirectPost |= CURL_REDIR_POST_301;
		if(globalConfig.redirectPost302) redirectPost |= CURL_REDIR_POST_302;
		if(globalConfig.redirectPost303) redirectPost |= CURL_REDIR_POST_303;
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_POSTREDIR, redirectPost);
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	if(!globalConfig.referer.empty() && !limited) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_REFERER, globalConfig.referer.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_AUTOREFERER, globalConfig.refererAutomatic ? 1L : 0L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_MAX_RECV_SPEED_LARGE, (long) globalConfig.speedDownLimit);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_LOW_SPEED_LIMIT, (long) globalConfig.speedLowLimit);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_LOW_SPEED_TIME, (long) globalConfig.speedLowTime);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_MAX_SEND_SPEED_LARGE, (long) globalConfig.speedUpLimit);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}

	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_SSL_VERIFYHOST, globalConfig.sslVerifyHost ? 2L : 0L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}

	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_SSL_VERIFYPEER, globalConfig.sslVerifyPeer ? 1L : 0L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_PROXY_SSL_VERIFYHOST, globalConfig.sslVerifyProxyHost ? 2L : 0L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_PROXY_SSL_VERIFYPEER, globalConfig.sslVerifyProxyPeer ? 1L : 0L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_SSL_VERIFYSTATUS, globalConfig.sslVerifyStatus ? 1L : 0L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_TCP_FASTOPEN, globalConfig.tcpFastOpen ? 1L : 0L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_TCP_KEEPALIVE, globalConfig.tcpKeepAlive ? 1L : 0L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_TCP_KEEPIDLE, (long) globalConfig.tcpKeepAliveIdle);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_TCP_KEEPINTVL, (long) globalConfig.tcpKeepAliveInterval);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_TCP_NODELAY, globalConfig.tcpNagle ? 0L : 1L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_CONNECTTIMEOUT, (long) globalConfig.timeOut);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 59)
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS, (long) globalConfig.timeOutHappyEyeballs);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
#else
	if(globalConfig.timeOutHappyEyeballs && warningsTo) warningsTo->push_back("Happy Eyeballs globalConfiguration currently not supported,"
			" \'network.timeout.happyeyeballs\' ignored.");
#endif
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_TIMEOUT, (long) globalConfig.timeOutRequest);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	if(!globalConfig.tlsSrpPassword.empty() || !globalConfig.tlsSrpUser.empty()) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_TLSAUTH_USERNAME, globalConfig.tlsSrpUser.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_TLSAUTH_PASSWORD, globalConfig.tlsSrpPassword.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	if(!globalConfig.userAgent.empty()) {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_USERAGENT, globalConfig.userAgent.c_str());
		if(this->curlCode != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			return false;
		}
	}
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_VERBOSE, globalConfig.verbose ? 1L : 0L);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}

	// save configuration
	this->config = &globalConfig;
	this->limitedSettings = limited;

	return true;
}

// set current network options from crawling configuration
bool Curl::setConfigCurrent(const Config& currentConfig) {
	//TODO (e.g. overwrite cookies)
	return true;
}

// get remote content
bool Curl::getContent(const std::string& url, std::string& contentTo, const std::vector<unsigned int>& errors) {
	std::string encodedUrl = this->escapeUrl(url);
	char errorBuffer[CURL_ERROR_SIZE];
	this->content.clear();
	this->contentType.clear();
	this->responseCode = 0;

	// set URL
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_URL, encodedUrl.c_str());
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}

	// set error buffer
	errorBuffer[0] = 0;
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_ERRORBUFFER, errorBuffer);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}

	// perform request
	try {
		this->curlCode = curl_easy_perform(this->curl);
	}
	catch(const std::exception& e) {
		this->errorMessage = e.what();
		return false;
	}
	if(this->curlCode != CURLE_OK) {
		if(errorBuffer[0] != 0) this->errorMessage = errorBuffer;
		else this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}

	// get response code
	long responseCodeL = 0;
	this->curlCode = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCodeL);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	else if(responseCodeL < 0 || responseCodeL > UINT_MAX) {
		this->errorMessage = "Invalid HTTP response code";
		return false;
	}
	this->responseCode = (unsigned int) responseCodeL;

	// check response code
	for(auto i = errors.begin(); i != errors.end(); ++i) {
		if(this->responseCode == *i) {
			std::ostringstream errStrStr;
			errStrStr << "HTTP error " << this->responseCode << " from " << url;
			this->errorMessage = errStrStr.str();
			return false;
		}
	}

	// get content type
	char * cString = NULL;
	this->curlCode = curl_easy_getinfo(this->curl, CURLINFO_CONTENT_TYPE, &cString);
	if(this->curlCode != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(this->curlCode);
		return false;
	}
	if(cString) this->contentType = cString;
	else this->contentType = "";

	// perform character encoding operations (convert ISO-8859-1 to UTF-8, remove invalid UTF-8 characters)
	std::string repairedContent;
	std::transform(this->contentType.begin(), this->contentType.end(), this->contentType.begin(), ::tolower);
	this->contentType.erase(std::remove_if(this->contentType.begin(), this->contentType.end(), isspace), this->contentType.end());
	if(this->contentType.find("charset=iso-8859-1") != std::string::npos) this->content = crawlservpp::Helper::Utf8::iso88591ToUtf8(this->content);
	if(crawlservpp::Helper::Utf8::repairUtf8(this->content, repairedContent)) this->content.swap(repairedContent);

	contentTo.swap(this->content);
	return true;
}

// get last response code
unsigned int Curl::getResponseCode() const {
	return this->responseCode;
}

// get last content type
std::string Curl::getContentType() const {
	return this->contentType;
}

// reset connection
void Curl::resetConnection(unsigned long sleep) {
	// cleanup cURL
	if(this->curl) {
		curl_easy_cleanup(this->curl);
		this->curl = NULL;
	}

	// cleanup lists
	if(this->dnsResolves) {
		curl_slist_free_all(this->dnsResolves);
		this->dnsResolves = NULL;
	}
	if(this->headers) {
		curl_slist_free_all(this->headers);
		this->headers = NULL;
	}
	if(this->http200Aliases) {
		curl_slist_free_all(this->http200Aliases);
		this->http200Aliases = NULL;
	}
	if(this->proxyHeaders) {
		curl_slist_free_all(this->proxyHeaders);
		this->proxyHeaders = NULL;
	}

	// sleep
	std::this_thread::sleep_for(std::chrono::milliseconds(sleep));

	// re-initialize cURL
	this->curl = curl_easy_init();
	if(!(this->curl)) throw std::runtime_error("Could not initialize cURL");

	// configure cURL (global defaults)
	bool error = false;
	this->curlCode = curl_easy_setopt(this->curl, CURLOPT_NOSIGNAL, 1L);
	if(this->curlCode != CURLE_OK) error = true;
	else {
		this->curlCode = curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, Curl::writer);
		if(this->curlCode != CURLE_OK) error = true;
		else {
			// set pointer to instance
			this->curlCode = curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, this);
			if(this->curlCode != CURLE_OK) error = true;
		}
	}

	// error handling
	if(error) {
		if(this->curl) {
			this->errorMessage = curl_easy_strerror(this->curlCode);
			curl_easy_cleanup(this->curl);
			this->curl = NULL;
		}
		curl_global_cleanup();
		throw std::runtime_error("Could not configure cURL");
	}

	// set configuration
	if(this->config) this->setConfigGlobal(*(this->config), this->limitedSettings, NULL);
}

// get last cURL code
CURLcode Curl::getCurlCode() const {
	return this->curlCode;
}

// get error message
const std::string& Curl::getErrorMessage() const {
	return this->errorMessage;
}

// escape a string
std::string Curl::escape(const std::string& stringToEscape, bool usePlusForSpace) {
	std::string result;

	if(!(this->curl) || stringToEscape.empty()) return "";

	char * cString = curl_easy_escape(this->curl, stringToEscape.c_str(), stringToEscape.length());

	if(cString) {
		result = std::string(cString);
		curl_free(cString);
	}

	if(usePlusForSpace) {
		unsigned long pos = 0;
		while(true) {
			pos = result.find("%20", pos);
			if(pos < result.length()) {
				result = result.substr(0, pos) + '+' + result.substr(pos + 3);
				pos++;
			}
			else break;
		}
	}
	return result;
}

// unescape an escaped string
std::string Curl::unescape(const std::string& escapedString, bool usePlusForSpace) {
	std::string result;

	if(!(this->curl) || escapedString.empty()) return "";
	char * cString = curl_easy_unescape(this->curl, escapedString.c_str(), escapedString.length(), NULL);

	if(cString) {
		result = std::string(cString);
		curl_free(cString);
	}

	if(usePlusForSpace) {
		unsigned long pos = 0;
		while(true) {
			pos = result.find("+", pos);
			if(pos < result.length()) {
				result.replace(pos, 1, " ");
				pos++;
			}
			else break;
		}
	}
	return result;
}

// escape an URL but leave reserved characters (; / ? : @ = & #) intact
std::string Curl::escapeUrl(const std::string& urlToEncode) {
	unsigned long pos = 0;
	std::string result;

	while(pos < urlToEncode.length()) {
		unsigned long end = urlToEncode.find_first_of(";/?:@=&#", pos);
		if(end == std::string::npos) end = urlToEncode.length();
		if(end - pos) {
			std::string part = urlToEncode.substr(pos, end - pos);
			char * CString = curl_easy_escape(this->curl, part.c_str(), part.length());
			if(CString) {
				result += std::string(CString);
				curl_free(CString);
			}
		}
		if(end < urlToEncode.length()) result += urlToEncode.at(end);
		pos = end + 1;
	}
	return result;
}

// static cURL writer function
int Curl::writer(char * data, unsigned long size, unsigned long nmemb, void * thisPointer) {
	if(!thisPointer) return 0;
	return static_cast<Curl *>(thisPointer)->writerInClass(data, size, nmemb);
}

// in-class cURL writer function
int Curl::writerInClass(char * data, unsigned long size, unsigned long nmemb) {
	this->content.append(data, size * nmemb);
	return size * nmemb;
}

}
