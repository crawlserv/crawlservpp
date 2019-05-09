/*
 * Config.hpp
 *
 * Network configuration. This class is used by both the crawler and the extractor.
 *
 * WARNING:	Changing the configuration requires updating 'json/include/network.json'
 * 			in crawlserv_frontend! See there for details on the specific configuration entries.
 *
 *  Created on: Jan 8, 2019
 *      Author: ans
 */

#ifndef NETWORK_CONFIG_HPP_
#define NETWORK_CONFIG_HPP_

#include "../Module/Config.hpp"

#include <string>
#include <vector>

namespace crawlservpp::Network {

	/*
	 * DECLARATION
	 */

	class Config : public Module::Config {
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
		std::string cookiesSet; // (crawler only)
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

		// parse network configuration option
		void parseOption() override;

		// parse additional configuration options
		virtual void parseAdditionalOption() = 0;
	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor: set default values
	inline Config::Config() :	Module::Config(),
								connectionsMax(5),
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
	inline void Config::parseOption() {
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
		this->option("headers", this->headers);
		this->option("http.200aliases", this->http200Aliases);
		this->option("http.version", this->httpVersion);
		this->option("local.interface", this->localInterface);
		this->option("local.port", this->localPort);
		this->option("local.portrange", this->localPortRange);
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

		this->parseAdditionalOption();
	}

} /* crawlservpp::Network */

#endif /* NETWORK_CONFIG_HPP_ */
