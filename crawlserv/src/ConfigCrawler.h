/*
 * ConfigCrawler.h
 *
 * Crawling configuration.
 *
 * WARNING:	Changing the configuration requires updating 'json/crawler.json' in crawlserv_frontend!
 * 			See there for details on the specific configuration entries.
 *
 *  Created on: Oct 25, 2018
 *      Author: ans
 */

#ifndef CONFIGCRAWLER_H_
#define CONFIGCRAWLER_H_

#include "ConfigModule.h"
#include "ConfigNetwork.h"

#include "external/rapidjson/document.h"

#include <sstream>
#include <string>
#include <vector>

class ConfigCrawler : public ConfigModule {
public:
	ConfigCrawler();
	virtual ~ConfigCrawler();

	// crawler entries
	bool crawlerArchives;
	std::vector<std::string> crawlerArchivesNames;
	std::vector<std::string> crawlerArchivesUrlsMemento;
	std::vector<std::string> crawlerArchivesUrlsTimemap;
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
	ConfigNetwork network;

protected:
	// load crawling-specific configuration from parsed JSON document
	void loadModule(const rapidjson::Document& jsonDocument, std::vector<std::string>& warningsTo) override;
};

#endif /* CONFIGCRAWLER_H_ */
