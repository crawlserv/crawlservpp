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

#ifndef MODULE_CRAWLER_CONFIG_HPP_
#define MODULE_CRAWLER_CONFIG_HPP_

#include "../../Module/Config.hpp"
#include "../../Network/Config.hpp"
#include "../../Struct/ConfigItem.hpp"

#include <algorithm>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Module::Crawler {

	/*
	 * DECLARATION
	 */

	class Config : public Module::Config {
		// for convenience
		typedef Struct::ConfigItem ConfigItem;

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

	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor: set default values
	inline Config::Config() :	crawlerArchives(false),
								crawlerHTMLCanonicalCheck(false),
								crawlerHTMLConsistencyCheck(false),
								crawlerLock(300),
								crawlerLogging(Config::crawlerLoggingDefault),
								crawlerReCrawl(false),
								crawlerReCrawlStart(true),
								crawlerReTries(-1),
								crawlerRetryArchive(true),
								crawlerSleepError(5000),
								crawlerSleepHttp(0),
								crawlerSleepIdle(1000),
								crawlerSleepMySql(20),
								crawlerStart("/"),
								crawlerTiming(false),
								crawlerUrlCaseSensitive(true),
								crawlerUrlChunks(5000),
								crawlerUrlDebug(false),
								crawlerUrlStartupCheck(true),
								crawlerWarningsFile(false),
								crawlerXml(false),
								customCountersGlobal(true),
								customReCrawl(true)	{
		this->crawlerArchivesNames.emplace_back("archives.org");
		this->crawlerArchivesUrlsMemento.emplace_back("http://web.archive.org/web/");
		this->crawlerArchivesUrlsTimemap.emplace_back("http://web.archive.org/web/timemap/link/");
		this->crawlerRetryHttp.push_back(502);
		this->crawlerRetryHttp.push_back(503);
		this->crawlerRetryHttp.push_back(504);
	}

	// destructor stub
	inline Config::~Config() {}

	// load crawling-specific configuration from parsed JSON document
	inline void Config::loadModule(const rapidjson::Document& jsonDocument, std::queue<std::string>& warningsTo) {
		// set logging queue
		this->setLog(warningsTo);

		// parse. configuration entries
		for(auto entry = jsonDocument.Begin(); entry != jsonDocument.End(); entry++) {
			this->begin(entry);

			this->category("crawler");
			this->option("archives", this->crawlerArchives);
			this->option("archives.names", this->crawlerArchivesNames);
			this->option("archives.urls.memento", this->crawlerArchivesUrlsMemento);
			this->option("archives.urls.timemap", this->crawlerArchivesUrlsTimemap);
			this->option("html.canonical.check", this->crawlerHTMLCanonicalCheck);
			this->option("html.consistency.check", this->crawlerHTMLConsistencyCheck);
			this->option("lock", this->crawlerLock);
			this->option("logging", this->crawlerLogging);
			this->option("params.blacklist", this->crawlerParamsBlackList);
			this->option("params.whitelist", this->crawlerParamsWhiteList);
			this->option("queries.blacklist.content", this->crawlerQueriesBlackListContent);
			this->option("queries.blacklist.types", this->crawlerQueriesBlackListTypes);
			this->option("queries.blacklist.urls", this->crawlerQueriesBlackListUrls);
			this->option("queries.links", this->crawlerQueriesLinks);
			this->option("queries.whitelist.content", this->crawlerQueriesWhiteListContent);
			this->option("queries.whitelist.types", this->crawlerQueriesWhiteListTypes);
			this->option("queries.whitelist.urls", this->crawlerQueriesWhiteListUrls);
			this->option("recrawl", this->crawlerReCrawl);
			this->option("recrawl.always", this->crawlerReCrawlAlways);
			this->option("recrawl.start", this->crawlerReCrawlStart);
			this->option("retries", this->crawlerReTries);
			this->option("retry.archive", this->crawlerRetryArchive);
			this->option("retry.http", this->crawlerRetryHttp);
			this->option("sleep.error", this->crawlerSleepError);
			this->option("sleep.http", this->crawlerSleepHttp);
			this->option("sleep.idle", this->crawlerSleepIdle);
			this->option("sleep.mysql", this->crawlerSleepMySql);
			this->option("start", this->crawlerStart, StringParsingOption::SubURL);
			this->option("timing", this->crawlerTiming);
			this->option("url.case.sensitive", this->crawlerUrlCaseSensitive);
			this->option("url.chunks", this->crawlerUrlChunks);
			this->option("url.debug", this->crawlerUrlDebug);
			this->option("url.startup.check", this->crawlerUrlStartupCheck);
			this->option("xml", this->crawlerXml);
			this->option("warnings.file", this->crawlerWarningsFile);

			this->category("custom");
			this->option("counters", this->customCounters);
			this->option("counters.end", this->customCountersEnd);
			this->option("counters.global", this->customCountersGlobal);
			this->option("counters.start", this->customCountersStart);
			this->option("counters.step", this->customCountersStep);
			this->option("recrawl", this->customReCrawl);
			this->option("urls", this->customUrls, StringParsingOption::SubURL);

			this->category("network");
			this->network.parse(*this);

			this->end();
		}

		// check properties of archives
		unsigned long completeArchives = std::min({ // number of complete archives (= minimum size of all arrays)
				this->crawlerArchivesNames.size(),
				this->crawlerArchivesUrlsMemento.size(),
				this->crawlerArchivesUrlsTimemap.size()
		});
		bool incompleteArchives = false;
		if(this->crawlerArchivesNames.size() > completeArchives) {
			// remove names of incomplete archives
			this->crawlerArchivesNames.resize(completeArchives);
			incompleteArchives = true;
		}
		if(this->crawlerArchivesUrlsMemento.size() > completeArchives) {
			// remove memento URL templates of incomplete archives
			this->crawlerArchivesUrlsMemento.resize(completeArchives);
			incompleteArchives = true;
		}
		if(this->crawlerArchivesUrlsTimemap.size() > completeArchives) {
			// remove timemap URL templates of incomplete archives
			this->crawlerArchivesUrlsTimemap.resize(completeArchives);
			incompleteArchives = true;
		}
		if(incompleteArchives) {
			// warn about incomplete counters
			warningsTo.emplace("\'archives.names\', \'.urls.memento\' and \'.urls.timemap\'"
					" should have the same number of elements.");
			warningsTo.emplace("Incomplete archive(s) removed.");
		}

		// check properties of counters
		unsigned long completeCounters = std::min({ // number of complete counters (= minimum size of all arrays)
				this->customCounters.size(),
				this->customCountersStart.size(),
				this->customCountersEnd.size(),
				this->customCountersStep.size()
		});
		bool incompleteCounters = false;
		if(this->customCounters.size() > completeCounters) {
			// remove counter variables of incomplete counters
			this->customCounters.resize(completeCounters);
			incompleteCounters = true;
		}
		if(this->customCountersStart.size() > completeCounters) {
			// remove starting values of incomplete counters
			this->customCountersStart.resize(completeCounters);
			incompleteCounters = true;
		}
		if(this->customCountersEnd.size() > completeCounters) {
			// remove ending values of incomplete counters
			this->customCountersEnd.resize(completeCounters);
			incompleteCounters = true;
		}
		if(this->customCountersStep.size() > completeCounters) {
			// remove step values of incomplete counters
			this->customCountersStep.resize(completeCounters);
			incompleteCounters = true;
		}
		if(incompleteCounters) {
			// warn about incomplete counters
			warningsTo.emplace("\'custom.counters\', \'.start\', \'.end\' and \'.step\'"
					" should have the same number of elements.");
			warningsTo.emplace("Incomplete counter(s) removed.");
		}

		// check validity of counters (infinite counters are invalid, therefore the need to check for counter termination)
		for(unsigned long n = 1; n <= this->customCounters.size(); n++) {
			unsigned long i = n - 1;
			if((this->customCountersStep.at(i) <= 0 && this->customCountersStart.at(i) < this->customCountersEnd.at(i))
					|| (this->customCountersStep.at(i) >= 0 && this->customCountersStart.at(i) > this->customCountersEnd.at(i))) {
				this->customCounters.erase(this->customCounters.begin() + i);
				this->customCountersStart.erase(this->customCountersStart.begin() + i);
				this->customCountersEnd.erase(this->customCountersEnd.begin() + i);
				this->customCountersStep.erase(this->customCountersStep.begin() + i);
				std::ostringstream warningStrStr;
				warningStrStr << "Counter loop of counter #" << (n + 1) << " would be infinitive, counter removed.";
				warningsTo.emplace(warningStrStr.str());
				n--;
			}
		}
	}

} /* crawlservpp::Module::Crawler */

#endif /* MODULE_CRAWLER_CONFIG_HPP_ */
