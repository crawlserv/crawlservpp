/*
 * Networking.cpp
 *
 * A class using the cURL library to provide networking functionality.
 * NOT THREAD-SAFE! Use multiple instances for multiple threads.
 *
 *  Created on: Oct 8, 2018
 *      Author: ans
 */

#include "Networking.h"

bool Networking::globalInit = false;

// constructor
Networking::Networking() {
	bool error = false;

	// set default values
	this->dnsResolves = NULL;
	this->headers = NULL;
	this->http200Aliases = NULL;
	this->proxyHeaders = NULL;
	this->curlCode = CURLE_OK;
	this->responseCode = 0;
	this->configCrawler = NULL;
	this->limitedSettings = false;

	// initialize networking if necessary
	if(Networking::globalInit) this->localInit = false;
	else {
		Networking::globalInit = true;
		this->localInit = true;
		curl_global_init(CURL_GLOBAL_ALL);
	}

	// initialize cURL
	this->curl = curl_easy_init();
	if(!this->curl) throw std::runtime_error("Could not initialize cURL");

	// configure cURL (global defaults)
	CURLcode result = curl_easy_setopt(this->curl, CURLOPT_NOSIGNAL, 1L);
	if(result != CURLE_OK) error = true;
	else {
		result = curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, Networking::cURLWriter);
		if(result != CURLE_OK) error = true;
		else {
			// set pointer to instance
			result = curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, this);
			if(result != CURLE_OK) error = true;
		}
	}

	// error handling
	if(error) {
		if(this->curl) {
			this->errorMessage = curl_easy_strerror(result);
			curl_easy_cleanup(this->curl);
			this->curl = NULL;
		}
		curl_global_cleanup();
		throw std::runtime_error("Could not configure cURL");
	}
}

// destructor
Networking::~Networking() {
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
	if(Networking::globalInit && this->localInit) {
		curl_global_cleanup();
		Networking::globalInit = false;
		this->localInit = false;
	}
}

// set global network options from crawling configuration
// if limited is set, cookie settings, custom headers, HTTP version and error responses will be ignored
bool Networking::setCrawlingConfigGlobal(const ConfigCrawler& config, bool limited, std::vector<std::string> * warningsTo) {
	if(!(this->curl)) {
		this->errorMessage = "cURL not initialized";
		return false;
	}

	CURLcode result = curl_easy_setopt(this->curl, CURLOPT_MAXCONNECTS, (long) config.networkConnectionsMax);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_IGNORE_CONTENT_LENGTH, config.networkContentLengthIgnore ? 1L : 0L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	if(config.networkCookies && !limited) {
		result = curl_easy_setopt(this->curl, CURLOPT_COOKIEFILE, config.networkCookiesLoad.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
		if(config.networkCookiesSave.c_str()) {
			result = curl_easy_setopt(this->curl, CURLOPT_COOKIEJAR, config.networkCookiesSave.c_str());
			if(result != CURLE_OK) {
				this->errorMessage = curl_easy_strerror(result);
				return false;
			}
		}
	}
	if(!config.networkCookiesSession && !limited) {
		result = curl_easy_setopt(this->curl, CURLOPT_COOKIESESSION, 1L);
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	if(config.networkCookiesSet.length() && !limited) {
		result = curl_easy_setopt(this->curl, CURLOPT_COOKIE, config.networkCookiesSet.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	result = curl_easy_setopt(this->curl, CURLOPT_DNS_CACHE_TIMEOUT, config.networkDnsCacheTimeOut);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	if(config.networkDnsDoH.length()) {
#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 62)
		result = curl_easy_setopt(this->curl, CURLOPT_DOH_URL, config.networkDnsDoH.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
#else
		if(warningsTo) warningsTo->push_back("DNS-over-HTTPS currently not supported, \'network.dns.doh\' ignored.");
#endif
	}
	if(config.networkDnsInterface.length()) {
		result = curl_easy_setopt(this->curl, CURLOPT_DNS_INTERFACE, config.networkDnsInterface.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	if(config.networkDnsResolves.size()) {
		for(auto i = config.networkDnsResolves.begin(); i != config.networkDnsResolves.end(); ++i)
			this->dnsResolves = curl_slist_append(this->dnsResolves, i->c_str());
		result = curl_easy_setopt(this->curl, CURLOPT_RESOLVE, this->dnsResolves);
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	if(config.networkDnsServers.size()) {
		std::string serverList;
		for(auto i = config.networkDnsServers.begin(); i != config.networkDnsServers.end(); ++i)
			serverList += *i + ",";
		serverList.pop_back();
		result = curl_easy_setopt(this->curl, CURLOPT_DNS_SERVERS, serverList.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 60)
	result = curl_easy_setopt(this->curl, CURLOPT_DNS_SHUFFLE_ADDRESSES, config.networkDnsShuffle ? 1L : 0L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
#else
	if(config.networkDnsShuffle && warningsTo)
		warningsTo->push_back("DNS shuffling currently not supported, \'network.dns.shuffle\' ignored.");
#endif
	if(config.networkEncodingBr && config.networkEncodingDeflate && config.networkEncodingGZip && config.networkEncodingIdentity) {
		result = curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	else if(config.networkEncodingBr || config.networkEncodingDeflate || config.networkEncodingGZip || config.networkEncodingIdentity) {
		std::string encodingList;
		if(config.networkEncodingBr) encodingList += "br,";
		if(config.networkEncodingDeflate) encodingList += "deflate,";
		if(config.networkEncodingGZip) encodingList += "gzip,";
		if(config.networkEncodingIdentity) encodingList += "identity,";
		encodingList.pop_back();
		result = curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, encodingList.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	if(config.networkEncodingTransfer) {
		result = curl_easy_setopt(this->curl, CURLOPT_TRANSFER_ENCODING, config.networkEncodingTransfer ? 1L : 0L);
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	if(config.networkHeaders.size() && !limited) {
		for(auto i = config.networkHeaders.begin(); i != config.networkHeaders.end(); ++i)
			this->headers = curl_slist_append(this->headers, i->c_str());
		result = curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, this->dnsResolves);
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	if(config.networkHttp200Aliases.size() && !limited) {
		for(auto i = config.networkHttp200Aliases.begin(); i != config.networkHttp200Aliases.end(); ++i)
			this->http200Aliases = curl_slist_append(this->http200Aliases, i->c_str());
		result = curl_easy_setopt(this->curl, CURLOPT_HTTP200ALIASES, this->http200Aliases);
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	if(!limited) {
		switch(config.networkHttpVersion) {
		case ConfigCrawler::networkHttpVersionAny:
			result = curl_easy_setopt(this->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_NONE);
			break;
		case ConfigCrawler::networkHttpVersionV1:
			result = curl_easy_setopt(this->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
			break;
		case ConfigCrawler::networkHttpVersionV11:
			result = curl_easy_setopt(this->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
			break;
		case ConfigCrawler::networkHttpVersionV2:
			result = curl_easy_setopt(this->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
			break;
		case ConfigCrawler::networkHttpVersionV2only:
			result = curl_easy_setopt(this->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
			break;
		case ConfigCrawler::networkHttpVersionV2tls:
			result = curl_easy_setopt(this->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
			break;
		default:
			if(warningsTo) warningsTo->push_back("Enum value for HTTP version not recognized, \'network.http.version\' ignored.");
			result = CURLE_OK;
		}
	}
	if(config.networkLocalInterface.length()) {
		result = curl_easy_setopt(this->curl, CURLOPT_INTERFACE, config.networkLocalInterface.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	result = curl_easy_setopt(this->curl, CURLOPT_LOCALPORT, (long) config.networkLocalPort);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_LOCALPORTRANGE, (long) config.networkLocalPortRange);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	if(config.networkProxy.length()) {
		result = curl_easy_setopt(this->curl, CURLOPT_PROXY, config.networkProxy.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	if(config.networkProxyAuth.length()) {
		result = curl_easy_setopt(this->curl, CURLOPT_PROXYUSERPWD , config.networkProxyAuth.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	if(config.networkProxyHeaders.size()) {
		for(auto i = config.networkProxyHeaders.begin(); i != config.networkProxyHeaders.end(); ++i)
			this->headers = curl_slist_append(this->proxyHeaders, i->c_str());
		result = curl_easy_setopt(this->curl, CURLOPT_PROXYHEADER, this->proxyHeaders);
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	if(config.networkProxyPre.length()) {
		result = curl_easy_setopt(this->curl, CURLOPT_PRE_PROXY, config.networkProxyPre.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	if(config.networkProxyTlsSrpPassword.length() || config.networkProxyTlsSrpUser.length()) {
		result = curl_easy_setopt(this->curl, CURLOPT_PROXY_TLSAUTH_USERNAME, config.networkProxyTlsSrpUser.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
		result = curl_easy_setopt(this->curl, CURLOPT_PROXY_TLSAUTH_PASSWORD, config.networkProxyTlsSrpPassword.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	result = curl_easy_setopt(this->curl, CURLOPT_HTTPPROXYTUNNEL, config.networkProxyTunnelling ? 1L : 0L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_FOLLOWLOCATION, config.networkRedirect ? 1L : 0L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_MAXREDIRS , (long) config.networkRedirectMax);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	if(config.networkRedirectPost301 && config.networkRedirectPost302 && config.networkRedirectPost303) {
		result = curl_easy_setopt(this->curl, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	else {
		long redirectPost = 0;
		if(config.networkRedirectPost301) redirectPost |= CURL_REDIR_POST_301;
		if(config.networkRedirectPost302) redirectPost |= CURL_REDIR_POST_302;
		if(config.networkRedirectPost303) redirectPost |= CURL_REDIR_POST_303;
		result = curl_easy_setopt(this->curl, CURLOPT_POSTREDIR, redirectPost);
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	if(config.networkReferer.length() && !limited) {
		result = curl_easy_setopt(this->curl, CURLOPT_REFERER, config.networkReferer.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	result = curl_easy_setopt(this->curl, CURLOPT_AUTOREFERER, config.networkRefererAutomatic ? 1L : 0L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_MAX_RECV_SPEED_LARGE, (long) config.networkSpeedDownLimit);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_LOW_SPEED_LIMIT, (long) config.networkSpeedLowLimit);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_LOW_SPEED_TIME, (long) config.networkSpeedLowTime);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_MAX_SEND_SPEED_LARGE, (long) config.networkSpeedUpLimit);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}

	result = curl_easy_setopt(this->curl, CURLOPT_SSL_VERIFYHOST, config.networkSslVerifyHost ? 2L : 0L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}

	result = curl_easy_setopt(this->curl, CURLOPT_SSL_VERIFYPEER, config.networkSslVerifyPeer ? 1L : 0L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_PROXY_SSL_VERIFYHOST, config.networkSslVerifyProxyHost ? 2L : 0L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_PROXY_SSL_VERIFYPEER, config.networkSslVerifyProxyPeer ? 1L : 0L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_SSL_VERIFYSTATUS, config.networkSslVerifyStatus ? 1L : 0L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_TCP_FASTOPEN, config.networkTcpFastOpen ? 1L : 0L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_TCP_KEEPALIVE, config.networkTcpKeepAlive ? 1L : 0L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_TCP_KEEPIDLE, (long) config.networkTcpKeepAliveIdle);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_TCP_KEEPINTVL, (long) config.networkTcpKeepAliveInterval);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_TCP_NODELAY, config.networkTcpNagle ? 0L : 1L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	result = curl_easy_setopt(this->curl, CURLOPT_CONNECTTIMEOUT, (long) config.networkTimeOut);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 59)
	result = curl_easy_setopt(this->curl, CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS, (long) config.networkTimeOutHappyEyeballs);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
#else
	if(config.networkTimeOutHappyEyeballs && warningsTo) warningsTo->push_back("Happy Eyeballs configuration currently not supported,"
			" \'network.timeout.happyeyeballs\' ignored.");
#endif
	result = curl_easy_setopt(this->curl, CURLOPT_TIMEOUT, (long) config.networkTimeOutRequest);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	if(config.networkTlsSrpPassword.length() || config.networkTlsSrpUser.length()) {
		result = curl_easy_setopt(this->curl, CURLOPT_TLSAUTH_USERNAME, config.networkTlsSrpUser.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
		result = curl_easy_setopt(this->curl, CURLOPT_TLSAUTH_PASSWORD, config.networkTlsSrpPassword.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	if(config.networkUserAgent.length()) {
		result = curl_easy_setopt(this->curl, CURLOPT_USERAGENT, config.networkUserAgent.c_str());
		if(result != CURLE_OK) {
			this->errorMessage = curl_easy_strerror(result);
			return false;
		}
	}
	result = curl_easy_setopt(this->curl, CURLOPT_VERBOSE, config.networkVerbose ? 1L : 0L);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}

	// save configuration
	this->configCrawler = &config;
	this->limitedSettings = limited;

	return true;
}

// set current network options from crawling configuration
bool Networking::setCrawlingConfigCurrent(const ConfigCrawler& config) {
	//TODO
	return true;
}

// get remote content
bool Networking::getContent(const std::string& url, std::string& contentTo, const std::vector<unsigned int>& errors) {
	std::string encodedUrl = this->escapeUrl(url);
	char errorBuffer[CURL_ERROR_SIZE];
	this->content.clear();
	this->contentType.clear();
	this->responseCode = 0;

	// set URL
	CURLcode result = curl_easy_setopt(this->curl, CURLOPT_URL, encodedUrl.c_str());
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}

	// set error buffer
	errorBuffer[0] = 0;
	result = curl_easy_setopt(this->curl, CURLOPT_ERRORBUFFER, errorBuffer);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}

	// perform request
	try {
		result = curl_easy_perform(this->curl);
	}
	catch(const std::exception& e) {
		this->errorMessage = e.what();
		return false;
	}
	if(result != CURLE_OK) {
		if(errorBuffer[0] != 0) this->errorMessage = errorBuffer;
		else this->errorMessage = curl_easy_strerror(result);
		return false;
	}

	// get response code
	long responseCodeL = 0;
	result = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCodeL);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
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
			errStrStr << "HTTP error " << this->responseCode << " from " << url << ".";
			this->errorMessage = errStrStr.str();
			return false;
		}
	}

	// get content type
	char * cString = NULL;
	result = curl_easy_getinfo(this->curl, CURLINFO_CONTENT_TYPE, &cString);
	if(result != CURLE_OK) {
		this->errorMessage = curl_easy_strerror(result);
		return false;
	}
	if(cString) this->contentType = cString;
	else this->contentType = "";

	// perform character encoding operations (convert ISO-8859-1 to UTF-8, remove invalid UTF-8 characters)
	std::string repairedContent;
	std::transform(this->contentType.begin(), this->contentType.end(), this->contentType.begin(), ::tolower);
	this->contentType.erase(std::remove_if(this->contentType.begin(), this->contentType.end(), isspace), this->contentType.end());
	if(this->contentType.find("charset=iso-8859-1") != std::string::npos) this->content = Helpers::iso88591ToUtf8(this->content);
	if(Helpers::repairUtf8(this->content, repairedContent)) this->content.swap(repairedContent);

	contentTo.swap(this->content);
	return true;
}

// get last response code
unsigned int Networking::getResponseCode() const {
	return this->responseCode;
}

// get last content type
std::string Networking::getContentType() const {
	return this->contentType;
}

// reset connection
void Networking::resetConnection(unsigned long sleep) {
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
	if(!this->curl) throw std::runtime_error("Could not initialize cURL");

	// configure cURL (global defaults)
	bool error = false;
	CURLcode result = curl_easy_setopt(this->curl, CURLOPT_NOSIGNAL, 1L);
	if(result != CURLE_OK) error = true;
	else {
		result = curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, Networking::cURLWriter);
		if(result != CURLE_OK) error = true;
		else {
			// set pointer to instance
			result = curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, this);
			if(result != CURLE_OK) error = true;
		}
	}

	// error handling
	if(error) {
		if(this->curl) {
			this->errorMessage = curl_easy_strerror(result);
			curl_easy_cleanup(this->curl);
			this->curl = NULL;
		}
		curl_global_cleanup();
		throw std::runtime_error("Could not configure cURL");
	}

	// set configuration
	if(this->configCrawler) this->setCrawlingConfigGlobal(*(this->configCrawler), this->limitedSettings, NULL);
}

// get error message
const std::string& Networking::getErrorMessage() const {
	return this->errorMessage;
}

// escape a string
std::string Networking::escape(const std::string& stringToEscape, bool usePlusForSpace) {
	std::string result;

	if(!this->curl || !(stringToEscape.length())) return "";

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
std::string Networking::unescape(const std::string& escapedString, bool usePlusForSpace) {
	std::string result;

	if(!this->curl || !(escapedString.length())) return "";
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
std::string Networking::escapeUrl(const std::string& urlToEncode) {
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
int Networking::cURLWriter(char * data, unsigned long size, unsigned long nmemb, void * thisPointer) {
	if(!thisPointer) return 0;
	return static_cast<Networking*>(thisPointer)->cURLWriterInClass(data, size, nmemb);
}

// in-class cURL writer function
int Networking::cURLWriterInClass(char * data, unsigned long size, unsigned long nmemb) {
	this->content.append(data, size * nmemb);
	return size * nmemb;
}
