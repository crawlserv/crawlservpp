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

#ifndef NETWORK_CONFIG_H_
#define NETWORK_CONFIG_H_

#define RAPIDJSON_HAS_STDSTRING 1
#include "../_extern/rapidjson/include/rapidjson/document.h"

#include <queue>
#include <string>
#include <vector>

namespace crawlservpp::Network {
	class Config {
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

		// setter
		void setEntry(const std::string& name, const rapidjson::Value::ConstMemberIterator& iterator,
				std::queue<std::string>& warningsTo);

		// not moveable, not copyable
		Config(Config&) = delete;
		Config(Config&&) = delete;
		Config& operator=(Config&) = delete;
		Config& operator=(Config&&) = delete;
	};
}

#endif /* NETWORK_CONFIG_H_ */
