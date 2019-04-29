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
		Config() : crossDomain(false) {}
		virtual ~Config() {}

		// setter
		void setCrossDomain(bool isCrossDomain);

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
			unsigned int crawlerLock;
			unsigned short crawlerLogging;
			std::vector<std::string> crawlerParamsAdd;
			std::vector<std::string> crawlerParamsBlackList;
			std::vector<std::string> crawlerParamsWhiteList;
			std::vector<unsigned long> crawlerQueriesBlackListContent;
			std::vector<unsigned long> crawlerQueriesBlackListTypes;
			std::vector<unsigned long> crawlerQueriesBlackListUrls;
			std::vector<unsigned long> crawlerQueriesLinks;
			std::vector<unsigned long> crawlerQueriesLinksBlackListContent;
			std::vector<unsigned long> crawlerQueriesLinksBlackListTypes;
			std::vector<unsigned long> crawlerQueriesLinksBlackListUrls;
			std::vector<unsigned long> crawlerQueriesLinksWhiteListContent;
			std::vector<unsigned long> crawlerQueriesLinksWhiteListTypes;
			std::vector<unsigned long> crawlerQueriesLinksWhiteListUrls;
			std::vector<unsigned long> crawlerQueriesWhiteListContent;
			std::vector<unsigned long> crawlerQueriesWhiteListTypes;
			std::vector<unsigned long> crawlerQueriesWhiteListUrls;
			bool crawlerReCrawl;
			std::vector<std::string> crawlerReCrawlAlways;
			bool crawlerReCrawlStart;
			bool crawlerRepairCData;
			long crawlerReTries;
			bool crawlerRetryArchive;
			std::vector<unsigned int> crawlerRetryHttp;
			unsigned long crawlerSleepError;
			unsigned long crawlerSleepHttp;
			unsigned long crawlerSleepIdle;
			unsigned long crawlerSleepMySql;
			std::string crawlerStart;
			bool crawlerStartIgnore;
			bool crawlerTiming;
			bool crawlerUrlCaseSensitive;
			unsigned long crawlerUrlChunks;
			bool crawlerUrlDebug;
			unsigned short crawlerUrlMaxLength;
			bool crawlerUrlStartupCheck;
			bool crawlerWarningsFile;
			bool crawlerXml;

			// custom entries
			std::vector<std::string> customCounters;
			std::vector<std::string> customCountersAlias;
			std::vector<unsigned long> customCountersAliasAdd;
			std::vector<long> customCountersEnd;
			bool customCountersGlobal;
			std::vector<long> customCountersStart;
			std::vector<long> customCountersStep;
			bool customReCrawl;
			bool customRobots;
			std::vector<std::string> customTokens;
			std::vector<unsigned long> customTokensQuery;
			std::vector<std::string> customTokensSource;
			std::vector<std::string> customUrls;
			bool customUsePost;

			// expected number of results
			unsigned long expectedQuery;
			bool expectedErrorIfLarger;
			bool expectedErrorIfSmaller;
		} config;

	protected:
		// crawling-specific configuration parsing
		void parseOption() override;
		void checkOptions() override;

	private:
		bool crossDomain;
	};

	/*
	 * IMPLEMENTATION
	 */

	// set whether website is cross-domain
	inline void Config::setCrossDomain(bool isCrossDomain) {
		this->crossDomain = isCrossDomain;
	}

	// configuration constructor: set default values
	inline Config::Entries::Entries() : crawlerArchives(false),
										crawlerLock(300),
										crawlerLogging(Config::crawlerLoggingDefault),
										crawlerReCrawl(false),
										crawlerReCrawlStart(true),
										crawlerRepairCData(true),
										crawlerReTries(720),
										crawlerRetryArchive(true),
										crawlerSleepError(5000),
										crawlerSleepHttp(0),
										crawlerSleepIdle(5000),
										crawlerSleepMySql(20),
										crawlerStart("/"),
										crawlerStartIgnore(false),
										crawlerTiming(false),
										crawlerUrlCaseSensitive(true),
										crawlerUrlChunks(5000),
										crawlerUrlDebug(false),
										crawlerUrlMaxLength(2000),
										crawlerUrlStartupCheck(true),
										crawlerWarningsFile(false),
										crawlerXml(false),
										customCountersGlobal(true),
										customReCrawl(true),
										customRobots(false),
										customUsePost(false),
										expectedQuery(0),
										expectedErrorIfLarger(false),
										expectedErrorIfSmaller(false) {
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
		this->option("lock", this->config.crawlerLock);
		this->option("logging", this->config.crawlerLogging);
		this->option("params.add", this->config.crawlerParamsAdd);
		this->option("params.blacklist", this->config.crawlerParamsBlackList);
		this->option("params.whitelist", this->config.crawlerParamsWhiteList);
		this->option("queries.blacklist.cont", this->config.crawlerQueriesBlackListContent);
		this->option("queries.blacklist.types", this->config.crawlerQueriesBlackListTypes);
		this->option("queries.blacklist.urls", this->config.crawlerQueriesBlackListUrls);
		this->option("queries.links", this->config.crawlerQueriesLinks);
		this->option("queries.links.blacklist.cont", this->config.crawlerQueriesLinksBlackListContent);
		this->option("queries.links.blacklist.types", this->config.crawlerQueriesLinksBlackListTypes);
		this->option("queries.links.blacklist.urls", this->config.crawlerQueriesLinksBlackListUrls);
		this->option("queries.links.whitelist.cont", this->config.crawlerQueriesLinksWhiteListContent);
		this->option("queries.links.whitelist.types", this->config.crawlerQueriesLinksWhiteListTypes);
		this->option("queries.links.whitelist.urls", this->config.crawlerQueriesLinksWhiteListUrls);
		this->option("queries.whitelist.cont", this->config.crawlerQueriesWhiteListContent);
		this->option("queries.whitelist.types", this->config.crawlerQueriesWhiteListTypes);
		this->option("queries.whitelist.urls", this->config.crawlerQueriesWhiteListUrls);
		this->option("recrawl", this->config.crawlerReCrawl);
		this->option("recrawl.always", this->config.crawlerReCrawlAlways);
		this->option("recrawl.start", this->config.crawlerReCrawlStart);
		this->option("repair.cdata", this->config.crawlerRepairCData);
		this->option("retries", this->config.crawlerReTries);
		this->option("retry.archive", this->config.crawlerRetryArchive);
		this->option("retry.http", this->config.crawlerRetryHttp);
		this->option("sleep.error", this->config.crawlerSleepError);
		this->option("sleep.http", this->config.crawlerSleepHttp);
		this->option("sleep.idle", this->config.crawlerSleepIdle);
		this->option("sleep.mysql", this->config.crawlerSleepMySql);
		this->option("start", this->config.crawlerStart, this->crossDomain ? StringParsingOption::URL : StringParsingOption::SubURL);
		this->option("start.ignore", this->config.crawlerStartIgnore);
		this->option("timing", this->config.crawlerTiming);
		this->option("url.case.sensitive", this->config.crawlerUrlCaseSensitive);
		this->option("url.chunks", this->config.crawlerUrlChunks);
		this->option("url.debug", this->config.crawlerUrlDebug);
		this->option("url.max.length", this->config.crawlerUrlMaxLength);
		this->option("url.startup.check", this->config.crawlerUrlStartupCheck);
		this->option("xml", this->config.crawlerXml);
		this->option("warnings.file", this->config.crawlerWarningsFile);

		// custom URL options
		this->category("custom");
		this->option("counters", this->config.customCounters);
		this->option("counters.alias", this->config.customCountersAlias);
		this->option("counters.alias.add", this->config.customCountersAliasAdd);
		this->option("counters.end", this->config.customCountersEnd);
		this->option("counters.global", this->config.customCountersGlobal);
		this->option("counters.start", this->config.customCountersStart);
		this->option("counters.step", this->config.customCountersStep);
		this->option("recrawl", this->config.customReCrawl);
		this->option("robots", this->config.customRobots);
		this->option("tokens", this->config.customTokens);
		this->option("tokens.query", this->config.customTokensQuery);
		this->option("tokens.source", this->config.customTokensSource);
		this->option("urls", this->config.customUrls, this->crossDomain ? StringParsingOption::URL : StringParsingOption::SubURL);
		this->option("use.post", this->config.customUsePost);

		// expected number of results
		this->category("expected");
		this->option("query", this->config.expectedQuery);
		this->option("error.if.larger", this->config.expectedErrorIfLarger);
		this->option("error.if.smaller", this->config.expectedErrorIfSmaller);
	}

	// check parsing-specific configuration
	inline void Config::checkOptions() {
		// check for link extraction query
		if(this->config.crawlerQueriesLinks.empty())
			throw Exception("Crawler::Config::checkOptions(): No link extraction query specified");

		// check properties of archives
		bool incompleteArchives = false;
		unsigned long completeArchives = std::min({ // number of complete archives (= minimum size of all arrays)
				this->config.crawlerArchivesNames.size(),
				this->config.crawlerArchivesUrlsMemento.size(),
				this->config.crawlerArchivesUrlsTimemap.size()
		});

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
			// warn about incomplete archives
			this->warning(
					"\'archives.names\', \'.urls.memento\' and \'.urls.timemap\'"
					" should have the same number of elements."
			);
			this->warning("Incomplete archive(s) removed.");
		}

		// check properties of counters
		bool incompleteCounters = false;
		unsigned long completeCounters = std::min({ // number of complete counters (= minimum size of all arrays)
				this->config.customCounters.size(),
				this->config.customCountersStart.size(),
				this->config.customCountersEnd.size(),
				this->config.customCountersStep.size()
		});

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
		for(unsigned long n = 1; n <= this->config.customCounters.size(); ++n) {
			unsigned long i = n - 1;
			if(
					(
							this->config.customCountersStep.at(i) <= 0
							&& this->config.customCountersStart.at(i) < this->config.customCountersEnd.at(i)
					)
					||
					(
							this->config.customCountersStep.at(i) >= 0
							&& this->config.customCountersStart.at(i) > this->config.customCountersEnd.at(i)
					)
			) {
				this->config.customCounters.erase(this->config.customCounters.begin() + i);
				this->config.customCountersStart.erase(this->config.customCountersStart.begin() + i);
				this->config.customCountersEnd.erase(this->config.customCountersEnd.begin() + i);
				this->config.customCountersStep.erase(this->config.customCountersStep.begin() + i);

				std::ostringstream warningStrStr;

				warningStrStr << "Counter loop of counter #" << (n + 1) << " would be infinitive, counter removed.";

				this->warning(warningStrStr.str());

				--n;
			}
		}

		// check properties of tokens
		bool incompleteTokens = false;
		unsigned long completeTokens = std::min({ // number of complete tokens (= minimum size of all arrays)
				this->config.customTokens.size(),
				this->config.customTokensSource.size(),
				this->config.customTokensQuery.size()
		});

		if(this->config.customTokens.size() > completeTokens) {
			// remove token variables of incomplete counters
			this->config.customTokens.resize(completeTokens);

			incompleteTokens = true;
		}

		if(this->config.customTokensSource.size() > completeTokens) {
			// remove sources of incomplete counters
			this->config.customTokensSource.resize(completeTokens);

			incompleteTokens = true;
		}

		if(this->config.customTokensQuery.size() > completeTokens) {
			// remove queries of incomplete counters
			this->config.customTokensQuery.resize(completeTokens);

			incompleteTokens = true;
		}

		// remove aliases that are not used, add empty aliases where none exist
		this->config.customCountersAlias.resize(completeTokens);

		// remove alias summands that are not used, add zero as summand where none is specified
		this->config.customCountersAliasAdd.resize(completeTokens, 0);

		if(incompleteTokens) {
			// warn about incomplete counters
			this->warning(
					"\'custom.tokens\', \'.tokens.source\' and \'.tokens.query\'"
					" should have the same number of elements."
			);
			this->warning("Incomplete token(s) removed.");
		}
	}

} /* crawlservpp::Module::Crawler */

#endif /* MODULE_CRAWLER_CONFIG_HPP_ */
