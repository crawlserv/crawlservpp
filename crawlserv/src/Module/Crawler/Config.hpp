/*
 * Config.hpp
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

	class Config : public Network::Config {
		// for convenience
		typedef Struct::ConfigItem ConfigItem;

	public:
		Config() {}
		virtual ~Config() {}

		// configuration constants
		static const unsigned short crawlerLoggingSilent = 0;
		static const unsigned short crawlerLoggingDefault = 1;
		static const unsigned short crawlerLoggingExtended = 2;
		static const unsigned short crawlerLoggingVerbose = 3;

		// configuration entries
		struct Entries {
			// constructor with default values
			Entries();

			// crawler entries
			bool crawlerArchives;
			std::vector<std::string> crawlerArchivesNames;
			std::vector<std::string> crawlerArchivesUrlsMemento;
			std::vector<std::string> crawlerArchivesUrlsTimemap;
			bool crawlerHTMLCanonicalCheck;
			bool crawlerHTMLConsistencyCheck;
			unsigned int crawlerLock;
			unsigned short crawlerLogging;
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
			bool crawlerUrlSelectionLock;
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
		} config;

	protected:
		// crawling-specific configuration parsing
		void parseOption() override;
		void checkOptions() override;

	private:

	};

	/*
	 * IMPLEMENTATION
	 */

	// configuration constructor: set default values
	inline Config::Entries::Entries() : crawlerArchives(false),
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
										crawlerSleepIdle(5000),
										crawlerSleepMySql(20),
										crawlerStart("/"),
										crawlerTiming(false),
										crawlerUrlCaseSensitive(true),
										crawlerUrlChunks(5000),
										crawlerUrlDebug(false),
										crawlerUrlSelectionLock(false),
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

	// parse crawling-specific configuration option
	inline void Config::parseOption() {
		// parse network option
		this->Network::Config::parseOption();

		// crawler options
		this->category("crawler");
		this->option("archives", this->config.crawlerArchives);
		this->option("archives.names", this->config.crawlerArchivesNames);
		this->option("archives.urls.memento", this->config.crawlerArchivesUrlsMemento);
		this->option("archives.urls.timemap", this->config.crawlerArchivesUrlsTimemap);
		this->option("html.canonical.check", this->config.crawlerHTMLCanonicalCheck);
		this->option("html.consistency.check", this->config.crawlerHTMLConsistencyCheck);
		this->option("lock", this->config.crawlerLock);
		this->option("logging", this->config.crawlerLogging);
		this->option("params.blacklist", this->config.crawlerParamsBlackList);
		this->option("params.whitelist", this->config.crawlerParamsWhiteList);
		this->option("queries.blacklist.content", this->config.crawlerQueriesBlackListContent);
		this->option("queries.blacklist.types", this->config.crawlerQueriesBlackListTypes);
		this->option("queries.blacklist.urls", this->config.crawlerQueriesBlackListUrls);
		this->option("queries.links", this->config.crawlerQueriesLinks);
		this->option("queries.whitelist.content", this->config.crawlerQueriesWhiteListContent);
		this->option("queries.whitelist.types", this->config.crawlerQueriesWhiteListTypes);
		this->option("queries.whitelist.urls", this->config.crawlerQueriesWhiteListUrls);
		this->option("recrawl", this->config.crawlerReCrawl);
		this->option("recrawl.always", this->config.crawlerReCrawlAlways);
		this->option("recrawl.start", this->config.crawlerReCrawlStart);
		this->option("retries", this->config.crawlerReTries);
		this->option("retry.archive", this->config.crawlerRetryArchive);
		this->option("retry.http", this->config.crawlerRetryHttp);
		this->option("sleep.error", this->config.crawlerSleepError);
		this->option("sleep.http", this->config.crawlerSleepHttp);
		this->option("sleep.idle", this->config.crawlerSleepIdle);
		this->option("sleep.mysql", this->config.crawlerSleepMySql);
		this->option("start", this->config.crawlerStart, StringParsingOption::SubURL);
		this->option("timing", this->config.crawlerTiming);
		this->option("url.case.sensitive", this->config.crawlerUrlCaseSensitive);
		this->option("url.chunks", this->config.crawlerUrlChunks);
		this->option("url.debug", this->config.crawlerUrlDebug);
		this->option("url.selection.lock", this->config.crawlerUrlSelectionLock);
		this->option("url.startup.check", this->config.crawlerUrlStartupCheck);
		this->option("xml", this->config.crawlerXml);
		this->option("warnings.file", this->config.crawlerWarningsFile);

		// custom URL options
		this->category("custom");
		this->option("counters", this->config.customCounters);
		this->option("counters.end", this->config.customCountersEnd);
		this->option("counters.global", this->config.customCountersGlobal);
		this->option("counters.start", this->config.customCountersStart);
		this->option("counters.step", this->config.customCountersStep);
		this->option("recrawl", this->config.customReCrawl);
		this->option("urls", this->config.customUrls, StringParsingOption::SubURL);
	}

	// check parsing-specific configuration
	inline void Config::checkOptions() {
		// check for link extraction query
		if(this->config.crawlerQueriesLinks.empty())
			throw Exception("Crawler::Config::checkOptions(): No link extraction query specified");

		// check properties of archives
		unsigned long completeArchives = std::min({ // number of complete archives (= minimum size of all arrays)
				this->config.crawlerArchivesNames.size(),
				this->config.crawlerArchivesUrlsMemento.size(),
				this->config.crawlerArchivesUrlsTimemap.size()
		});
		bool incompleteArchives = false;
		if(this->config.crawlerArchivesNames.size() > completeArchives) {
			// remove names of incomplete archives
			this->config.crawlerArchivesNames.resize(completeArchives);
			incompleteArchives = true;
		}
		if(this->config.crawlerArchivesUrlsMemento.size() > completeArchives) {
			// remove memento URL templates of incomplete archives
			this->config.crawlerArchivesUrlsMemento.resize(completeArchives);
			incompleteArchives = true;
		}
		if(this->config.crawlerArchivesUrlsTimemap.size() > completeArchives) {
			// remove timemap URL templates of incomplete archives
			this->config.crawlerArchivesUrlsTimemap.resize(completeArchives);
			incompleteArchives = true;
		}
		if(incompleteArchives) {
			// warn about incomplete counters
			this->warning(
					"\'archives.names\', \'.urls.memento\' and \'.urls.timemap\'"
					" should have the same number of elements."
			);
			this->warning("Incomplete archive(s) removed.");
		}

		// check properties of counters
		unsigned long completeCounters = std::min({ // number of complete counters (= minimum size of all arrays)
				this->config.customCounters.size(),
				this->config.customCountersStart.size(),
				this->config.customCountersEnd.size(),
				this->config.customCountersStep.size()
		});
		bool incompleteCounters = false;
		if(this->config.customCounters.size() > completeCounters) {
			// remove counter variables of incomplete counters
			this->config.customCounters.resize(completeCounters);
			incompleteCounters = true;
		}
		if(this->config.customCountersStart.size() > completeCounters) {
			// remove starting values of incomplete counters
			this->config.customCountersStart.resize(completeCounters);
			incompleteCounters = true;
		}
		if(this->config.customCountersEnd.size() > completeCounters) {
			// remove ending values of incomplete counters
			this->config.customCountersEnd.resize(completeCounters);
			incompleteCounters = true;
		}
		if(this->config.customCountersStep.size() > completeCounters) {
			// remove step values of incomplete counters
			this->config.customCountersStep.resize(completeCounters);
			incompleteCounters = true;
		}
		if(incompleteCounters) {
			// warn about incomplete counters
			this->warning(
					"\'custom.counters\', \'.start\', \'.end\' and \'.step\'"
					" should have the same number of elements."
			);
			this->warning("Incomplete counter(s) removed.");
		}

		// check validity of counters (infinite counters are invalid, therefore the need to check for counter termination)
		for(unsigned long n = 1; n <= this->config.customCounters.size(); n++) {
			unsigned long i = n - 1;
			if((this->config.customCountersStep.at(i) <= 0
					&& this->config.customCountersStart.at(i) < this->config.customCountersEnd.at(i))
					|| (this->config.customCountersStep.at(i) >= 0
							&& this->config.customCountersStart.at(i) > this->config.customCountersEnd.at(i))) {
				this->config.customCounters.erase(this->config.customCounters.begin() + i);
				this->config.customCountersStart.erase(this->config.customCountersStart.begin() + i);
				this->config.customCountersEnd.erase(this->config.customCountersEnd.begin() + i);
				this->config.customCountersStep.erase(this->config.customCountersStep.begin() + i);

				std::ostringstream warningStrStr;
				warningStrStr << "Counter loop of counter #" << (n + 1) << " would be infinitive, counter removed.";
				this->warning(warningStrStr.str());
				n--;
			}
		}
	}

} /* crawlservpp::Module::Crawler */

#endif /* MODULE_CRAWLER_CONFIG_HPP_ */
