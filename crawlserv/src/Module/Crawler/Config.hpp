/*
 * Config.h
 *
 * Crawling configuration.
 *
 * WARNING:	Changing the configuration requires updating 'json/crawler.json' in crawlserv_frontend!
 * 			See there for details on the specific configuration entries.
 *
 *  Created on: Oct 25, 2018
 *      Author: ans
 */

#ifndef MODULE_CRAWLER_CONFIG_H_
#define MODULE_CRAWLER_CONFIG_H_

#include "../Config.h"

#include "../../Helper/Algos.h"
#include "../../Network/Config.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include "../../_extern/rapidjson/include/rapidjson/document.h"

#include <queue>
#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Module::Crawler {
class Config : public Module::Config {
public:
	Config();
	virtual ~Config();

	// crawler entries
	bool crawlerArchives;
	std::vector<std::string> crawlerArchivesNames;
	std::vector<std::string> crawlerArchivesUrlsMemento;
	std::vector<std::string> crawlerArchivesUrlsTimemap;
	bool crawlerHTMLCanonicalCheck;
	bool crawlerHTMLConsistencyCheck;
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
	bool crawlerUrlCaseSensitive;
	unsigned long crawlerUrlChunks;
	bool crawlerUrlDebug;
	bool crawlerUrlStartupCheck;
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
	Network::Config network;

protected:
	// load crawling-specific configuration from parsed JSON document
	void loadModule(const rapidjson::Document& jsonDocument, std::queue<std::string>& warningsTo) override;

private:
	// rudimentary check for sub-URL or curl-supported URL protocol beginning - adds a slash in the beginning if no protocol is found
	static std::string makeSubUrl(const std::string& source);
};

}

#endif /* MODULE_CRAWLER_CONFIG_H_ */
