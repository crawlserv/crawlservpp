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
 * Curl.cpp
 *
 * Using the cURL library to provide networking functionality.
 *  This class is used by both the crawler and the extractor.
 *  NOT THREAD-SAFE! Use multiple instances for multiple threads.
 *
 *  Created on: Oct 8, 2018
 *      Author: ans
 */

#include "Curl.hpp"

namespace crawlservpp::Network {

	// constructor, throws Curl::Exception
	Curl::Curl(const std::string& cookieDirectory)
			: cookieDir(cookieDirectory),
			  curlCode(CURLE_OK),
			  responseCode(0),
			  limitedSettings(false),
			  post(false),
			  config(nullptr) {
		// check pointer to cURL instance
		if(!(this->curl))
			throw Curl::Exception("Could not initialize cURL");

		// configure cURL (global defaults)
		this->curlCode = curl_easy_setopt(this->curl.get(), CURLOPT_NOSIGNAL, 1L);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(this->curl.get(), CURLOPT_WRITEFUNCTION, Curl::writer);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		// set pointer to instance
		this->curlCode = curl_easy_setopt(this->curl.get(), CURLOPT_WRITEDATA, this);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));
	}

	// destructor stub
	Curl::~Curl() {}

	// set global network options from crawling configuration, throws Curl::Exception
	//  NOTE:	If limited is true, cookie settings, custom headers,
	// 			HTTP version and error responses will be ignored.
	void Curl::setConfigGlobal(
			const Config& globalConfig,
			bool limited,
			std::queue<std::string>& warningsTo
	) {
		if(!(this->curl))
			throw Curl::Exception("cURL not initialized");

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_MAXCONNECTS,
				static_cast<long>(globalConfig.connectionsMax)
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_IGNORE_CONTENT_LENGTH,
				globalConfig.contentLengthIgnore ? 1L : 0L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		if(globalConfig.cookies && !limited) {
			// add cookie directory to cookie files
			std::string loadCookiesFrom;
			std::string saveCookiesTo;

			if(!globalConfig.cookiesLoad.empty()) {
				loadCookiesFrom =
						this->cookieDir
						+ Helper::FileSystem::getPathSeparator()
						+ globalConfig.cookiesLoad;

				// check whether cookie file really is located in cookie directory
				if(!Helper::FileSystem::contains(this->cookieDir, loadCookiesFrom))
					throw Curl::Exception(
							"Cookie file \'"
							+ loadCookiesFrom
							+ "is not in directory \'"
							+ this->cookieDir
							+ "\'"
					);
			}

			if(!globalConfig.cookiesSave.empty()) {
				saveCookiesTo =
						this->cookieDir
						+ Helper::FileSystem::getPathSeparator()
						+ globalConfig.cookiesSave;

				// check whether cookie file really is located in cookie directory
				if(!Helper::FileSystem::contains(this->cookieDir, saveCookiesTo))
					throw Curl::Exception(
							"Cookie file \'"
							+ saveCookiesTo
							+ "is not in directory \'"
							+ this->cookieDir
							+ "\'"
					);
			}

			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_COOKIEFILE,
					loadCookiesFrom.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));

			if(!saveCookiesTo.empty()) {
				this->curlCode = curl_easy_setopt(
						this->curl.get(),
						CURLOPT_COOKIEJAR,
						saveCookiesTo.c_str()
				);

				if(this->curlCode != CURLE_OK)
					throw Curl::Exception(curl_easy_strerror(this->curlCode));
			}
		}

		if(!globalConfig.cookiesSession && !limited) {
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_COOKIESESSION,
					1L
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		if(!globalConfig.cookiesSet.empty() && !limited)
			this->setCookies(globalConfig.cookiesSet);

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_DNS_CACHE_TIMEOUT,
				globalConfig.dnsCacheTimeOut
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		if(!globalConfig.dnsDoH.empty()) {
	#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 62)
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_DOH_URL,
					globalConfig.dnsDoH.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
	#else
			warningsTo.emplace("DNS-over-HTTPS currently not supported, \'network.dns.doh\' ignored.");
	#endif
		}

		if(!globalConfig.dnsInterface.empty()) {
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_DNS_INTERFACE,
					globalConfig.dnsInterface.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		if(!globalConfig.dnsResolves.empty()) {
			this->dnsResolves.append(globalConfig.dnsResolves);

			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_RESOLVE,
					this->dnsResolves.get()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		if(!globalConfig.dnsServers.empty()) {
			std::string serverList;

			for(const auto& dnsServer : globalConfig.dnsServers)
				serverList += dnsServer + ",";

			serverList.pop_back();

			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_DNS_SERVERS,
					serverList.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}
	#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 60)
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_DNS_SHUFFLE_ADDRESSES,
				globalConfig.dnsShuffle ? 1L : 0L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));
	#else
		if(globalConfig.dnsShuffle)
			warningsTo.emplace("DNS shuffling currently not supported, \'network.dns.shuffle\' ignored.");
	#endif

		if(
				globalConfig.encodingBr
				&& globalConfig.encodingDeflate
				&& globalConfig.encodingGZip
				&& globalConfig.encodingIdentity
		) {
			this->curlCode = curl_easy_setopt(this->curl.get(), CURLOPT_ACCEPT_ENCODING, "");

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}
		else if(
				globalConfig.encodingBr
				|| globalConfig.encodingDeflate
				|| globalConfig.encodingGZip
				|| globalConfig.encodingIdentity
		) {
			std::string encodingList;

			if(globalConfig.encodingBr)
				encodingList += "br,";

			if(globalConfig.encodingDeflate)
				encodingList += "deflate,";

			if(globalConfig.encodingGZip)
				encodingList += "gzip,";

			if(globalConfig.encodingIdentity)
				encodingList += "identity,";

			encodingList.pop_back();

			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_ACCEPT_ENCODING,
					encodingList.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		if(globalConfig.encodingTransfer) {
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_TRANSFER_ENCODING,
					globalConfig.encodingTransfer ? 1L : 0L
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		if(!globalConfig.headers.empty() && !limited) {
			this->headers.append(globalConfig.headers);

			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_HTTPHEADER,
					this->headers.get()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		if(!globalConfig.http200Aliases.empty() && !limited) {
			this->http200Aliases.append(globalConfig.http200Aliases);

			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_HTTP200ALIASES,
					this->http200Aliases.get()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		if(!limited) {
			switch(globalConfig.httpVersion) {
			case Config::httpVersionAny:
				this->curlCode = curl_easy_setopt(
						this->curl.get(),
						CURLOPT_HTTP_VERSION,
						CURL_HTTP_VERSION_NONE
				);

				break;

			case Config::httpVersionV1:
				this->curlCode = curl_easy_setopt(
						this->curl.get(),
						CURLOPT_HTTP_VERSION,
						CURL_HTTP_VERSION_1_0
				);

				break;

			case Config::httpVersionV11:
				this->curlCode = curl_easy_setopt(
						this->curl.get(),
						CURLOPT_HTTP_VERSION,
						CURL_HTTP_VERSION_1_1
				);

				break;

			case Config::httpVersionV2:
	#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 33)
				this->curlCode = curl_easy_setopt(
						this->curl.get(),
						CURLOPT_HTTP_VERSION,
						CURL_HTTP_VERSION_2_0
				);
	#else
				warningsTo.emplace("HTTP 2.0 currently not supported, \'http.version\' ignored.");

				this->curlCode = CURLE_OK;
	#endif

				break;

			case Config::httpVersionV2only:
	#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 49)
				this->curlCode = curl_easy_setopt(
						this->curl.get(),
						CURLOPT_HTTP_VERSION,
						CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE
				);
	#else
				warningsTo.emplace("HTTP 2.0 ONLY currently not supported, \'http.version\' ignored.");

				this->curlCode = CURLE_OK;
	#endif

				break;

			case Config::httpVersionV2tls:
#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 47)
				this->curlCode = curl_easy_setopt(
						this->curl.get(),
						CURLOPT_HTTP_VERSION,
						CURL_HTTP_VERSION_2TLS
				);
#else
				warningsTo.emplace("HTTP 2.0 OVER TLS ONLY currently not supported, \'http.version\' ignored.");

				this->curlCode = CURLE_OK;
#endif

				break;

			default:
					warningsTo.emplace("Enum value for HTTP version not recognized, \'network.http.version\' ignored.");

				this->curlCode = CURLE_OK;
			}

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		if(!globalConfig.localInterface.empty()) {
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_INTERFACE,
					globalConfig.localInterface.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_LOCALPORT,
				static_cast<long>(globalConfig.localPort)
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_LOCALPORTRANGE,
				static_cast<long>(globalConfig.localPortRange)
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		if(!globalConfig.proxy.empty()) {
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_PROXY,
					globalConfig.proxy.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		if(!globalConfig.proxyAuth.empty()) {
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_PROXYUSERPWD ,
					globalConfig.proxyAuth.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		if(!globalConfig.proxyHeaders.empty()) {
			this->proxyHeaders.append(globalConfig.proxyHeaders);

			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_PROXYHEADER,
					this->proxyHeaders.get()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		if(!globalConfig.proxyPre.empty()) {
	#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 52)
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_PRE_PROXY,
					globalConfig.proxyPre.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
	#else
				warningsTo.emplace("Pre-Proxy currently not supported, \' proxy.pre\' ignored.");
	#endif
		}

		if(!globalConfig.proxyTlsSrpPassword.empty() || !globalConfig.proxyTlsSrpUser.empty()) {
	#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 52)
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_PROXY_TLSAUTH_USERNAME,
					globalConfig.proxyTlsSrpUser.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));

			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_PROXY_TLSAUTH_PASSWORD,
					globalConfig.proxyTlsSrpPassword.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
	#else
			warningsTo.emplace(
					"Proxy TLS authentication currently not supported,"
					" \'proxy.tls.srp.user\' and \'proxy.tls.srp.password\' ignored."
			);
	#endif
		}

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_HTTPPROXYTUNNEL,
				globalConfig.proxyTunnelling ? 1L : 0L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_FOLLOWLOCATION,
				globalConfig.redirect ? 1L : 0L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_MAXREDIRS ,
				static_cast<long>(globalConfig.redirectMax)
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		if(
				globalConfig.redirectPost301
				&& globalConfig.redirectPost302
				&& globalConfig.redirectPost303
		) {
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_POSTREDIR,
					CURL_REDIR_POST_ALL
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}
		else {
			long redirectPost = 0;

			if(globalConfig.redirectPost301)
				redirectPost |= CURL_REDIR_POST_301;

			if(globalConfig.redirectPost302)
				redirectPost |= CURL_REDIR_POST_302;

			if(globalConfig.redirectPost303)
				redirectPost |= CURL_REDIR_POST_303;

			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_POSTREDIR,
					redirectPost
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		if(!globalConfig.referer.empty() && !limited) {
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_REFERER,
					globalConfig.referer.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_AUTOREFERER,
				globalConfig.refererAutomatic ? 1L : 0L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_MAX_RECV_SPEED_LARGE,
				static_cast<long>(globalConfig.speedDownLimit)
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_LOW_SPEED_LIMIT,
				static_cast<long>(globalConfig.speedLowLimit)
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_LOW_SPEED_TIME,
				static_cast<long>(globalConfig.speedLowTime)
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_MAX_SEND_SPEED_LARGE,
				static_cast<long>(globalConfig.speedUpLimit)
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_SSL_VERIFYHOST,
				globalConfig.sslVerifyHost ? 2L : 0L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_SSL_VERIFYPEER,
				globalConfig.sslVerifyPeer ? 1L : 0L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

	#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 52)
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_PROXY_SSL_VERIFYHOST,
				globalConfig.sslVerifyProxyHost ? 2L : 0L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_PROXY_SSL_VERIFYPEER,
				globalConfig.sslVerifyProxyPeer ? 1L : 0L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));
	#else
		if(globalConfig.sslVerifyProxyHost || globalConfig.sslVerifyProxyPeer)
			warningsTo.emplace(
					"SLL verification of proxy host and peer currently not supported,"
					" \'ssl.verify.proxy.host\' and  \'ssl.verify.proxy.peer\' ignored."
			);
	#endif

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_SSL_VERIFYSTATUS,
				globalConfig.sslVerifyStatus ? 1L : 0L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

	#if linux && (LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 49))
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_TCP_FASTOPEN,
				globalConfig.tcpFastOpen ? 1L : 0L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));
	#else
		if(globalConfig.tcpFastOpen)
			warningsTo.emplace("TCP Fast Open currently not supported, \'tcp.fast.open\' ignored.");
	#endif

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_TCP_KEEPALIVE,
				globalConfig.tcpKeepAlive ? 1L : 0L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_TCP_KEEPIDLE,
				static_cast<long>(globalConfig.tcpKeepAliveIdle)
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_TCP_KEEPINTVL,
				static_cast<long>(globalConfig.tcpKeepAliveInterval)
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_TCP_NODELAY,
				globalConfig.tcpNagle ? 0L : 1L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_CONNECTTIMEOUT,
				static_cast<long>(globalConfig.timeOut)
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

	#if LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 59)
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS,
				static_cast<long>(globalConfig.timeOutHappyEyeballs)
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));
	#else
		if(globalConfig.timeOutHappyEyeballs)
			warningsTo.emplace(
					"Happy Eyeballs Configuration currently not supported,"
					" \'network.timeout.happyeyeballs\' ignored."
			);
	#endif

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_TIMEOUT,
				static_cast<long>(globalConfig.timeOutRequest)
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		if(!globalConfig.tlsSrpPassword.empty() || !globalConfig.tlsSrpUser.empty()) {
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_TLSAUTH_USERNAME,
					globalConfig.tlsSrpUser.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));

			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_TLSAUTH_PASSWORD,
					globalConfig.tlsSrpPassword.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		if(!globalConfig.userAgent.empty()) {
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_USERAGENT,
					globalConfig.userAgent.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_VERBOSE,
				globalConfig.verbose ? 1L : 0L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		// save configuration
		this->config = &globalConfig;
		this->limitedSettings = limited;
	}

	// set current network options from crawling configuration, throws Curl::Exception
	void Curl::setConfigCurrent(const Config& currentConfig) {
		// overwrite cookies
		for(const auto& cookie : currentConfig.cookiesOverwrite) {
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_COOKIELIST,
					cookie.c_str()
			);

			if(this->curlCode != CURLE_OK)
				throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}
	}

	// set custom cookies (independent from cookie engine), throws Curl::Exception
	//  WARNING: custom cookies will be lost if connection is reset
	void Curl::setCookies(const std::string& cookies) {
		if(cookies.empty())
			// reset cookies if string is empty
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_COOKIE,
					nullptr
			);
		else
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_COOKIE,
					cookies.c_str()
			);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->oldCookies = this->tmpCookies;
		this->tmpCookies = cookies;
	}

	// set custom headers, throws Curl::Exception
	//  WARNING: custom headers will be lost if connection is reset
	void Curl::setHeaders(const std::vector<std::string>& customHeaders) {
		// clear old temporary headers if necessary
		this->tmpHeaders.reset();

		if(customHeaders.empty())
			// reset headers if vector is empty
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_HTTPHEADER,
					this->headers.get()
			);
		else {
			// temporarily combine global and current headers
			this->tmpHeaders.append(this->headers);
			this->tmpHeaders.append(customHeaders);

			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_HTTPHEADER,
					this->tmpHeaders.get()
			);
		}

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));
	}

	// enable or disable verbose mode (WARNING: overrides any configuration!)
	void Curl::setVerbose(bool isVerbose) {
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_VERBOSE,
				isVerbose
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));
	}

	// unset custom cookies (independent from cookie engine), throws Curl::Exception
	void Curl::unsetCookies() {
		if(this->oldCookies.empty()) {
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_COOKIE,
					nullptr
			);

			this->tmpCookies.clear();
		}
		else {
			this->curlCode = curl_easy_setopt(
					this->curl.get(),
					CURLOPT_COOKIE,
					this->oldCookies.c_str()
			);

			this->tmpCookies = this->oldCookies;

			this->oldCookies.clear();
		}

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));
	}

	// unset custom headers, throws Curl::Exception
	void Curl::unsetHeaders() {
		// clear temporary headers if necessary
		this->tmpHeaders.reset();

		// reset headers
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_HTTPHEADER,
				this->headers.get()
		);
	}

	// get remote content, throws Curl::Exception
	void Curl::getContent(
			const std::string& url,
			bool usePost,
			std::string& contentTo,
			const std::vector<unsigned int>& errors
	) {
		std::string escapedUrl;
		char errorBuffer[CURL_ERROR_SIZE];

		this->content.clear();
		this->contentType.clear();

		this->responseCode = 0;

		// check whether the explicit setting of the HTTP method is needed
		if(usePost) {
			const auto delim = url.find('?');

			if(delim == std::string::npos) {
				// no POST data found: escape whole URL
				escapedUrl = this->escapeUrl(url);

				if(!(this->post)) {
					// set POST method
					this->curlCode = curl_easy_setopt(
							this->curl.get(),
							CURLOPT_POST,
							1L
					);

					if(this->curlCode != CURLE_OK)
						throw Curl::Exception(curl_easy_strerror(this->curlCode));
				}
			}
			else {
				// get POST data
				const std::string postFields(url, delim + 1);

				// remove POST data from URL (and escape it)
				escapedUrl = this->escapeUrl(url.substr(0, delim));

				// set POST data size
				this->curlCode = curl_easy_setopt(
						this->curl.get(),
						CURLOPT_POSTFIELDSIZE,
						postFields.length()
				);

				if(this->curlCode != CURLE_OK)
					throw Curl::Exception(curl_easy_strerror(this->curlCode));

				// set POST data
				this->curlCode = curl_easy_setopt(
						this->curl.get(),
						CURLOPT_POSTFIELDS,
						postFields.c_str()
				);

				std::cout << "\n" << postFields << "[" << postFields.length() << "]" << std::flush;

				if(this->curlCode != CURLE_OK)
					throw Curl::Exception(curl_easy_strerror(this->curlCode));
			}

			this->post = true;
		}
		else {
			// using GET: escape whole URL
			escapedUrl = this->escapeUrl(url);

			if(this->post) {
				// unset POST method
				this->curlCode = curl_easy_setopt(
						this->curl.get(),
						CURLOPT_POST,
						0L
				);

				if(this->curlCode != CURLE_OK)
					throw Curl::Exception(curl_easy_strerror(this->curlCode));

				this->post = false;
			}
		}

		// set URL
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_URL,
				escapedUrl.c_str()
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		// set error buffer
		errorBuffer[0] = 0;

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_ERRORBUFFER,
				errorBuffer
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		// perform request
		try {
			this->curlCode = curl_easy_perform(this->curl.get());
		}
		catch(const std::exception& e) {
			throw Curl::Exception(e.what());
		}

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		// get response code
		long responseCodeL = 0;

		this->curlCode = curl_easy_getinfo(
				this->curl.get(),
				CURLINFO_RESPONSE_CODE,
				&responseCodeL
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));
		else if(responseCodeL < 0 || responseCodeL > UINT_MAX)
			throw Curl::Exception("Invalid HTTP response code");

		this->responseCode = static_cast<unsigned int>(responseCodeL);

		// check response code
		if(
				std::find(
						errors.begin(),
						errors.end(),
						this->responseCode
				) != errors.end()
		)
			throw Curl::Exception(
					"HTTP error "
					+ std::to_string(this->responseCode)
					+ " from "
					+ url
			);

		// get content type
		char * cString = nullptr;

		this->curlCode = curl_easy_getinfo(
				this->curl.get(),
				CURLINFO_CONTENT_TYPE,
				&cString
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		if(cString)
			this->contentType = cString;
		else
			this->contentType = "";

		// perform character encoding operations (convert ISO-8859-1 to UTF-8, remove invalid UTF-8 characters)
		std::string repairedContent;

		std::transform(
				this->contentType.begin(),
				this->contentType.end(),
				this->contentType.begin(), ::tolower
		);

		this->contentType.erase(
				std::remove_if(
						this->contentType.begin(),
						this->contentType.end(),
						::isspace
				),
				this->contentType.end()
		);

		if(this->contentType.find("charset=iso-8859-1") != std::string::npos)
			this->content = Helper::Utf8::iso88591ToUtf8(this->content);

		if(Helper::Utf8::repairUtf8(this->content, repairedContent))
			this->content.swap(repairedContent);

		contentTo.swap(this->content);
	}

	// get last response code
	unsigned int Curl::getResponseCode() const {
		return this->responseCode;
	}

	// get last content type
	std::string Curl::getContentType() const {
		return this->contentType;
	}

	// reset connection, throws Curl::Exception
	void Curl::resetConnection(unsigned long sleep) {
		// cleanup lists
		this->dnsResolves.reset();
		this->headers.reset();
		this->tmpHeaders.reset();
		this->http200Aliases.reset();
		this->proxyHeaders.reset();

		// cleanup cURL
		this->curl.reset();

		// sleep
		std::this_thread::sleep_for(std::chrono::milliseconds(sleep));

		// re-initialize cURL
		this->curl.init();

		// configure cURL (global defaults)
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_NOSIGNAL,
				1L
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_WRITEFUNCTION,
				Curl::writer
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		// set pointer to instance
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_WRITEDATA,
				this
		);

		if(this->curlCode != CURLE_OK)
			throw Curl::Exception(curl_easy_strerror(this->curlCode));

		// set configuration
		if(this->config) {
			std::queue<std::string> logDump; // do not log warnings another time

			this->setConfigGlobal(*(this->config), this->limitedSettings, logDump);
		}
	}

	// get last cURL code
	CURLcode Curl::getCurlCode() const {
		return this->curlCode;
	}

	// escape a string
	std::string Curl::escape(const std::string& stringToEscape, bool usePlusForSpace) {
		if(!(this->curl) || stringToEscape.empty())
			return "";

		std::string result(
				Curl::curlStringToString(
						curl_easy_escape(
								this->curl.get(),
								stringToEscape.c_str(),
								stringToEscape.length()
						)
				)
		);

		if(usePlusForSpace) {
			unsigned long pos = 0;

			while(true) {
				pos = result.find("%20", pos);

				if(pos < result.length()) {
					result = result.substr(0, pos) + '+' + result.substr(pos + 3);

					++pos;
				}
				else
					break;
			}
		}
		return result;
	}

	// unescape an escaped string
	std::string Curl::unescape(const std::string& escapedString, bool usePlusForSpace) {
		if(!(this->curl) || escapedString.empty())
			return "";

		std::string result(
				Curl::curlStringToString(
						curl_easy_unescape(
								this->curl.get(),
								escapedString.c_str(),
								escapedString.length(),
								nullptr
						)
				)
		);

		if(usePlusForSpace) {
			unsigned long pos = 0;

			while(true) {
				pos = result.find("+", pos);

				if(pos < result.length()) {
					result.replace(pos, 1, " ");

					++pos;
				}
				else
					break;
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

			if(end == std::string::npos)
				end = urlToEncode.length();
			if(end - pos) {
				const std::string part(urlToEncode, pos, end - pos);

				result += Curl::curlStringToString(curl_easy_escape(this->curl.get(), part.c_str(), part.length()));
			}

			if(end < urlToEncode.length())
				result += urlToEncode.at(end);

			pos = end + 1;
		}

		return result;
	}

	// static helper function for converting cURL string to std::string and releasing its memory
	std::string Curl::curlStringToString(char * curlString) {
		if(curlString) {
			const std::string newString(curlString);

			curl_free(curlString);

			return newString;
		}

		return "";
	}

	// static cURL writer function
	int Curl::writer(char * data, unsigned long size, unsigned long nmemb, void * thisPointer) {
		if(!thisPointer)
			return 0;

		return static_cast<Curl *>(thisPointer)->writerInClass(data, size, nmemb);
	}

	// in-class cURL writer function
	int Curl::writerInClass(char * data, unsigned long size, unsigned long nmemb) {
		this->content.append(data, size * nmemb);

		return size * nmemb;
	}

} /* crawlservpp::Network */
