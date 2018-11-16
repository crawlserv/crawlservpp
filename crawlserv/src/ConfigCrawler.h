/*
 * ConfigCrawler.h
 *
 * Configuration for crawlers.
 *
 * WARNING:	Changing the configuration requires updating 'json/crawler.json' in crawlserv_frontend!
 * 			See there for details on the specific configuration entries.
 *
 *  Created on: Oct 25, 2018
 *      Author: ans
 */

#ifndef CONFIGCRAWLER_H_
#define CONFIGCRAWLER_H_

#include "external/rapidjson/document.h"

#include <sstream>
#include <string>
#include <vector>

class ConfigCrawler {
public:
	ConfigCrawler();
	virtual ~ConfigCrawler();

	// configuration loader
	bool loadConfig(const std::string& configJson, std::vector<std::string>& warningsTo);

	// get error message
	const std::string& getErrorMessage() const;

	// crawler entries
	bool crawlerArchives;
	unsigned int crawlerLock;
	unsigned short crawlerLogging;
	static const unsigned short crawlerLoggingSilent = 0;
	static const unsigned short crawlerLoggingDefault = 1;
	static const unsigned short crawlerLoggingExtended = 2;
	static const unsigned short crawlerLoggingVerbose = 3;
	std::vector<std::string> crawlerParamsBlackList;
	std::vector<std::string> crawlerParamsWhiteList;
	std::vector<unsigned long> crawlerQueriesBlackListContent;
	std::vector<unsigned long> crawlerQueriesBlackListTypes;
	std::vector<unsigned long> crawlerQueriesBlackListUrls;
	std::vector<unsigned long> crawlerQueriesLinks;
	std::vector<unsigned long> crawlerQueriesWhiteListContent;
	std::vector<unsigned long> crawlerQueriesWhiteListTypes;
	std::vector<unsigned long> crawlerQueriesWhiteListUrls;
	bool crawlerReCrawl;
	std::vector<std::string> crawlerReCrawlAlways;
	bool crawlerReCrawlStart;
	long crawlerReTries;
	bool crawlerRetryArchive;
	std::vector<unsigned int> crawlerRetryHttp;
	unsigned long crawlerSleepError;
	unsigned long crawlerSleepHttp;
	unsigned long crawlerSleepIdle;
	unsigned long crawlerSleepMySql;
	std::string crawlerStart;
	bool crawlerTiming;
	bool crawlerWarningsFile;
	bool crawlerXml;

	// custom entries
	std::vector<std::string> customCounters;
	std::vector<long> customCountersEnd;
	bool customCountersGlobal;
	std::vector<long> customCountersStart;
	std::vector<long> customCountersStep;
	bool customReCrawl;
	std::vector<std::string> customUrls;

	// network entries
	unsigned short networkConnectionsMax;
	bool networkContentLengthIgnore;
	bool networkCookies;
	std::string networkCookiesLoad;
	std::vector<std::string> networkCookiesOverwrite;
	std::string networkCookiesSave;
	bool networkCookiesSession;
	std::string networkCookiesSet;
	long networkDnsCacheTimeOut;
	std::string networkDnsDoH;
	std::string networkDnsInterface;
	std::vector<std::string> networkDnsResolves;
	std::vector<std::string> networkDnsServers;
	bool networkDnsShuffle;
	bool networkEncodingBr;
	bool networkEncodingDeflate;
	bool networkEncodingGZip;
	bool networkEncodingIdentity;
	bool networkEncodingTransfer;
	std::vector<std::string> networkHeaders;
	std::vector<std::string> networkHttp200Aliases;
	unsigned short networkHttpVersion;
	const static unsigned short networkHttpVersionAny = 0;
	const static unsigned short networkHttpVersionV1 = 1;
	const static unsigned short networkHttpVersionV11 = 2;
	const static unsigned short networkHttpVersionV2 = 3;
	const static unsigned short networkHttpVersionV2only = 4;
	const static unsigned short networkHttpVersionV2tls = 5;
	std::string networkLocalInterface;
	unsigned short networkLocalPort;
	unsigned short networkLocalPortRange;
	std::string networkProxy;
	std::string networkProxyAuth;
	std::vector<std::string> networkProxyHeaders;
	std::string networkProxyPre;
	std::string networkProxyTlsSrpPassword;
	std::string networkProxyTlsSrpUser;
	bool networkProxyTunnelling;
	bool networkRedirect;
	unsigned long networkRedirectMax;
	bool networkRedirectPost301;
	bool networkRedirectPost302;
	bool networkRedirectPost303;
	std::string networkReferer;
	bool networkRefererAutomatic;
	unsigned long networkSpeedDownLimit;
	unsigned long networkSpeedLowLimit;
	unsigned long networkSpeedLowTime;
	unsigned long networkSpeedUpLimit;
	bool networkSslVerifyHost;
	bool networkSslVerifyPeer;
	bool networkSslVerifyProxyHost;
	bool networkSslVerifyProxyPeer;
	bool networkSslVerifyStatus;
	bool networkTcpFastOpen;
	bool networkTcpKeepAlive;
	unsigned long networkTcpKeepAliveIdle;
	unsigned long networkTcpKeepAliveInterval;
	bool networkTcpNagle;
	unsigned long networkTimeOut;
	unsigned short networkTimeOutHappyEyeballs;
	unsigned long networkTimeOutRequest;
	std::string networkTlsSrpUser;
	std::string networkTlsSrpPassword;
	std::string networkUserAgent;
	bool networkVerbose;

private:
	std::string errorMessage;
};

#endif /* CONFIGCRAWLER_H_ */
