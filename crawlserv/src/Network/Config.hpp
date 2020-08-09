/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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
 * Config.hpp
 *
 * Network configuration. This class is used by both the crawler and the extractor.
 *
 * WARNING:	Changing the configuration requires updating 'json/include/network.json'
 * 			in crawlserv_frontend! See there for  details on the specific configuration entries.
 *
 *  Created on: Jan 8, 2019
 *      Author: ans
 */

#ifndef NETWORK_CONFIG_HPP_
#define NETWORK_CONFIG_HPP_

#include "../Module/Config.hpp"

#include <cstdint>		// std::int64_t, std::uint16_t, std::uint64_t
#include <string>		// std::string
#include <string_view>	// std::string_view_literals
#include <vector>		// std::vector

//! Namespace for networking classes.
namespace crawlservpp::Network {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! Use any available HTTP version.
	inline constexpr std::uint16_t httpVersionAny{0};

	//! Use HTTP/1 only.
	inline constexpr std::uint16_t httpVersion1{1};

	//! Use HTTP/1.1 only.
	inline constexpr std::uint16_t httpVersion11{2};

	//! Attempt to use HTTP/2, fall back to HTTP/1.1.
	inline constexpr std::uint16_t httpVersion2{3};

	//! Use non-TLS HTTP/2, even if HTTPS is not available.
	inline constexpr std::uint16_t httpVersion2Only{4};

	//! Attempt to use HTTP/2 over TLS, fall back to HTTP/1.1.
	inline constexpr std::uint16_t httpVersion2Tls{5};

	//! Use HTTP/3 only.
	/*!
	 * \warning Fails, if a server does not support HTTP/3.
	 */
	inline constexpr std::uint16_t httpVersion3Only{6};

	//! Default maximum number of connections.
	inline constexpr std::uint16_t defaultConnectionsMax{5};

	//! Default lifetime of DNS cache entries.
	inline constexpr std::int64_t defaultDnsCacheTimeOut{60};

	//! Default maximum number of automatic redirects by default.
	inline constexpr std::uint64_t defaultRedirectMax{20};

	//! Default number of seconds that need to have been passed before requesting a new TOR identity.
	inline constexpr std::uint64_t defaultResetTorOnlyAfter{60};

	//! Default delay that will be waited before sending keep-alive probes, in seconds.
	inline constexpr std::uint64_t defaultTcpKeepAliveIdle{60};

	//! Default interval for TCP Keep-alive probing, in seconds
	inline constexpr std::uint64_t defaultTcpKeepAliveInterval{60};

	//! Default connecting time-out, in seconds.
	inline constexpr std::uint64_t defaultTimeOut{300};

	//! Default request time-out, in seconds.
	inline constexpr std::uint64_t defaultTimeOutRequest{300};

	//! Default protocol.
	inline constexpr auto defaultProtocol{"https://"sv};

	///@}

	/*
	 * DECLARATION
	 */

	//! Abstract class containing the network-specific configuration for threads.
	/*!
	 * See also the
	 *  <a href="https://curl.haxx.se/libcurl/c/curl_easy_setopt.html">
	 *  documentation of libcurl's @c curl_easy_setopt
	 *  function</a> for more information about specific
	 *  networking options.
	 */
	class Config : protected Module::Config {
	public:
		///@name Network Configuration
		///@{

		//! The maximum number of parallel connections.
		std::uint16_t connectionsMax{defaultConnectionsMax};

		//! Specifies whether the @c Content-Length header in HTTP responses will be ignored.
		bool contentLengthIgnore{false};

		//! Specifies whether the internal cookie engine will be enabled.
		/*!
		 * The internal cookie engine temporarily
		 *  stores and re-sends received cookies.
		 *  They will be reset as soon as the
		 *  connection is reset.
		 *
		 * Use Network::Config::cookiesLoad and
		 *  Network::Config::cookiesSave for
		 *  a more permanent solution.
		 *
		 * \sa cookiesOverwrite, cookiesSession
		 */
		bool cookies{false};

		//! The file from which cookies will be read.
		/*!
		 * Will be ignored when empty.
		 *
		 * \sa cookies, cookiesOverwrite,
		 *   cookiesSave, cookiesSession
		 */
		std::string cookiesLoad;

		//! Cookies to be overwritten.
		/*!
		 * \sa cookies, cookiesLoad,
		 *   cookiesSave, cookiesSession
		 */
		std::vector<std::string> cookiesOverwrite;

		//! The file to which cookies will be saved.
		/*!
		 * Will be ignored when empty.
		 *
		 * \sa cookies, cookiesLoad,
		 *   cookiesOverwrite, cookiesSession
		 */
		std::string cookiesSave;

		//! Specifies whether to ignore obsolete session cookies.
		/*!
		 * \sa cookies, cookiesLoad,
		 *   cookiesOverwrite, cookiesSave
		 */
		bool cookiesSession{true};

		//! Custom HTTP @c Cookie header independent from the internal cookie engine.
		/*!
		 * Will be ignored when empty.
		 *
		 * Will be overwritten if a cookie is set during dynamic redirect.
		 *
		 * Used by the crawler only.
		 */
		std::string cookiesSet;

		//! The lifetime of DNS cache entries.
		/*!
		 * Set to -1 for a infinite lifetime.
		 */
		std::int64_t dnsCacheTimeOut{defaultDnsCacheTimeOut};

		//! The URL of a custom DNS-over-HTTPS (DoH) server.
		/*!
		 * Will be ignored when empty.
		 */
		std::string dnsDoH;

		//! The interface that DNS name resolves should be bound to.
		/*!
		 * Will be ignored when empty.
		 */
		std::string dnsInterface;

		//! DNS name resolves to be overwritten.
		/*!
		 * Each resolve should be in the format
		 *  @c HOST:PORT:ADDRESS[,ADDRESS]..., where
		 *  @c HOST is the name @c libcurl will try
		 *  to resolve, @c PORT is the port number
		 *  of the service where @ libcurl wants
		 *  to connect to the @c HOST and @c ADDRESS
		 *  is one or more numerical IP addresses.
		 *  Multiple IP addresses need to be separated
		 *  by comma.
		 */
		std::vector<std::string> dnsResolves;

		//! DNS servers to be preffered.
		std::vector<std::string> dnsServers;

		//! Specifies whether to shuffle addresses when a host name returns more than one.
		bool dnsShuffle{false};

		//! Specifies whether to request @c brotli encoding for requested content.
		/*!
		 * If no encoding for the requested content
		 *  will be specified, no @c Accept-Encoding
		 *  header will be sent and no decompression
		 *  of received content will be performed.
		 *
		 * \sa encodingDeflate, encodingGZip,
		 *   encodingIdentity, encodingZstd
		 */
		bool encodingBr{true};

		//! Specifies whether to request @c DEFLATE encoding for requested content.
		/*!
		 * If no encoding for the requested content
		 *  will be specified, no @c Accept-Encoding
		 *  header will be sent and no decompression
		 *  of received content will be performed.
		 *
		 * \sa encodingBr, encodingGZip,
		 *   encodingIdentity, encodingZstd
		 */
		bool encodingDeflate{true};

		//! Specifies whether to request @c gzip encoding for requested content.
		/*!
		 * If no encoding for the requested content
		 *  will be specified, no @c Accept-Encoding
		 *  header will be sent and no decompression
		 *  of received content will be performed.
		 *
		 * \sa encodingBr, encodingDeflate,
		 *   encodingIdentity, encodingZstd
		 */
		bool encodingGZip{true};

		//! Specifies whether to (also) request non-compressed encoding for requested content.
		/*!
		 * If no encoding for the requested content
		 *  will be specified, no @c Accept-Encoding
		 *  header will be sent and no decompression
		 *  of received content will be performed.
		 *
		 * \sa encodingBr, encodingDeflate,
		 *   encodingGZip, encodingZstd
		 */
		bool encodingIdentity{true};

		//! Specifies whether to request HTTP Transfer Encoding.
		bool encodingTransfer{false};

		//! Specifies whether to request @c Zstandard encoding for requested content.
		/*!
		 * If no encoding for the requested content
		 *  will be specified, no @c Accept-Encoding
		 *  header will be sent and no decompression
		 *  of received content will be performed.
		 *
		 * \sa encodingBr, encodingDeflate,
		 *   encodingGZip, encodingIdentity
		 */
		bool encodingZstd{false};

		//! Custom HTTP headers to be sent with every request.
		std::vector<std::string> headers;

		//! Aliases that will be treated like @c HTTP/1.0 @c 200 @c OK.
		std::vector<std::string> http200Aliases;

		//! HTTP version(s) to be used.
		/*!
		 * \sa httpVersionAny, httpVersion1,
		 *  httpVersion11, httpVersion2,
		 *  httpVersion2Only, httpVersion2Tls,
		 *  httpVersion3Only
		 */
		std::uint16_t httpVersion{httpVersion2Tls};

		//! Interface to be used for outgoing traffic.
		/*!
		 * Will be ignored when empty.
		 */
		std::string localInterface;

		//! Port to be used for outgoing traffic.
		/*!
		 * Set to zero if any port is fine.
		 *
		 * \sa localPortRange
		 */
		std::uint16_t localPort{0};

		//! Number of ports to be tried for outgoing traffic.
		/*!
		 * If greater than one, a range of ports
		 *  starting with localPort will be tried
		 *  for outgoing traffic.
		 *
		 * Set to one or below to not try any
		 *  additional ports.
		 *
		 * \sa localPort
		 */
		std::uint16_t localPortRange{1};

		//! Specifies whether to prevent connections from re-using previous ones.
		/*!
		 * This might lead to a large number
		 *  of re-connects, which might in turn
		 *  negatively affect performance.
		 */
		bool noReUse{false};

		//! Proxy server used.
		/*!
		 * Will be ignored when empty.
		 *
		 * \sa proxyAuth, proxyHeaders
		 */
		std::string proxy;

		//! Authentification for the proxy server used.
		/*!
		 * The authentification needs to be
		 *  in the format @c user:password.
		 *
		 * Will be ignored when empty.
		 *
		 * \sa proxy, proxyHeaders
		 */
		std::string proxyAuth;

		//! Custom HTTP headers to be sent to the proxy server.
		/*!
		 * \sa proxy, proxyAuth
		 */
		std::vector<std::string> proxyHeaders;

		//! Pre-proxy server to be used.
		/*!
		 * Will be ignored when empty.
		 */
		std::string proxyPre;

		//! TSL-SRP password for the proxy server used.
		/*!
		 * Will be ignored when empty.
		 *
		 * \sa proxyTlsSrpUser
		 */
		std::string proxyTlsSrpPassword;

		//! TSL-SRP user for the proxy server used.
		/*!
		 * Will be ignored when empty.
		 *
		 *
		 * \sa proxyTlsSrpPassword
		 */
		std::string proxyTlsSrpUser;

		//! Specifies whether to enable proxy tunnelling.
		bool proxyTunnelling{false};

		//! Specifies whether to follow HTTP @c Location headers for automatic redirects.
		/*!
		 * \sa redirectMax
		 */
		bool redirect{true};

		//! The maximum number of automatic redirects.
		/*!
		 * Set to @c -1 to disable the limit.
		 *
		 * \sa redirect
		 */
		std::uint64_t redirectMax{defaultRedirectMax};

		//! Specifies whether to NOT convert POST to GET requests when following 301 redirects.
		/*!
		 * \sa redirectPost302, redirectPost303
		 */
		bool redirectPost301{false};

		//! Specifies whether to NOT convert POST to GET requests when following 302 redirects.
		/*!
		 * \sa redirectPost301, redirectPost303
		 */
		bool redirectPost302{false};

		//! Specifies whether to NOT convert POST to GET requests when following 303 redirects.
		/*!
		 * \sa redirectPost301, redirectPost302
		 */
		bool redirectPost303{false};

		//! The HTTP @c Referer header to be set.
		/*!
		 * No custom HTTP @c Referer header will be sent
		 *  if the string is empty.
		 *
		 * \sa refererAutomatic
		 */
		std::string referer;

		//! Specifies whether to send an updated HTTP @c Referer header when automatically redirected.
		/*!
		 * \sa referer
		 */
		bool refererAutomatic{false};

		//! Specifies whether to use the TOR control server to request a new identity on connection resets.
		/*!
		 * \sa resetTorAfter, resetTorOnlyAfter
		 */
		bool resetTor{true};

		//! Number of seconds until automatically using the TOR control server to request a new identity.
		/*!
		 * Set to zero to deactivate the
		 *  automatic request for a new
		 *  TOR identity after a specific
		 *  amount of time has passed.
		 */
		/*!
		 * \sa resetTor, resetTorOnlyAfter
		 */
		std::uint64_t resetTorAfter{0};

		//! Number of seconds that need to be parsed before new identity will be requested from the TOR control server.
		/*!
		 * \sa resetTor, resetTorAfter
		 */
		std::uint64_t resetTorOnlyAfter{defaultResetTorOnlyAfter};

		//! Maximum download speed in bytes per second.
		/*!
		 * Set to zero for unlimited download speed.
		 */
		std::uint64_t speedDownLimit{0};

		//! Low speed limit in bytes per second.
		/*!
		 * Set to zero for not using any low speed limit.
		 *
		 * \sa speedLowTime
		 */
		std::uint64_t speedLowLimit{0};

		//! Number of seconds before a timeout occurs while the transfer speed is below the low speed limit.
		/*!
		 * Set to zero to disable timeouts because
		 *  of the low speed limit.
		 *
		 * \sa speedLowLimit
		 */
		std::uint64_t speedLowTime{0};

		//! Maximum upload speed in bytes per second.
		/*!
		 * Set to zero for unlimited upload speed.
		 */
		std::uint64_t speedUpLimit{0};

		//! Specifies whether to verify that the SSL certificate is for the server it is known as.
		/*!
		 * \sa sslVerifyPeer, sslVerifyProxyHost,
		 *   sslVerifyProxyPeer, sslVerifyStatus
		 */
		bool sslVerifyHost{true};

		//! Specifies whether to verify the authenticity of the server's SSL certificate.
		/*!
		 * \sa sslVerifyHost, sslVerifyProxyHost,
		 *   sslVerifyProxyPeer, sslVerifyStatus
		 */
		bool sslVerifyPeer{true};

		//! Specifies whether to verify that the SSL certificate is for the proxy server it is known as.
		/*!
		 * \sa sslVerifyHost, sslVerifyPeer,
		 *   sslVerifyProxyPeer, sslVerifyStatus
		 */
		bool sslVerifyProxyHost{true};

		//! Specifies whether to verify the authenticity of the proxy's SSL certificate.
		/*!
		 * \sa sslVerifyHost, sslVerifyPeer,
		 *   sslVerifyProxyHost, sslVerifyStatus
		 */
		bool sslVerifyProxyPeer{true};

		//! Specifies whether to verify the status of the server's SSL certificate.
		/*!
		 * For this verification, the "Certificate
		 *  Status Request" TLS extension (i.e.
		 *  "OCSCP stapling") will be used.
		 *
		 * \note If activated and a server does
		 *   not support the TLS extension,
		 *   the verification will fail.
		 *
		 * \sa sslVerifyHost, sslVerifyPeer,
		 *   sslVerifyProxyHost, sslVerifyProxyPeer
		 */
		bool sslVerifyStatus{false};

		//! Specifies whether TCP Fast Open will be enabled.
		bool tcpFastOpen{false};

		//! Specifies whether TCP keep-alive probing will be enabled.
		bool tcpKeepAlive{false};

		//! The delay that will be waited before sending keep-alive probes, in seconds.
		/*!
		 * \sa tcpKeepAlive, tcpKeepAliveInterval
		 */
		std::uint64_t tcpKeepAliveIdle{defaultTcpKeepAliveIdle};

		//! The interval time between keep-alive probes to sent, in seconds.
		/*!
		 * \sa tcpKeepAlive, tcpKeepAliveIdle
		 */
		std::uint64_t tcpKeepAliveInterval{defaultTcpKeepAliveInterval};

		//! Specifies whether the TCP's Nagle algorithm is enabled on this connection.
		/*!
		 * The purpose of this algorithm is
		 *  to try to minimize the number of
		 *  small packets on the network.
		 */
		bool tcpNagle{false};

		//! The maximum amount of time a connection is allowed to take, in seconds.
		/*!
		 * Set to zero to disable timeouts
		 *  (not recommended).
		 *
		 * \sa timeOutRequest
		 */
		std::uint64_t timeOut{defaultTimeOut};

		//! Number of milliseconds to try to connect only via IPv6 using the Happy Eyeballs algorithm.
		/*!
		 * After this amount of milliseconds,
		 *  a connection attempt is made to
		 *  the IPv4 address in parallel.
		 *
		 * Set to zero for using the default
		 *  value provided by @c libcurl.
		 */
		std::uint16_t timeOutHappyEyeballs{0};

		//! The maximum amount of time a request is allowed to take, in seconds.
		/*!
		 * Set to zero to disable timeouts
		 *  (not recommended).
		 *
		 * \sa timeOut
		 */
		std::uint64_t timeOutRequest{defaultTimeOutRequest};

		//! Password used for TLS-SRP authentification.
		/*!
		 * Will be ignored when both tlsSrpUser
		 *  and tlsSrpPassword are empty.
		 *
		 * \sa tlsSrpPassword
		 */
		std::string tlsSrpUser;

		//! User name used for TLS-SRP authentification.
		/*!
		 * Will be ignored when both tlsSrpUser
		 *  and tlsSrpPassword are empty.
		 *
		 * \sa tlsSrpUser
		 */
		std::string tlsSrpPassword;

		//! Custom HTTP @c User-Agent header to be sent with all HTTP requests.
		/*!
		 * No HTTP @c User-Agent header will be sent when empty.
		 */
		std::string userAgent;

		//! Specifies whether @c libtidy should produce verbose output.
		/*!
		 * \warning Should be used for debugging
		 *   purposes only!
		 *
		 * The output will be written to @c stdout.
		 */
		bool verbose{false};

		//! The protocol to be used for HTTP requests.
		/*!
		 * Including @c :// at the end of the protocol,
		 *  e.g. @c https:// or @c http://.
		 */
		std::string protocol{defaultProtocol};

		///@}
		///@name Parsing (Network Configuration)
		///@{

		void parseBasicOption() override;

		//! Parses additional configuration options.
		/*!
		 * Needs to be implemented by inherited
		 *  classes.
		 *
		 * \sa Module::Crawler::Config::parseOption,
		 *   Module::Extractor::Config::parseOption
		 */
		void parseOption() override = 0;

		///@}
		///@name Helper (Network Configuration)
		///@{

		[[nodiscard]] const std::string& getProtocol() const;

		///@}
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Parses basic network configuration options.
	inline void Config::parseBasicOption() {
		this->category("network");

		this->option("connections.max", this->connectionsMax);
		this->option("contentlength.ignore", this->contentLengthIgnore);
		this->option("cookies", this->cookies);
		this->option("cookies.load", this->cookiesLoad);
		this->option("cookies.overwrite", this->cookiesOverwrite);
		this->option("cookies.save", this->cookiesSave);
		this->option("cookies.session", this->cookiesSession);
		this->option("cookies.set", this->cookiesSet);
		this->option("dns.cachetimeout", this->dnsCacheTimeOut);
		this->option("dns.doh", this->dnsDoH);
		this->option("dns.interface", this->dnsInterface);
		this->option("dns.resolves", this->dnsResolves);
		this->option("dns.servers", this->dnsServers);
		this->option("dns.shuffle", this->dnsShuffle);
		this->option("encoding.br", this->encodingBr);
		this->option("encoding.deflate", this->encodingDeflate);
		this->option("encoding.gzip", this->encodingGZip);
		this->option("encoding.identity", this->encodingIdentity);
		this->option("encoding.transfer", this->encodingTransfer);
		this->option("encoding.zstd", this->encodingZstd);
		this->option("headers", this->headers);
		this->option("http.200aliases", this->http200Aliases);
		this->option("http.version", this->httpVersion);
		this->option("local.interface", this->localInterface);
		this->option("local.port", this->localPort);
		this->option("local.portrange", this->localPortRange);
		this->option("no.reuse", this->noReUse);
		this->option("proxy", this->proxy);
		this->option("proxy.auth", this->proxyAuth);
		this->option("proxy.headers", this->proxyHeaders);
		this->option("proxy.pre", this->proxyPre);
		this->option("proxy.tlssrp.password", this->proxyTlsSrpPassword);
		this->option("proxy.tlssrp.user", this->proxyTlsSrpUser);
		this->option("proxy.tunnelling", this->proxyTunnelling);
		this->option("redirect", this->redirect);
		this->option("redirect.max", this->redirectMax);
		this->option("redirect.post301", this->redirectPost301);
		this->option("redirect.post302", this->redirectPost302);
		this->option("redirect.post303", this->redirectPost303);
		this->option("referer", this->referer);
		this->option("referer.automatic", this->refererAutomatic);
		this->option("reset.tor", this->resetTor);
		this->option("reset.tor.after", this->resetTorAfter);
		this->option("reset.tor.only.after", this->resetTorOnlyAfter);
		this->option("speed.downlimit", this->speedDownLimit);
		this->option("speed.lowlimit", this->speedLowLimit);
		this->option("speed.lowtime", this->speedLowTime);
		this->option("speed.uplimit", this->speedUpLimit);
		this->option("ssl.verify.host", this->sslVerifyHost);
		this->option("ssl.verify.peer", this->sslVerifyPeer);
		this->option("ssl.verify.proxy.host", this->sslVerifyProxyHost);
		this->option("ssl.verify.proxy.peer", this->sslVerifyProxyPeer);
		this->option("ssl.verify.status", this->sslVerifyStatus);
		this->option("tcp.fastopen", this->tcpFastOpen);
		this->option("tcp.keepalive", this->tcpKeepAlive);
		this->option("tcp.keepalive.idle", this->tcpKeepAliveIdle);
		this->option("tcp.keepalive.interval", this->tcpKeepAliveInterval);
		this->option("tcp.nagle", this->tcpNagle);
		this->option("timeout", this->timeOut);
		this->option("timeout.happyeyeballs", this->timeOutHappyEyeballs);
		this->option("timeout.request", this->timeOutRequest);
		this->option("tlssrp.password", this->tlsSrpPassword);
		this->option("tlssrp.user", this->tlsSrpUser);
		this->option("useragent", this->userAgent);
		this->option("verbose", this->verbose);

		bool insecure{false};

		this->option("insecure", insecure);

		if(insecure) {
			this->warning("Using INSECURE connections.");

			this->protocol = "http://";
		}

		this->parseOption();
	}

	//! Gets the protocol to be used for networking.
	/*!
	 * \returns A const reference to the string containing the
	 *   URI component of the protocol to be used for networking,
	 *   either "https://" or "http://".
	 */
	inline const std::string& Config::getProtocol() const {
		return this->protocol;
	}

} /* namespace crawlservpp::Network */

#endif /* NETWORK_CONFIG_HPP_ */
