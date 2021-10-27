/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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
 * Using the libcurl library to provide networking functionality.
 *  This class is used by both the crawler and the extractor.
 *  NOT THREAD-SAFE! Use multiple instances for multiple threads.
 *
 *  Created on: Oct 8, 2018
 *      Author: ans
 */

#ifndef NETWORK_CURL_HPP_
#define NETWORK_CURL_HPP_

#include "Config.hpp"

#include "../Data/Compression/Gzip.hpp"
#include "../Helper/FileSystem.hpp"
#include "../Helper/Utf8.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/NetworkSettings.hpp"
#include "../Wrapper/Curl.hpp"
#include "../Wrapper/CurlList.hpp"

#ifndef CRAWLSERVPP_TESTING

#include "../Helper/Portability/curl.h"

#else

#include "FakeCurl/FakeCurl.hpp"

#endif

#include <algorithm>	// std::find, std::remove_if, std::transform
#include <array>		// std::array
#include <cctype>		// std::isspace, std::tolower
#include <chrono>		// std::chrono
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint32_t, std::uint64_t
#include <exception>	// std::exception
#include <functional>	// std::function
#include <limits>		// std::numeric_limits
#include <queue>		// std::queue
#include <string>		// std::string, std::to_string
#include <string_view>	// std::string_view
#include <thread>		// std::this_thread
#include <vector>		// std::vector

namespace crawlservpp::Network {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! URL encoding of a space.
	inline constexpr auto encodedSpace{"%20"};

	//! Length of a URL encoded space.
	inline constexpr auto encodedSpaceLength{3};

	//! The number of milliseconds to sleep before re-checking the status of the application.
	inline constexpr auto checkEveryMilliseconds{100};

	//! Reserved characters to be ignored when escaping a URL.
	inline constexpr auto reservedCharacters{";/?:@=&#"};

	//! @c libcurl version needed for DNS-over-HTTPS support, i.e. @c 7.62.0.
	inline constexpr auto versionDoH{0x073E00};

	//! @c libcurl version needed for DNS shuffling, i.e. @c 7.60.0.
	inline constexpr auto versionDnsShuffle{0x073C00};

	//! @c libcurl version needed for Brotli encoding, i.e. @c 7.57.0.
	inline constexpr auto versionBrotli{0x073900};

	//! @c libcurl version needed for zstd encoding, i.e. @c 7.72.0.
	inline constexpr auto versionZstd{0x074800};

	//! @c libcurl version needed for HTTP/2 support, i.e. @c 7.33.0.
	inline constexpr auto versionHttp2{0x072100};

	//! @c libcurl version needed for HTTP/2 ONLY support, i.e. @c 7.49.0.
	inline constexpr auto versionHttp2Only{0x073100};

	//! @c libcurl version needed for HTTP/2 OVER TLS ONLY support, i.e. @c 7.47.0.
	inline constexpr auto versionHttp2Tls{0x072F00};

	//! @c libcurl version needed for HTTP/3 ONLY support, i.e. @c 7.66.0.
	inline constexpr auto versionHttp3Only{0x074200};

	//! @c libcurl version needed for pre-proxy support, i.e. @c 7.52.0.
	inline constexpr auto versionPreProxy{0x073400};

	//! @c libcurl version needed for proxy TLS authentification, i.e. @c 7.52.0.
	inline constexpr auto versionProxyTlsAuth{0x073400};

	//! @c libcurl authentification type for the proxy TLS-SRP authentification.
	inline constexpr auto authTypeTlsSrp{"SRP"};

	//! @c libcurl version needed for SSL verification of proxy host and peer, i.e. @c 7.52.0.
	inline constexpr auto versionProxySslVerify{0x073400};

	//! @c libcurl version needed for TCP Fast Open support, i.e. @c 7.49.0.
	inline constexpr auto versionTcpFastOpen{0x073100};

	//! @c libcurl version needed for the Happy Eyeballs algorithm, i.e. @c 7.59.0.
	inline constexpr auto versionHappyEyeballs{0x073B00};

	//! URL to retrieve the IP of the server from.
	inline constexpr auto getPublicIpFrom{"https://myexternalip.com/raw"};

	//! Errors when retrieving the IP of the server.
	inline constexpr std::array getPublicIpErrors{429, 502, 503, 504, 521, 522, 524};

	//! Name of the @c X-ts header.
	inline constexpr auto xTsHeaderName{"X-ts: "};

	//! Length of the @c X-ts header name, in bytes.
	inline constexpr auto xTsHeaderNameLen{6};

	//! GZIP magic number
	inline constexpr std::array gzipMagicNumber{0x1f, 0x8b};

	///@}

	/*
	 * DECLARATION
	 */

	//! Provides an interface to the @c libcurl library for sending and receiving data over the network.
	/*!
	 * This class is used by both the crawler and the extractor.
	 *
	 * It is **not thread-safe**, which means you need to use
	 *  multiple instances for multiple threads.
	 *
	 * Internally, the class uses Wrapper::Curl to interface
	 *  with the @c libcurl library.
	 *
	 * For more information about the @c libcurl library, see its
	 *  <a href="https://curl.haxx.se/libcurl/c/">website</a>.
	 */
	class Curl {
		// for convenience
		using NetworkSettings = Struct::NetworkSettings;

		using CurlList = Wrapper::CurlList;

		using IsRunningCallback = std::function<bool()>;

	public:
		///@name Construction and Destruction
		///@{

		Curl(
				std::string_view cookieDirectory,
				const NetworkSettings& setNetworkSettings
		);

		//! Default destructor.
		virtual ~Curl() = default;

		///@}
		///@name Setters
		///@{

		void setConfigGlobal(
				const Config& globalConfig,
				bool limited,
				std::queue<std::string>& warningsTo
		);
		void setConfigCurrent(const Config& currentConfig);
		void setCookies(const std::string& cookies);
		void setHeaders(const std::vector<std::string>& customHeaders);
		void setVerbose(bool isVerbose);
		void unsetCookies();
		void unsetHeaders();

		///@}
		///@name Getters
		///@{

		void getContent(
				std::string_view url,
				bool usePost,
				std::string& contentTo,
				const std::vector<std::uint32_t>& errors
		);
		[[nodiscard]] std::uint32_t getResponseCode() const noexcept;
		[[nodiscard]] std::string getContentType() const noexcept;
		[[nodiscard]] CURLcode getCurlCode() const noexcept;
		[[nodiscard]] std::string getPublicIp();

		///@}
		///@name Reset
		///@{

		void resetConnection(
				std::uint64_t sleepForMilliseconds,
				const IsRunningCallback& isRunningCallback
		);

		///@}
		///@name URL Encoding
		///@{

		[[nodiscard]] std::string escape(
				const std::string& stringToEscape,
				bool usePlusForSpace
		);
		[[nodiscard]] std::string unescape(
				const std::string& escapedString,
				bool usePlusForSpace
		);
		[[nodiscard]] std::string escapeUrl(std::string_view urlToEscape);

		///@}

		//! Class for @c libcurl exceptions
		/*!
		 * This exception is being thrown when
		 * - the @c libcurl library could not be initialized
		 * - the used @c libcurl library does not support SSL
		 * - a specified cookie file has not be found
		 * - a HTTP request failed
		 * - information about a HTTP request could not be extracted
		 * - an invalid HTTP response code has been received
		 * - one of the HTTP response codes specified as errors has been received
		 * - URL encoding/decoding of a non-empty string has been attempted
		 *    without successfully initialized @c libcurl library
		 * - any specific @c libcurl option could not be set
		 *
		 * Use getCurlCode() to get the error code returned by the API, if available.
		 */
		MAIN_EXCEPTION_CLASS();

		/**@name Copy and Move
		 * The class is not copyable and not moveable.
		 */
		///@{

		//! Deleted copy constructor.
		Curl(Curl&) = delete;

		//! Deleted copy assignment operator.
		Curl& operator=(Curl&) = delete;

		//! Deleted move constructor.
		Curl(Curl&&) = delete;

		//! Deleted move assignment operator.
		Curl& operator=(Curl&&) = delete;

		///@}

	protected:
		///@name Helper
		///@{

		[[nodiscard]] static std::string curlStringToString(char * curlString);

		///@}
		///@name Header Handling
		///@{

		static int header(char * data, std::size_t size, std::size_t nitems, void * thisPtr);
		int headerInClass(char * data, std::size_t size);

		///@}
		///@name Writers
		///@{

		static int writer(char * data, std::size_t size, std::size_t nmemb, void * thisPtr);
		int writerInClass(char * data, std::size_t size);

		///@}

	private:
		const std::string_view cookieDir;
		CURLcode curlCode{CURLE_OK};
		std::string content;
		std::string contentType;
		std::uint32_t responseCode{};
		std::uint32_t xTsHeaderValue{};
		bool limitedSettings{false};
		bool post{false};
		std::string tmpCookies;
		std::string oldCookies;
		const NetworkSettings networkSettings;
		int features{};
		unsigned int version{};

		// const pointer to network configuration
		const Network::Config * config{nullptr};

		// libcurl object
		Wrapper::Curl curl;

		// libcurl lists
		CurlList dnsResolves;
		CurlList headers;
		CurlList tmpHeaders;
		CurlList http200Aliases;
		CurlList proxyHeaders;

		// internal helper functions
		template<typename T> std::enable_if_t<std::is_integral_v<T>>
		setOption(CURLoption option, T numericValue) {
			//NOLINTNEXTLINE(google-runtime-int)
			this->setOption(option, static_cast<long>(numericValue));
		}

		void setOption(CURLoption option, const std::string& stringValue);
		//NOLINTNEXTLINE(google-runtime-int)
		void setOption(CURLoption option, long longValue);
		void setOption(CURLoption option, CurlList& list);
		void setOption(CURLoption option, void * pointer);

		[[nodiscard]] bool hasFeature(int feature) const noexcept;

		void checkCode();

		void clearContent();
		[[nodiscard]] std::string preparePost(std::string_view url, std::string& postFieldsTo);
		[[nodiscard]] std::string prepareGet(std::string_view url);
		void checkResult(const std::array<char, CURL_ERROR_SIZE>& errorBuffer);
		void checkResponseCode(const std::vector<std::uint32_t>& errors);
		void retrieveContentType();
		void processContentType();
		void checkCompression();
		void repairEncoding();
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * CONSTRUCTION AND DESTRUCTION
	 */

	//! Constructor setting the cookie directory and the network options.
	/*!
	 * Initializes @c libcurl and sets some basic global default options like the write function,
	 *  which is used to handle incoming network traffic (and is provided by the class).
	 *
	 * \param cookieDirectory The path to the directory where cookies will be saved in.
	 * \param setNetworkSettings The network options for the connection represented by this instance.
	 *
	 * \throws Curl::Exception if the API could not be initalized,
	 *   the used @c libcurl library does not support SSL,
	 *   or the initial options not be set.
	 *
	 * \sa writer, writerInClass, NetworkSettings
	 */
	inline Curl::Curl(
			std::string_view cookieDirectory,
			const NetworkSettings& setNetworkSettings
	)		:	cookieDir(cookieDirectory),
				networkSettings(setNetworkSettings) {
		// check pointer to libcurl instance
		if(!(this->curl.valid())) {
			throw Curl::Exception("Could not initialize the libcurl library");
		}

		// get libcurl version information
		const auto * versionInfo = curl_version_info(CURLVERSION_NOW);

		this->features = versionInfo->features;
		this->version = versionInfo->version_num;

		// check for SSL support
		if(!(this->hasFeature(CURL_VERSION_SSL))) { //NOLINT(hicpp-signed-bitwise)
			throw Curl::Exception("The libcurl library does not support SSL");
		}

		// configure libcurl
		this->setOption(CURLOPT_NOSIGNAL, 1L);

		// set header function
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_HEADERFUNCTION,
				Curl::header
		);

		this->checkCode();

		// set write function
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_WRITEFUNCTION,
				Curl::writer
		);

		this->checkCode();

		// set pointers to instance
		this->setOption(CURLOPT_WRITEDATA, this);
		this->setOption(CURLOPT_HEADERDATA, this);
	}

	/*
	 * SETTERS
	 */

	//! Sets the network options for the connection according to the given configuration.
	/*!
	 * Warnings might include options set, but not
	 *  supported by the available version of the
	 *  @c libcurl library.
	 *
	 * \note If limited is **true**, cookie settings,
	 *   custom headers, HTTP version and error responses
	 *   of the network configuration will be ignored.
	 *
	 * \note Use this for secondary connections not related
	 *   to the original website, e.g. to web archives.
	 *
	 * \param globalConfig %a Network configuration.
	 * \param limited Indicates whether the settings
	 *   will have only limited effect (see below).
	 * \param warningsTo Reference to a queue of strings
	 *   that will be filled with warnings if they occur.
	 *
	 * \throws Curl::Exception if any of the options could not be set.
	 *
	 * \sa setConfigCurrent, Config
	 */
	inline void Curl::setConfigGlobal(
			const Config& globalConfig,
			bool limited,
			std::queue<std::string>& warningsTo
	) {
		// check libcurl handle
		if(!(this->curl.valid())) {
			throw Curl::Exception("libcurl has not been initialized");
		}

		// set libcurl options
		this->setOption(CURLOPT_MAXCONNECTS, globalConfig.networkConfig.connectionsMax);
		this->setOption(
				CURLOPT_IGNORE_CONTENT_LENGTH,
				globalConfig.networkConfig.contentLengthIgnore
		);

		if(globalConfig.networkConfig.cookies && !limited) {
			// add cookie directory to cookie files
			std::string loadCookiesFrom;
			std::string saveCookiesTo;

			if(!globalConfig.networkConfig.cookiesLoad.empty()) {
				loadCookiesFrom.reserve(
						this->cookieDir.length()
						+ globalConfig.networkConfig.cookiesLoad.length()
						+ 1 // path separator
				);

				loadCookiesFrom = this->cookieDir;

				loadCookiesFrom += Helper::FileSystem::getPathSeparator();
				loadCookiesFrom += globalConfig.networkConfig.cookiesLoad;

				// check whether cookie file really is located in cookie directory
				if(!Helper::FileSystem::contains(this->cookieDir, loadCookiesFrom)) {
					std::string exceptionString{"Cookie file '"};

					exceptionString += loadCookiesFrom;
					exceptionString += "is not in directory '";
					exceptionString += this->cookieDir;
					exceptionString += "'";

					throw Curl::Exception(exceptionString);
				}
			}

			if(!globalConfig.networkConfig.cookiesSave.empty()) {
				saveCookiesTo.reserve(
						this->cookieDir.length()
						+ globalConfig.networkConfig.cookiesSave.length()
						+ 1 // path separator
				);

				saveCookiesTo = this->cookieDir;

				saveCookiesTo += Helper::FileSystem::getPathSeparator();
				saveCookiesTo += globalConfig.networkConfig.cookiesSave;

				// check whether cookie file really is located in cookie directory
				if(!Helper::FileSystem::contains(this->cookieDir, saveCookiesTo)) {
					std::string exceptionString{"Cookie file '"};

					exceptionString += saveCookiesTo;
					exceptionString += "is not in directory '";
					exceptionString += this->cookieDir;
					exceptionString += "'";

					throw Curl::Exception(exceptionString);
				}
			}

			this->setOption(CURLOPT_COOKIEFILE, loadCookiesFrom);

			if(!saveCookiesTo.empty()) {
				this->setOption(CURLOPT_COOKIEJAR, saveCookiesTo);
			}
		}

		if(!globalConfig.networkConfig.cookiesSession && !limited) {
			this->setOption(CURLOPT_COOKIESESSION, true);
		}

		if(!globalConfig.networkConfig.cookiesSet.empty() && !limited) {
			this->setCookies(globalConfig.networkConfig.cookiesSet);
		}

		this->setOption(CURLOPT_DNS_CACHE_TIMEOUT, globalConfig.networkConfig.dnsCacheTimeOut);

		if(!globalConfig.networkConfig.dnsDoH.empty()) {
			if(this->version >= versionDoH) {
				this->setOption(CURLOPT_DOH_URL, globalConfig.networkConfig.dnsDoH);
			}
			else {
				warningsTo.emplace(
						"DNS-over-HTTPS currently not supported,"
						" 'network.dns.doh' ignored."
				);
			}
		}

		if(!globalConfig.networkConfig.dnsInterface.empty()) {
			this->setOption(CURLOPT_DNS_INTERFACE, globalConfig.networkConfig.dnsInterface);
		}

		if(!globalConfig.networkConfig.dnsResolves.empty()) {
			this->dnsResolves.append(globalConfig.networkConfig.dnsResolves);

			this->setOption(CURLOPT_RESOLVE, this->dnsResolves);
		}

		if(!globalConfig.networkConfig.dnsServers.empty()) {
			std::string serverList;

			for(const auto& dnsServer : globalConfig.networkConfig.dnsServers) {
				serverList += dnsServer + ",";
			}

			serverList.pop_back();

			this->setOption(CURLOPT_DNS_SERVERS, serverList);
		}

		if(this->version >= versionDnsShuffle) {
			this->setOption(CURLOPT_DNS_SHUFFLE_ADDRESSES, globalConfig.networkConfig.dnsShuffle);
		}
		else {
			if(globalConfig.networkConfig.dnsShuffle) {
				warningsTo.emplace(
						"DNS shuffling currently not supported,"
						" 'network.dns.shuffle' ignored."
				);
			}
		}

		if(
				globalConfig.networkConfig.encodingBr
				|| globalConfig.networkConfig.encodingDeflate
				|| globalConfig.networkConfig.encodingGZip
				|| globalConfig.networkConfig.encodingIdentity
				|| globalConfig.networkConfig.encodingZstd
		) {
			std::string encodingList;

			if(globalConfig.networkConfig.encodingBr) {
				if(
						this->version >= versionBrotli
						&& this->hasFeature(CURL_VERSION_BROTLI) //NOLINT(hicpp-signed-bitwise)
				) {
					encodingList += "br,";
				}
				else {
					warningsTo.emplace(
							"brotli encoding currently not supported,"
							" 'network.encoding.br' ignored."
					);
				}
			}

			if(globalConfig.networkConfig.encodingDeflate) {
				if(this->hasFeature(CURL_VERSION_LIBZ)) { //NOLINT(hicpp-signed-bitwise)
					encodingList += "deflate,";
				}
				else {
					warningsTo.emplace(
							"deflate encoding currently not supported"
							" (the libcurl library misses libz support),"
							" 'network.encoding.deflate' ignored."
					);
				}
			}

			if(globalConfig.networkConfig.encodingGZip) {
				if(this->hasFeature(CURL_VERSION_LIBZ)) { //NOLINT(hicpp-signed-bitwise)
					encodingList += "gzip,";
				}
				else {
					warningsTo.emplace(
							"deflate encoding currently not supported"
							" (the libcurl library misses libz support),"
							" 'network.encoding.gzip' ignored."
					);
				}
			}

			if(globalConfig.networkConfig.encodingIdentity) {
				encodingList += "identity,";
			}

			if(globalConfig.networkConfig.encodingZstd) {
				if(
						this->version >= versionZstd
						&& this->hasFeature(CURL_VERSION_ZSTD) //NOLINT(hicpp-signed-bitwise)
				) {
					encodingList += "zstd,";
				}
				else {
					warningsTo.emplace(
							"Zstandard encoding currently not supported,"
							" 'network.encoding.zstd' ignored."
					);
				}
			}

			encodingList.pop_back();

			this->setOption(CURLOPT_ACCEPT_ENCODING, encodingList);
		}
		else {
			this->setOption(CURLOPT_ACCEPT_ENCODING, nullptr);
		}

		if(globalConfig.networkConfig.encodingTransfer) {
			this->setOption(CURLOPT_TRANSFER_ENCODING, globalConfig.networkConfig.encodingTransfer);
		}

		if(!globalConfig.networkConfig.headers.empty() && !limited) {
			this->headers.append(globalConfig.networkConfig.headers);

			this->setOption(CURLOPT_HTTPHEADER, this->headers);
		}

		if(!globalConfig.networkConfig.http200Aliases.empty() && !limited) {
			this->http200Aliases.append(globalConfig.networkConfig.http200Aliases);

			this->setOption(CURLOPT_HTTP200ALIASES, this->http200Aliases);
		}

		if(!limited) {
			switch(globalConfig.networkConfig.httpVersion) {
			case httpVersionAny:
				this->setOption(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_NONE);

				break;

			case httpVersion1:
				this->setOption(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);

				break;

			case httpVersion11:
				this->setOption(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

				break;

			case httpVersion2:
				if(
						this->version >= versionHttp2
						&& this->hasFeature(CURL_VERSION_HTTP2) //NOLINT(hicpp-signed-bitwise)
				) {
					this->setOption(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
				}
				else {
					warningsTo.emplace(
							"HTTP/2 currently not supported,"
							" 'network.http.version' ignored."
					);
				}

				break;

			case httpVersion2Only:
				if(
						this->version >= versionHttp2Only
						&& this->hasFeature(CURL_VERSION_HTTP2) //NOLINT(hicpp-signed-bitwise)
				) {
					this->setOption(
							CURLOPT_HTTP_VERSION,
							CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE
					);
				}
				else {
					warningsTo.emplace(
							"HTTP/2 ONLY currently not supported,"
							" 'network.http.version' ignored."
					);
				}

				break;

			case httpVersion2Tls:
				if(
						this->version >= versionHttp2Tls
						&& this->hasFeature(CURL_VERSION_HTTP2) //NOLINT(hicpp-signed-bitwise)
				) {
					this->setOption(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
				}
				else {
					warningsTo.emplace(
							"HTTP/2 OVER TLS ONLY currently not supported,"
							" 'network.http.version' ignored."
					);
				}

				break;

			case httpVersion3Only:
				if(
						this->version >= versionHttp3Only
						&& this->hasFeature(CURL_VERSION_HTTP3) //NOLINT(hicpp-signed-bitwise)
				) {
					this->setOption(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_3);
				}
				else {
					warningsTo.emplace(
							"HTTP/3 ONLY currently not supported,"
							" 'network.http.version' ignored."
					);
				}

				break;

			default:
				warningsTo.emplace(
						"Enum value for HTTP version not recognized,"
						" 'network.http.version' ignored."
				);
			}
		}

		if(!globalConfig.networkConfig.localInterface.empty()) {
			this->setOption(CURLOPT_INTERFACE, globalConfig.networkConfig.localInterface);
		}

		this->setOption(CURLOPT_LOCALPORT, globalConfig.networkConfig.localPort);
		this->setOption(CURLOPT_LOCALPORTRANGE, globalConfig.networkConfig.localPortRange);
		this->setOption(CURLOPT_FORBID_REUSE, globalConfig.networkConfig.noReUse);

		if(globalConfig.networkConfig.proxy.empty()) {
			if(!(this->networkSettings.defaultProxy.empty())) {
				// no proxy is given, but default proxy is set: use default proxy
				this->setOption(CURLOPT_PROXY, this->networkSettings.defaultProxy);
			}
		}
		else {
			this->setOption(CURLOPT_PROXY, globalConfig.networkConfig.proxy);
		}

		if(!globalConfig.networkConfig.proxyAuth.empty()) {
			this->setOption(CURLOPT_PROXYUSERPWD, globalConfig.networkConfig.proxyAuth);
		}

		if(!globalConfig.networkConfig.proxyHeaders.empty()) {
			this->proxyHeaders.append(globalConfig.networkConfig.proxyHeaders);

			this->setOption(CURLOPT_PROXYHEADER, this->proxyHeaders);
		}

		if(!globalConfig.networkConfig.proxyPre.empty()) {
			if(this->version >= versionPreProxy) {
				this->setOption(CURLOPT_PRE_PROXY, globalConfig.networkConfig.proxyPre);
			}
			else {
				warningsTo.emplace(
						"Pre-Proxy currently not supported,"
						" ' proxy.pre' ignored."
				);
			}
		}

		if(
				!globalConfig.networkConfig.proxyTlsSrpPassword.empty()
				|| !globalConfig.networkConfig.proxyTlsSrpUser.empty()
		) {
			if(
					this->version >= versionProxyTlsAuth
					&& this->hasFeature(CURL_VERSION_TLSAUTH_SRP) //NOLINT(hicpp-signed-bitwise)
			) {
				this->setOption(CURLOPT_PROXY_TLSAUTH_TYPE, authTypeTlsSrp);
				this->setOption(
						CURLOPT_PROXY_TLSAUTH_USERNAME,
						globalConfig.networkConfig.proxyTlsSrpUser
				);
				this->setOption(
						CURLOPT_PROXY_TLSAUTH_PASSWORD,
						globalConfig.networkConfig.proxyTlsSrpPassword
				);
			}
			else {
				warningsTo.emplace(
						"Proxy TLS authentication currently not supported,"
						" 'proxy.tls.srp.user' and 'proxy.tls.srp.password' ignored."
				);
			}
		}

		this->setOption(CURLOPT_HTTPPROXYTUNNEL, globalConfig.networkConfig.proxyTunnelling);
		this->setOption(CURLOPT_FOLLOWLOCATION, globalConfig.networkConfig.redirect);
		this->setOption(CURLOPT_MAXREDIRS, globalConfig.networkConfig.redirectMax);

		if(
				globalConfig.networkConfig.redirectPost301
				&& globalConfig.networkConfig.redirectPost302
				&& globalConfig.networkConfig.redirectPost303
		) {
			//NOLINTNEXTLINE(hicpp-signed-bitwise)
			this->setOption(CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
		}
		else {
			//NOLINTNEXTLINE(google-runtime-int)
			long redirectPost{};

			if(globalConfig.networkConfig.redirectPost301) {
				//NOLINTNEXTLINE(hicpp-signed-bitwise)
				redirectPost |= CURL_REDIR_POST_301;
			}

			if(globalConfig.networkConfig.redirectPost302) {
				//NOLINTNEXTLINE(hicpp-signed-bitwise)
				redirectPost |= CURL_REDIR_POST_302;
			}

			if(globalConfig.networkConfig.redirectPost303) {
				//NOLINTNEXTLINE(hicpp-signed-bitwise)
				redirectPost |= CURL_REDIR_POST_303;
			}

			this->setOption(CURLOPT_POSTREDIR, redirectPost);
		}

		if(!globalConfig.networkConfig.referer.empty() && !limited) {
			this->setOption(CURLOPT_REFERER, globalConfig.networkConfig.referer);
		}

		this->setOption(CURLOPT_AUTOREFERER, globalConfig.networkConfig.refererAutomatic);
		this->setOption(CURLOPT_MAX_RECV_SPEED_LARGE, globalConfig.networkConfig.speedDownLimit);
		this->setOption(CURLOPT_LOW_SPEED_LIMIT, globalConfig.networkConfig.speedLowLimit);
		this->setOption(CURLOPT_LOW_SPEED_TIME, globalConfig.networkConfig.speedLowTime);
		this->setOption(CURLOPT_MAX_SEND_SPEED_LARGE, globalConfig.networkConfig.speedUpLimit);
		this->setOption(CURLOPT_SSL_VERIFYHOST, globalConfig.networkConfig.sslVerifyHost);
		this->setOption(CURLOPT_SSL_VERIFYPEER, globalConfig.networkConfig.sslVerifyPeer);

		if(this->version >= versionProxySslVerify) {
			this->setOption(CURLOPT_PROXY_SSL_VERIFYHOST, globalConfig.networkConfig.sslVerifyProxyHost);
			this->setOption(CURLOPT_PROXY_SSL_VERIFYPEER, globalConfig.networkConfig.sslVerifyProxyPeer);
		}
		else {
			if(
					globalConfig.networkConfig.sslVerifyProxyHost
					|| globalConfig.networkConfig.sslVerifyProxyPeer
			) {
				warningsTo.emplace(
						"SSL verification of proxy host and peer currently not supported,"
						" 'ssl.verify.proxy.host' and  'ssl.verify.proxy.peer' ignored."
				);
			}
		}

		this->setOption(CURLOPT_SSL_VERIFYSTATUS, globalConfig.networkConfig.sslVerifyStatus);

		if(this->version >= versionTcpFastOpen) {
			this->setOption(CURLOPT_TCP_FASTOPEN, globalConfig.networkConfig.tcpFastOpen);
		}
		else {
			if(globalConfig.networkConfig.tcpFastOpen) {
				warningsTo.emplace(
						"TCP Fast Open currently not supported,"
						" 'tcp.fast.open' ignored."
				);
			}
		}

		this->setOption(CURLOPT_TCP_KEEPALIVE, globalConfig.networkConfig.tcpKeepAlive);
		this->setOption(CURLOPT_TCP_KEEPIDLE, globalConfig.networkConfig.tcpKeepAliveIdle);
		this->setOption(CURLOPT_TCP_KEEPINTVL, globalConfig.networkConfig.tcpKeepAliveInterval);
		this->setOption(CURLOPT_TCP_NODELAY, globalConfig.networkConfig.tcpNagle);
		this->setOption(CURLOPT_CONNECTTIMEOUT, globalConfig.networkConfig.timeOut);

		if(this->version >= versionHappyEyeballs) {
			if(globalConfig.networkConfig.timeOutHappyEyeballs > 0) {
				this->setOption(
						CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS,
						globalConfig.networkConfig.timeOutHappyEyeballs
				);
			}
			else {
				this->setOption(CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS, CURL_HET_DEFAULT);
			}
		}
		else if(globalConfig.networkConfig.timeOutHappyEyeballs > 0) {
			warningsTo.emplace(
					"Happy Eyeballs Configuration currently not supported,"
					" 'network.timeout.happyeyeballs' ignored."
			);
		}

		this->setOption(CURLOPT_TIMEOUT, globalConfig.networkConfig.timeOutRequest);

		if(
				!globalConfig.networkConfig.tlsSrpPassword.empty()
				|| !globalConfig.networkConfig.tlsSrpUser.empty()
		) {
			this->setOption(CURLOPT_TLSAUTH_TYPE, "SRP");
			this->setOption(CURLOPT_TLSAUTH_USERNAME, globalConfig.networkConfig.tlsSrpUser);
			this->setOption(CURLOPT_TLSAUTH_PASSWORD, globalConfig.networkConfig.tlsSrpPassword);
		}

		if(!globalConfig.networkConfig.userAgent.empty()) {
			this->setOption(CURLOPT_USERAGENT, globalConfig.networkConfig.userAgent);
		}

		this->setOption(CURLOPT_VERBOSE, globalConfig.networkConfig.verbose);

		// save configuration
		this->config = &globalConfig;
		this->limitedSettings = limited;
	}

	//! Sets temporary network options for the connection according to the given configuration.
	/*!
	 * Only uses Config::cookiesOverwrite from the given configuration
	 *  to add or manipulate cookies already set.
	 *
	 * \param currentConfig The network configuration to be used.
	 *
	 * \throws Curl::Exception
	 *
	 * \sa <a href="https://curl.haxx.se/libcurl/c/CURLOPT_COOKIELIST.html">CURLOPT_COOKIELIST</a>
	 */
	inline void Curl::setConfigCurrent(const Config& currentConfig) {
		// overwrite cookies
		for(const auto& cookie : currentConfig.networkConfig.cookiesOverwrite) {
			this->setOption(CURLOPT_COOKIELIST, "Set-Cookie:" + cookie);
		}
	}

	//! Sets custom cookies.
	/*!
	 * These cookies will be sent along with all subsequent
	 *  HTTP requests as long as the connection is not reset.
	 *
	 * If a reference to an empty string is given,
	 *  the function will unset cookies previously
	 *  set through this function.
	 *
	 * This function works independently
	 *  from the internal @c libcurl cookie engine.
	 *
	 * \warning Custom cookies set by this function will
	 *   be lost as soon as the connection is reset.
	 *
	 * \note A string view cannot be used, because the
	 *   underlying API requires a null-terminated string.
	 *
	 * \param cookies Const reference to a string containing
	 *   the cookies to send in the same format as in the
	 *   corresponding HTTP header, i.e.
	 *   "name1=content1; name2=content2;" etc.
	 *
	 * \throws Curl::Exception if the cookies could not be set.
	 *
	 * \sa unsetCookies, resetConnection,
	 *   <a href="https://curl.haxx.se/libcurl/c/CURLOPT_COOKIE.html">CURLOPT_COOKIE</a>
	 */
	inline void Curl::setCookies(const std::string& cookies) {
		if(cookies.empty()) {
			// reset cookies if string is empty
			this->setOption(CURLOPT_COOKIE, nullptr);
		}
		else {
			this->setOption(CURLOPT_COOKIE, cookies);
		}

		this->oldCookies = this->tmpCookies;
		this->tmpCookies = cookies;
	}

	//! Sets custom HTTP headers.
	/*!
	 * These headers will be sent along with all subsequent
	 *  HTTP requests as long as the connection is not reset.
	 *
	 * \warning Custom headers set by this function will
	 *   be lost as soon as the connection is reset.
	 *
	 * \param customHeaders A vector of strings providing
	 *   the custom HTTP headers to be set.
	 *
	 * \throws Curl::Exception if the headers could not be set.
	 *
	 * \sa unsetHeaders, resetConnection,
	 *   <a href="https://curl.haxx.se/libcurl/c/CURLOPT_HTTPHEADER.html">CURLOPT_HTTPHEADER</a>
	 */
	inline void Curl::setHeaders(const std::vector<std::string>& customHeaders) {
		// clear old temporary headers if necessary
		this->tmpHeaders.clear();

		if(customHeaders.empty()) {
			// reset headers if vector is empty
			this->setOption(CURLOPT_HTTPHEADER, this->headers);
		}
		else {
			// temporarily combine global and current headers
			this->tmpHeaders.append(this->headers);
			this->tmpHeaders.append(customHeaders);

			this->setOption(CURLOPT_HTTPHEADER, this->tmpHeaders);
		}
	}

	//! Forces @c libcurl into or out of verbose mode.
	/*!
	 * In verbose mode, extensive connection information will be written
	 *   to stdout.
	 *
	 * \warning This function will override any configuration.
	 *   It should be used for debugging purposes only.
	 *
	 * \param isVerbose If true, @c libcurl will be forced into verbose mode.
	 *   If false, @c libcurl will be forced out of verbose mode.
	 *
	 * \throws Curl::Exception if the verbose mode could not be set.
	 *
	 * \sa <a href="https://curl.haxx.se/libcurl/c/CURLOPT_VERBOSE.html">CURLOPT_VERBOSE</a>
	 */
	inline void Curl::setVerbose(bool isVerbose) {
		this->setOption(CURLOPT_VERBOSE, isVerbose);
	}

	//! Unsets custom cookies previously set.
	/*!
	 * All cookies set by setCookies() will be discarded.
	 *
	 * This function works independently
	 *  from the internal @c libcurl cookie engine.
	 *
	 * \throws Curl::Exception if the cookies could not be unset.
	 *
	 * \sa <a href="https://curl.haxx.se/libcurl/c/CURLOPT_COOKIE.html">CURLOPT_COOKIE</a>
	 */
	inline void Curl::unsetCookies() {
		if(this->oldCookies.empty()) {
			this->setOption(CURLOPT_COOKIE, nullptr);

			this->tmpCookies.clear();
		}
		else {
			this->setOption(CURLOPT_COOKIE, this->oldCookies);

			this->tmpCookies = this->oldCookies;

			this->oldCookies.clear();
		}
	}

	//! Unsets custom HTTP headers previously set.
	/*!
	 * All HTTP headers set by setHeaders() will be discarded.
	 *
	 * \throws Curl::Exception if the headers could not be unset.
	 *
	 * \sa <a href="https://curl.haxx.se/libcurl/c/CURLOPT_HTTPHEADER.html">CURLOPT_HTTPHEADER</a>
	 */
	inline void Curl::unsetHeaders() {
		// clear temporary headers if necessary
		this->tmpHeaders.clear();

		// reset headers
		this->setOption(CURLOPT_HTTPHEADER, this->headers);
	}

	/*
	 * GETTERS
	 */

	//! Uses the connection to get content by sending a HTTP request to the specified URL.
	/*!
	 * When using HTTP POST, the data to be sent
	 *  will be determined the same way as for a HTTP GET
	 *  request – from behind the first question mark (?)
	 *  in the given URL.
	 *
	 * If no question mark is present, no additional
	 *  data will be sent along the HTTP POST request.
	 *
	 * Before sending the request, the given URL will be encoded
	 *  while keeping possible reserved characters intact.
	 *
	 * Response code and content type of the reply will be saved
	 *  to be requested by getResponseCode() and getContentType().
	 *
	 * After a successful request, replies encoded in ISO-8859-1
	 *  will be converted to UTF-8 and invalid UTF-8 sequences
	 *  will be removed.
	 *
	 * \param url Const reference to the string
	 *   containing the URL to request.
	 *
	 * \param usePost States whether to use HTTP POST
	 *   instead of HTTP GET on this request.
	 *
	 * \param contentTo Reference to a string in which
	 *   the received content will be stored.
	 *
	 * \param errors Vector of HTTP error codes which
	 *   will be handled by throwing an exception,
	 *   except if the error code is also present in
	 *   the @c X-ts header returned by the host.
	 *
	 * \throws Curl::Exception if setting the necessary options failed,
	 *   the HTTP request could not be sent, information about the reply
	 *   could not be retrieved or any of the specified HTTP error codes
	 *   has been received.
	 */
	inline void Curl::getContent(
			std::string_view url,
			bool usePost,
			std::string& contentTo,
			const std::vector<std::uint32_t>& errors
	) {
		this->clearContent();

		/*
		 * NOTE:	The following string needs to be available until after the request,
		 * 			because libcurl does not create its own copy of it!
		 */
		std::string postFields;

		const auto escapedUrl{
			usePost ? this->preparePost(url, postFields) : this->prepareGet(url)
		};

		// set URL
		this->setOption(CURLOPT_URL, escapedUrl);

		// set error buffer
		std::array<char, CURL_ERROR_SIZE> errorBuffer{};

		errorBuffer.at(0) = 0;

		this->setOption(CURLOPT_ERRORBUFFER, errorBuffer.data());

		// perform request
		try {
			this->curlCode = curl_easy_perform(this->curl.get());
		}
		catch(const std::exception& e) { /* handle exception */
			// reset error buffer
			this->setOption(CURLOPT_ERRORBUFFER, nullptr);

			throw Curl::Exception(e.what());
		}

		// process reply
		this->checkResult(errorBuffer);
		this->checkResponseCode(errors);
		this->retrieveContentType();
		this->processContentType();
		this->checkCompression();
		this->repairEncoding();

		contentTo.swap(this->content);
	}

	//! Gets the response code of the HTTP reply received last.
	/*!
	 * \returns The HTTP code received during the last call to getContent.
	 */
	inline std::uint32_t Curl::getResponseCode() const noexcept {
		return this->responseCode;
	}

	//! Gets the content type of the HTTP reply received last.
	/*!
	 * \returns A copy to the string containing the content type received
	 *  during the last call to getContent.
	 */
	inline std::string Curl::getContentType() const noexcept {
		return this->contentType;
	}

	//! Gets the @c libcurl return code received from the last API call.
	/*!
	 * Use this function to determine which error occured
	 *  after another call to this class failed.
	 *
	 * \returns The received @c libcurl return code.
	 *
	 * \sa <a href="https://curl.haxx.se/libcurl/c/libcurl-errors.html">@c libcurl error codes</a>
	 */
	inline CURLcode Curl::getCurlCode() const noexcept {
		return this->curlCode;
	}

	//! Uses the connection to determine its public IP address.
	/*!
	 * Requests the public IP address of the connection
	 *  from an external URL defined inside this function.
	 *
	 *  \returns The public IP address of the connection as string
	 *   – or a string beginning with "N/A" if the IP address could
	 *    not be determined. An error description might follow.
	 */
	inline std::string Curl::getPublicIp() {
		std::string ip;

		try {
			this->getContent(
					getPublicIpFrom,
					false,
					ip,
					std::vector<std::uint32_t>(
							getPublicIpErrors.cbegin(),
							getPublicIpErrors.cend()
					)
			);
		}
		catch(const Curl::Exception& e) {
			return "N/A (" + std::string(e.view()) + ")";
		}

		if(ip.empty()) {
			return "N/A";
		}

		return ip;
	}

	/*
	 * RESET
	 */

	//! Resets the connection.
	/*!
	 * After cleaning up the connection, the function will wait for
	 *  the specified sleep time, but regularly check the status of
	 *  the application to not considerably delay its shutdown.
	 *
	 * It then resets the configuration passed to setConfigGlobal().
	 *
	 * \warning The configuration passed to setConfigCurrent()
	 *   will be discarded.
	 *
	 * \note Warnings when re-setting the configuration will be discarded,
	 *   because they will have already been reported when the configuration
	 *   was originally set.
	 *
	 * \param sleepForMilliseconds Time to wait in milliseconds
	 *   before re-establishing the connection.
	 * \param isRunningCallback Constant reference to a callback function
	 *   (or lambda)
	 *   which returns whether the application is still running.
	 *
	 * \throws Curl::Exception if any connection option could not be (re-)set.
	 */
	inline void Curl::resetConnection(
			std::uint64_t sleepForMilliseconds,
			const IsRunningCallback& isRunningCallback
	) {
		// cleanup lists
		this->dnsResolves.clear();
		this->headers.clear();
		this->tmpHeaders.clear();
		this->http200Aliases.clear();
		this->proxyHeaders.clear();

		// cleanup libcurl
		this->curl.clear();

		// sleep
		const auto sleepTill{
			std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(sleepForMilliseconds)
		};

		while(
				isRunningCallback()
				&& std::chrono::steady_clock::now() < sleepTill
		) {
			std::this_thread::sleep_for(std::chrono::milliseconds(checkEveryMilliseconds));
		}

		// re-initialize libcurl
		this->curl.init();

		// configure libcurl (global defaults)
		this->setOption(CURLOPT_NOSIGNAL, true);

		// set header function
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_HEADERFUNCTION,
				Curl::header
		);

		this->checkCode();

		// set write function
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				CURLOPT_WRITEFUNCTION,
				Curl::writer
		);

		this->checkCode();

		// set pointers to instance
		this->setOption(CURLOPT_WRITEDATA, this);
		this->setOption(CURLOPT_HEADERDATA, this);

		// set configuration
		if(this->config != nullptr) {
			std::queue<std::string> logDump; // do not log warnings again

			this->setConfigGlobal(
					*(this->config),
					this->limitedSettings,
					logDump
			);
		}
	}

	/*
	 * URL ENCODING
	 */

	//! URL encodes the given string.
	/*!
	 *
	 * \note A string view cannot be used, because the
	 *   underlying API requires a null-terminated string.
	 *
	 * \warning The @c libcurl library needs to be
	 *   successfully initialized for URL encoding,
	 *   except for an empty string.
	 *
	 * \param stringToEscape Const reference to the string to be encoded.
	 * \param usePlusForSpace States whether to convert
	 *   spaces to + instead of %20.
	 *
	 * \returns A copy of the encoded string.
	 *
	 * \throws Curl::Exception if the @c libcurl library has
	 *   not been initialized.
	 *
	 * \sa <a href="https://curl.haxx.se/libcurl/c/curl_escape.html">curl_escape</a>
	 */
	inline std::string Curl::escape(const std::string& stringToEscape, bool usePlusForSpace) {
		if(stringToEscape.empty()) {
			return "";
		}

		if(!(this->curl.valid())) {
			throw Curl::Exception(
					"Curl::escape():"
					" libcurl library has not been initialized"
			);
		}

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
			std::size_t pos{};

			while(true) {
				pos = result.find(encodedSpace, pos);

				if(pos < result.length()) {
					result = result.substr(0, pos)
							+ '+' + result.substr(pos + encodedSpaceLength);

					++pos;
				}
				else {
					break;
				}
			}
		}
		return result;
	}

	//! URL decodes the given string.
	/*!
	 * \note A string view cannot be used, because the
	 *   underlying API requires a null-terminated string.
	 *
	 * \warning The @c libcurl library needs to be
	 *  successfully initialized for URL encoding,
	 *  except for an empty string.
	 *
	 * \param escapedString Const reference to the string to be decoded.
	 * \param usePlusForSpace States whether plusses should be decoded to spaces.
	 *
	 * \returns A copy of the decoded string.
	 *
	 * \throws Curl::Exception if the @c libcurl library
	 *   has not been initialized.
	 *
	 * \sa <a href="https://curl.haxx.se/libcurl/c/curl_unescape.html">curl_unescape</a>
	 */
	inline std::string Curl::unescape(const std::string& escapedString, bool usePlusForSpace) {
		if(escapedString.empty()) {
			return "";
		}

		if(!(this->curl.valid())) {
			throw Curl::Exception(
					"Curl::unescape():"
					" libcurl library had not been initialized"
			);
		}

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
			std::size_t pos{};

			while(true) {
				pos = result.find('+', pos);

				if(pos < result.length()) {
					result.replace(pos, 1, " ");

					++pos;
				}
				else {
					break;
				}
			}
		}

		return result;
	}

	//! URL encodes the given string while leaving reserved characters (; / ? : @ = & #) intact.
	/*!
	 * The function will copy those parts of the string that
	 *  need to be escaped and use the @c libcurl library to
	 *  escape them.
	 *
	 * Leaves the characters ; / ? : @ = & # unchanged
	 *  in the resulting string.
	 *
	 * \warning The @c libcurl library needs to be
	 *   successfully initialized for URL encoding,
	 *   except for an empty string (or a string containing
	 *   only reserved characters).
	 *
	 * \param urlToEscape A view to the string containing the URL to be encoded.
	 *
	 * \returns A copy of the encoded string.
	 *
	 * \throws Curl::Exception if the @c libcurl library
	 *   has not been initialized.
	 *
	 * \sa <a href="https://curl.haxx.se/libcurl/c/curl_escape.html">curl_escape</a>
	 */
	inline std::string Curl::escapeUrl(std::string_view urlToEscape) {
		if(
				urlToEscape.find_first_not_of(
						reservedCharacters
				) == std::string::npos
		) {
			return "";
		}

		if(!(this->curl.valid())) {
			throw Curl::Exception(
					"Curl::unescape():"
					" libcurl library has not been initialized"
			);
		}

		std::size_t pos{};
		std::string result;

		while(pos < urlToEscape.length()) {
			auto end{urlToEscape.find_first_of(reservedCharacters, pos)};

			if(end == std::string::npos) {
				end = urlToEscape.length();
			}
			if(end - pos > 0) {
				const std::string part(urlToEscape, pos, end - pos);

				result += Curl::curlStringToString(
						curl_easy_escape(
								this->curl.get(),
								part.c_str(),
								part.length()
						)
				);
			}

			if(end < urlToEscape.length()) {
				result += urlToEscape.at(end);
			}

			pos = end + 1;
		}

		return result;
	}

	/*
	 * HELPER (protected)
	 */

	//! Copies the given @c libcurl string into a std::string and releases its memory.
	/*!
	 * Afterwards curlString will be invalid and its memory freed.
	 *
	 * If @c curlString is a @c nullptr it will be ignored.
	 *
	 * \warning It is imperative that curlString is either a @c nullptr
	 *  or a valid @c libcurl string. Otherwise the program may crash and
	 *  the memory be corrupted.
	 *
	 * \warning In this case, there is no exception handling
	 *  preventing crashes or memory leaks provided by the API.
	 *
	 * \param curlString A pointer to a valid curlString
	 *  or @c nullptr.
	 *
	 * \returns A copy of the string originally saved in curlString
	 *  or an empty string if curlString is a @c nullptr.
	 */
	inline std::string Curl::curlStringToString(char * curlString) {
		if(curlString != nullptr) {
			std::string newString{curlString};

			curl_free(curlString);

			return newString;
		}

		return std::string();
	}

	/*
	 * HEADER HANDLING (protected)
	 */

	//! Static header function to handle incoming header data.
	/*!
	 * If @c thisPtr is not @c nullptr, the function will forward the
	 *  incoming header data without change to the headerInClass function.
	 *
	 * \param data Pointer to the incoming header data.
	 * \param size Always 1.
	 * \param nitems The size of the incoming header data.
	 * \param thisPtr Pointer to the instance of the Curl class.
	 *
	 *  \returns The number of bytes processed by the in-class function.
	 */
	inline int Curl::header(char * data, std::size_t size, std::size_t nitems, void * thisPtr) {
		if(thisPtr == nullptr) {
			return 0;
		}

		return static_cast<Curl *>(thisPtr)->headerInClass(data, size * nitems);
	}

	//! In-class header function to handle incoming header data.
	/*!
	 * The function will check for a @c X-ts header and save its value.
	 *
	 * \param data Pointer to the incoming data.
	 * \param size The size of the incoming header data.
	 *
	 * \returns The number of bytes processed, which will be identical to size.
	 */
	inline int Curl::headerInClass(char * data, std::size_t size) {
		if(size > xTsHeaderNameLen) {
			bool found{true};

			for(std::size_t n{}; n < xTsHeaderNameLen; ++n) {
				if(data[n] != xTsHeaderName[n]) { //NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
					found = false;

					break;
				}
			}

			if(found) {
				std::stringstream stringStream;

				for(std::size_t n{xTsHeaderNameLen}; n < size; ++n) {
					stringStream << data[n]; //NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
				}

				stringStream >> this->xTsHeaderValue;
			}
		}

		return static_cast<int>(size);
	}

	/*
	 * WRITERS (protected)
	 */

	//! Static writer function to handle incoming network data.
	/*!
	 * If @c thisPtr is not @c nullptr, the function will forward the
	 *  incoming data without change to the writerInClass function.
	 *
	 * \param data Pointer to the incoming data.
	 * \param size Always 1.
	 * \param nmemb The size of the incoming data.
	 * \param thisPtr Pointer to the instance of the Curl class.
	 *
	 *  \returns The number of bytes processed by the in-class function.
	 */
	inline int Curl::writer(char * data, std::size_t size, std::size_t nmemb, void * thisPtr) {
		if(thisPtr == nullptr) {
			return 0;
		}

		return static_cast<Curl *>(thisPtr)->writerInClass(data, size * nmemb);
	}

	//! In-class writer function to handle incoming network data.
	/*!
	 * The function will append the data to the currently processed content.
	 *
	 * \param data Pointer to the incoming data.
	 * \param size The size of the incoming data.
	 *
	 * \returns The number of bytes processed, which will be identical to size.
	 */
	inline int Curl::writerInClass(char * data, std::size_t size) {
		this->content.append(data, size);

		return static_cast<int>(size);
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// set a libcurl option to a string value
	inline void Curl::setOption(CURLoption option, const std::string& stringValue) {
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				option,
				stringValue.c_str()
		);

		this->checkCode();
	}

	// set a libcurl option to a numeric value
	//NOLINTNEXTLINE(google-runtime-int)
	inline void Curl::setOption(CURLoption option, long longValue) {
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				option,
				longValue
		);

		this->checkCode();
	}

	// set a libcurl option to a libcurl list
	inline void Curl::setOption(CURLoption option, CurlList& list) {
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				option,
				list.get()
		);

		this->checkCode();
	}

	// set a libcurl option to a pointer
	inline void Curl::setOption(CURLoption option, void * pointer) {
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		this->curlCode = curl_easy_setopt(
				this->curl.get(),
				option,
				pointer
		);

		this->checkCode();
	}

	// check whether a libcurl feature is supported
	inline bool Curl::hasFeature(int feature) const noexcept {
		return (this->features & feature) == feature; //NOLINT(hicpp-signed-bitwise)
	}

	// check the return code of ANY libcurl function call
	inline void Curl::checkCode() {
		if(this->curlCode != CURLE_OK) {
			throw Curl::Exception(curl_easy_strerror(this->curlCode));
		}
	}

	// clear previously received content, including type, response code, and 'X-ts' header value
	inline void Curl::clearContent() {
		this->content.clear();
		this->contentType.clear();

		this->responseCode = 0;
		this->xTsHeaderValue = 0;
	}

	// prepare POST request and fill POST fields, if necessary, returns escaped URL
	inline std::string Curl::preparePost(std::string_view url, std::string& postFieldsTo) {
		const auto delim{url.find('?')};
		const bool noFields{delim == std::string::npos};

		if(noFields) {
			// set POST method if necessary
			if(!(this->post)) {
				this->setOption(CURLOPT_POST, true);

				this->post = true;
			}

			// set POST field size to zero
			this->setOption(CURLOPT_POSTFIELDSIZE, 0);

			// escape whole URL
			return this->escapeUrl(url);
		}

		// split POST data from URL (and escape the latter)
		postFieldsTo = url.substr(delim + 1);

		// set POST data and its size
		this->setOption(CURLOPT_POSTFIELDSIZE, postFieldsTo.size());
		this->setOption(CURLOPT_POSTFIELDS, postFieldsTo);

		this->post = true;

		return this->escapeUrl(url.substr(0, delim));
	}

	// prepare GET request, returns escaped URL
	inline std::string Curl::prepareGet(std::string_view url) {
		if(this->post) {
			// unset POST method
			this->setOption(CURLOPT_POST, false);

			this->post = false;
		}

		// escape whole URL
		return this->escapeUrl(url);
	}

	// check the result of performing a HTTP request, throws Curl::Exception
	inline void Curl::checkResult(const std::array<char, CURL_ERROR_SIZE>& errorBuffer) {
		// store result
		const auto result{this->curlCode};

		if(result == CURLE_OK) {
			// reset error buffer
			this->setOption(CURLOPT_ERRORBUFFER, nullptr);

			return;
		}

		std::string curlError{curl_easy_strerror(result)};

		if(errorBuffer.at(0) > 0) {
			curlError += " [";
			curlError += errorBuffer.data();
			curlError += "]";
		}

		// reset error buffer
		this->setOption(CURLOPT_ERRORBUFFER, nullptr);

		// restore result (might have been changed by call to setOption())
		this->curlCode = result;

		throw Curl::Exception(curlError);
	}

	// check the response code after performing a HTTP request
	inline void Curl::checkResponseCode(const std::vector<std::uint32_t>& errors) {
		// get response code
		//NOLINTNEXTLINE(google-runtime-int)
		long responseCodeL{};

		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		this->curlCode = curl_easy_getinfo(
				this->curl.get(),
				CURLINFO_RESPONSE_CODE,
				&responseCodeL
		);

		this->checkCode();

		if(
				responseCodeL < 0
				|| responseCodeL > std::numeric_limits<std::uint32_t>::max()
		) {
			throw Curl::Exception("Invalid HTTP response code");
		}

		this->responseCode = static_cast<std::uint32_t>(responseCodeL);

		// check response code for errors that should throw an exception
		if(
				std::find(
						errors.cbegin(),
						errors.cend(),
						this->responseCode
				) != errors.cend()
				&& (
						this->xTsHeaderValue == 0
						|| this->responseCode != this->xTsHeaderValue
				)
		) {
			std::string exceptionString{"HTTP error "};

			exceptionString += std::to_string(this->responseCode);

			throw Curl::Exception(exceptionString);
		}
	}

	// get the content type after performing a HTTP request
	inline void Curl::retrieveContentType() {
		// get content type
		char * cString{nullptr};

		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		this->curlCode = curl_easy_getinfo(
				this->curl.get(),
				CURLINFO_CONTENT_TYPE,
				&cString
		);

		this->checkCode();

		if(cString == nullptr) {
			this->contentType = std::string{};

			return;
		}
		this->contentType = cString;
	}

	// transform content type to lower case and remove spaces
	inline void Curl::processContentType() {
		std::transform(
				this->contentType.begin(),
				this->contentType.end(),
				this->contentType.begin(),
				[](const auto c) {
					return std::tolower(c);
				}
		);

		this->contentType.erase(
				std::remove_if(
						this->contentType.begin(),
						this->contentType.end(),
						[](const auto c) {
							return std::isspace(c);
						}
				),
				this->contentType.end()
		);
	}

	// check for gzipped content that has not been detected by curl
	inline void Curl::checkCompression() {
		// check for gzipped content that curl could not decompress
		if(
				this->content.size() >= gzipMagicNumber.size()
				&& this->contentType.find("gzip") != std::string::npos
		) {
			for(std::size_t byte{}; byte < gzipMagicNumber.size(); ++byte) {
				if(static_cast<unsigned char>(this->content[byte]) != gzipMagicNumber[byte]) {
					return;
				}
			}

			this->content = Data::Compression::Gzip::decompress(this->content);
		}
	}

	// perform character encoding operations
	//  (convert ISO-8859-1 to UTF-8, remove invalid UTF-8 characters)
	inline void Curl::repairEncoding() {
		std::string repairedContent;

		if(this->contentType.find("charset=iso-8859-1") != std::string::npos) {
			this->content = Helper::Utf8::iso88591ToUtf8(this->content);
		}

		if(Helper::Utf8::repairUtf8(this->content, repairedContent)) {
			this->content.swap(repairedContent);
		}
	}

} /* namespace crawlservpp::Network */

#endif /* NETWORK_CURL_HPP_ */
