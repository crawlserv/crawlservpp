/*
 * Config.h
 *
 * Network configuration. This class is used by both the crawler and the extractor.
 *
 * WARNING:	Changing the configuration requires updating 'json/crawler.json' and 'json/extractor.json'
 * 			in crawlserv_frontend! See there for details on the specific configuration entries.
 *
 *  Created on: Jan 8, 2019
 *      Author: ans
 */

#ifndef NETWORK_CONFIG_HPP_
#define NETWORK_CONFIG_HPP_

#include "../Module/Config.hpp"
#include "../Struct/ConfigItem.hpp"

#define RAPIDJSON_HAS_STDSTRING 1
#include "../_extern/rapidjson/include/rapidjson/document.h"

#include <queue>
#include <string>
#include <vector>

namespace crawlservpp::Network {

	/*
	 * DECLARATION
	 */

	class Config {
		// for convenience
		typedef Struct::ConfigItem ConfigItem;

	public:
		Config();
		virtual ~Config();

		// configuration
		unsigned short connectionsMax;
		bool contentLengthIgnore;
		bool cookies;
		std::string cookiesLoad;
		std::vector<std::string> cookiesOverwrite;
		std::string cookiesSave;
		bool cookiesSession;
		std::string cookiesSet;
		long dnsCacheTimeOut;
		std::string dnsDoH;
		std::string dnsInterface;
		std::vector<std::string> dnsResolves;
		std::vector<std::string> dnsServers;
		bool dnsShuffle;
		bool encodingBr;
		bool encodingDeflate;
		bool encodingGZip;
		bool encodingIdentity;
		bool encodingTransfer;
		std::vector<std::string> headers;
		std::vector<std::string> http200Aliases;
		unsigned short httpVersion;
		const static unsigned short httpVersionAny = 0;
		const static unsigned short httpVersionV1 = 1;
		const static unsigned short httpVersionV11 = 2;
		const static unsigned short httpVersionV2 = 3;
		const static unsigned short httpVersionV2only = 4;
		const static unsigned short httpVersionV2tls = 5;
		std::string localInterface;
		unsigned short localPort;
		unsigned short localPortRange;
		std::string proxy;
		std::string proxyAuth;
		std::vector<std::string> proxyHeaders;
		std::string proxyPre;
		std::string proxyTlsSrpPassword;
		std::string proxyTlsSrpUser;
		bool proxyTunnelling;
		bool redirect;
		unsigned long redirectMax;
		bool redirectPost301;
		bool redirectPost302;
		bool redirectPost303;
		std::string referer;
		bool refererAutomatic;
		unsigned long speedDownLimit;
		unsigned long speedLowLimit;
		unsigned long speedLowTime;
		unsigned long speedUpLimit;
		bool sslVerifyHost;
		bool sslVerifyPeer;
		bool sslVerifyProxyHost;
		bool sslVerifyProxyPeer;
		bool sslVerifyStatus;
		bool tcpFastOpen;
		bool tcpKeepAlive;
		unsigned long tcpKeepAliveIdle;
		unsigned long tcpKeepAliveInterval;
		bool tcpNagle;
		unsigned long timeOut;
		unsigned short timeOutHappyEyeballs;
		unsigned long timeOutRequest;
		std::string tlsSrpUser;
		std::string tlsSrpPassword;
		std::string userAgent;
		bool verbose;

		// parse configuration option
		void parseOption(Module::Config& parser);

		// not moveable, not copyable
		Config(Config&) = delete;
		Config(Config&&) = delete;
		Config& operator=(Config&) = delete;
		Config& operator=(Config&&) = delete;
	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor: set default values
	inline Config::Config() :	connectionsMax(5),
								contentLengthIgnore(false),
								cookies(false),
								cookiesSession(true),
								dnsCacheTimeOut(60),
								dnsShuffle(false),
								encodingBr(true),
								encodingDeflate(true),
								encodingGZip(true),
								encodingIdentity(true),
								encodingTransfer(false),
								httpVersion(Config::httpVersionV2tls),
								localPort(0),
								localPortRange(1),
								proxyTunnelling(false),
								redirect(true),
								redirectMax(20),
								redirectPost301(false),
								redirectPost302(false),
								redirectPost303(false),
								refererAutomatic(false),
								speedDownLimit(0),
								speedLowLimit(0),
								speedLowTime(0),
								speedUpLimit(0),
								sslVerifyHost(true),
								sslVerifyPeer(true),
								sslVerifyProxyHost(true),
								sslVerifyProxyPeer(true),
								sslVerifyStatus(false),
								tcpFastOpen(false),
								tcpKeepAlive(false),
								tcpKeepAliveIdle(60),
								tcpKeepAliveInterval(60),
								tcpNagle(false),
								timeOut(300),
								timeOutHappyEyeballs(0),
								timeOutRequest(300),
								verbose(false) {}

	// destructor stub
	inline Config::~Config() {}

	// parse configuration option
	inline void Config::parseOption(Module::Config& parser) {
		parser.option("connections.max", this->connectionsMax);
		parser.option("contentlength.ignore", this->contentLengthIgnore);
		parser.option("cookies", this->cookies);
		parser.option("cookies.load", this->cookiesLoad);
		parser.option("cookies.overwrite", this->cookiesOverwrite);
		parser.option("cookies.save", this->cookiesSave);
		parser.option("cookies.session", this->cookiesSession);
		parser.option("cookies.set", this->cookiesSet);
		parser.option("dns.cachetimeout", this->dnsCacheTimeOut);
		parser.option("dns.doh", this->dnsDoH);
		parser.option("dns.interface", this->dnsInterface);
		parser.option("dns.resolves", this->dnsResolves);
		parser.option("dns.servers", this->dnsServers);
		parser.option("dns.shuffle", this->dnsShuffle);
		parser.option("encoding.br", this->encodingBr);
		parser.option("encoding.deflate", this->encodingDeflate);
		parser.option("encoding.gzip", this->encodingGZip);
		parser.option("encoding.identity", this->encodingIdentity);
		parser.option("encoding.transfer", this->encodingTransfer);
		parser.option("headers", this->headers);
		parser.option("http.200aliases", this->http200Aliases);
		parser.option("http.version", this->httpVersion);
		parser.option("local.interface", this->localInterface);
		parser.option("local.port", this->localPort);
		parser.option("local.portrange", this->localPortRange);
		parser.option("proxy", this->proxy);
		parser.option("proxy.auth", this->proxyAuth);
		parser.option("proxy.headers", this->proxyHeaders);
		parser.option("proxy.pre", this->proxyPre);
		parser.option("proxy.tlssrp.password", this->proxyTlsSrpPassword);
		parser.option("proxy.tlssrp.user", this->proxyTlsSrpUser);
		parser.option("proxy.tunnelling", this->proxyTunnelling);
		parser.option("redirect", this->redirect);
		parser.option("redirect.max", this->redirectMax);
		parser.option("redirect.post301", this->redirectPost301);
		parser.option("redirect.post302", this->redirectPost302);
		parser.option("redirect.post303", this->redirectPost303);
		parser.option("referer", this->referer);
		parser.option("referer.automatic", this->refererAutomatic);
		parser.option("speed.downlimit", this->speedDownLimit);
		parser.option("speed.lowlimit", this->speedLowLimit);
		parser.option("speed.lowtime", this->speedLowTime);
		parser.option("speed.uplimit", this->speedUpLimit);
		parser.option("ssl.verify.host", this->sslVerifyHost);
		parser.option("ssl.verify.peer", this->sslVerifyPeer);
		parser.option("ssl.verify.proxy.host", this->sslVerifyProxyHost);
		parser.option("ssl.verify.proxy.peer", this->sslVerifyProxyPeer);
		parser.option("ssl.verify.status", this->sslVerifyStatus);
		parser.option("tcp.fastopen", this->tcpFastOpen);
		parser.option("tcp.keepalive", this->tcpKeepAlive);
		parser.option("tcp.keepalive.idle", this->tcpKeepAliveIdle);
		parser.option("tcp.keepalive.interval", this->tcpKeepAliveInterval);
		parser.option("tcp.nagle", this->tcpNagle);
		parser.option("timeout", this->timeOut);
		parser.option("timeout.happyeyeballs", this->timeOutHappyEyeballs);
		parser.option("timeout.request", this->timeOutRequest);
		parser.option("tlssrp.password", this->tlsSrpPassword);
		parser.option("tlssrp.user", this->tlsSrpUser);
		parser.option("useragent", this->userAgent);
		parser.option("verbose", this->verbose);
	}

} /* crawlservpp::Network */

#endif /* NETWORK_CONFIG_HPP_ */
