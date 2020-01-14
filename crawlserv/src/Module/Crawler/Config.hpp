/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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

#include "../../Main/Exception.hpp"
#include "../../Network/Config.hpp"

#include <algorithm>	// std::min
#include <string>		// std::string
#include <vector>		// std::vector

namespace crawlservpp::Module::Crawler {

	/*
	 * DECLARATION
	 */

	class Config : protected Network::Config {
	public:
		Config() : crossDomain(false) {}
		virtual ~Config() {}

		// setter
		void setCrossDomain(bool isCrossDomain);

		// configuration constants
		static const unsigned char crawlerLoggingSilent = 0;
		static const unsigned char crawlerLoggingDefault = 1;
		static const unsigned char crawlerLoggingExtended = 2;
		static const unsigned char crawlerLoggingVerbose = 3;

		static const unsigned char redirectSourceUrl = 0;
		static const unsigned char redirectSourceContent = 1;

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
			unsigned char crawlerLogging;
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
			bool crawlerRepairComments;
			long crawlerReTries;
			bool crawlerRetryArchive;
			std::vector<unsigned int> crawlerRetryHttp;
			unsigned long crawlerSleepError;
			unsigned long crawlerSleepHttp;
			unsigned long crawlerSleepIdle;
			unsigned long crawlerSleepMySql;
			std::string crawlerStart;
			bool crawlerStartIgnore;
			unsigned int crawlerTidyErrors;
			bool crawlerTidyWarnings;
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
			std::vector<std::string> customTokensCookies;
			std::vector<unsigned int> customTokensKeep;
			std::vector<unsigned long> customTokensQuery;
			std::vector<std::string> customTokensSource;
			std::vector<std::string> customTokenHeaders;
			std::vector<bool> customTokensUsePost;
			std::vector<std::string> customUrls;
			bool customUsePost;

			// dynamic redirect
			std::string redirectCookies;
			std::vector<std::string> redirectHeaders;
			unsigned long redirectQueryUrl;
			unsigned long redirectQueryContent;
			std::string redirectTo;
			bool redirectUsePost;
			std::vector<std::string> redirectVarNames;
			std::vector<unsigned long> redirectVarQueries;
			std::vector<unsigned char> redirectVarSources;

			// expected number of results
			unsigned long expectedQuery;
			bool expectedErrorIfLarger;
			bool expectedErrorIfSmaller;
		} config;

		// class for Crawler::Config exceptions
		MAIN_EXCEPTION_CLASS();

	protected:
		// parsing of crawling-specific configuration
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
										crawlerRepairComments(true),
										crawlerReTries(720),
										crawlerRetryArchive(true),
										crawlerSleepError(10000),
										crawlerSleepHttp(0),
										crawlerSleepIdle(5000),
										crawlerSleepMySql(20),
										crawlerStart("/"),
										crawlerStartIgnore(false),
										crawlerTidyErrors(0),
										crawlerTidyWarnings(false),
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
										customTokensKeep(0),
										customUsePost(false),
										redirectQueryUrl(0),
										redirectQueryContent(0),
										redirectUsePost(false),
										expectedQuery(0),
										expectedErrorIfLarger(false),
										expectedErrorIfSmaller(false) {
		this->crawlerArchivesNames.emplace_back("archives.org");
		this->crawlerArchivesUrlsMemento.emplace_back("http://web.archive.org/web/");
		this->crawlerArchivesUrlsTimemap.emplace_back("http://web.archive.org/web/timemap/link/");

		this->crawlerRetryHttp.push_back(429);
		this->crawlerRetryHttp.push_back(502);
		this->crawlerRetryHttp.push_back(503);
		this->crawlerRetryHttp.push_back(504);
		this->crawlerRetryHttp.push_back(521);
		this->crawlerRetryHttp.push_back(522);
		this->crawlerRetryHttp.push_back(524);
	}

	// parse crawling-specific configuration option
	inline void Config::parseOption() {
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
		this->option("repair.comments", this->config.crawlerRepairComments);
		this->option("retries", this->config.crawlerReTries);
		this->option("retry.archive", this->config.crawlerRetryArchive);
		this->option("retry.http", this->config.crawlerRetryHttp);
		this->option("sleep.error", this->config.crawlerSleepError);
		this->option("sleep.http", this->config.crawlerSleepHttp);
		this->option("sleep.idle", this->config.crawlerSleepIdle);
		this->option("sleep.mysql", this->config.crawlerSleepMySql);
		this->option("start", this->config.crawlerStart, this->crossDomain ? StringParsingOption::URL : StringParsingOption::SubURL);
		this->option("start.ignore", this->config.crawlerStartIgnore);
		this->option("tidy.errors", this->config.crawlerTidyErrors);
		this->option("tidy.warnings", this->config.crawlerTidyWarnings);
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
		this->option("tokens.cookies", this->config.customTokensCookies);
		this->option("tokens.keep", this->config.customTokensKeep);
		this->option("tokens.query", this->config.customTokensQuery);
		this->option("tokens.source", this->config.customTokensSource);
		this->option("tokens.use.post", this->config.customTokensUsePost);
		this->option("token.headers", this->config.customTokenHeaders);	// NOTE: to be used for ALL tokens
		this->option("urls", this->config.customUrls, this->crossDomain ? StringParsingOption::URL : StringParsingOption::SubURL);
		this->option("use.post", this->config.customUsePost);

		// dynamic redirect
		this->category("redirect");
		this->option("cookies", this->config.redirectCookies);
		this->option("headers", this->config.redirectHeaders);
		this->option("query.content", this->config.redirectQueryContent);
		this->option("query.url", this->config.redirectQueryUrl);
		this->option("to", this->config.redirectTo);
		this->option("use.post", this->config.redirectUsePost);
		this->option("var.names", this->config.redirectVarNames);
		this->option("var.queries", this->config.redirectVarQueries);
		this->option("var.sources", this->config.redirectVarSources);

		// expected number of results
		this->category("expected");
		this->option("query", this->config.expectedQuery);
		this->option("error.if.larger", this->config.expectedErrorIfLarger);
		this->option("error.if.smaller", this->config.expectedErrorIfSmaller);
	}

	// check parsing-specific configuration, throws Config::Exception
	inline void Config::checkOptions() {
		// check for link extraction query
		if(this->config.crawlerQueriesLinks.empty())
			throw Exception("Crawler::Config::checkOptions(): No link extraction query specified");

		// check properties of archives
		bool incompleteArchives = false;

		const unsigned long completeArchives = std::min({ // number of complete archives (= minimum size of all arrays)
				this->config.crawlerArchivesNames.size(),
				this->config.crawlerArchivesUrlsMemento.size(),
				this->config.crawlerArchivesUrlsTimemap.size()
		});

		// remove names that are not used
		if(this->config.crawlerArchivesNames.size() > completeArchives) {
			this->config.crawlerArchivesNames.resize(completeArchives);

			incompleteArchives = true;
		}

		// remove memento URL templates that are not used
		if(this->config.crawlerArchivesUrlsMemento.size() > completeArchives) {
			this->config.crawlerArchivesUrlsMemento.resize(completeArchives);

			incompleteArchives = true;
		}

		// remove timemap URL templates that are not used
		if(this->config.crawlerArchivesUrlsTimemap.size() > completeArchives) {
			this->config.crawlerArchivesUrlsTimemap.resize(completeArchives);

			incompleteArchives = true;
		}

		// warn about incomplete archives
		if(incompleteArchives) {
			this->warning(
					"\'archives.names\', \'.urls.memento\' and \'.urls.timemap\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete archive(s) removed from configuration.");
		}

		// check properties of counters
		bool incompleteCounters = false;

		const unsigned long completeCounters = std::min({ // number of complete counters (= minimum size of arrays)
				this->config.customCounters.size(),
				this->config.customCountersStart.size(),
				this->config.customCountersEnd.size()
		});

		// remove counter variable names that are not used
		if(this->config.customCounters.size() > completeCounters) {
			// remove counter variables of incomplete counters
			this->config.customCounters.resize(completeCounters);

			incompleteCounters = true;
		}

		// remove starting values that are not used
		if(this->config.customCountersStart.size() > completeCounters) {
			this->config.customCountersStart.resize(completeCounters);

			incompleteCounters = true;
		}

		// remove ending values that are not used
		if(this->config.customCountersEnd.size() > completeCounters) {
			this->config.customCountersEnd.resize(completeCounters);

			incompleteCounters = true;
		}

		// warn about incomplete counters
		if(incompleteCounters) {
			this->warning(
					"\'custom.counters\', \'.start\', \'.end\' and \'.step\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete counter(s) removed from configuration.");

			incompleteCounters = false;
		}

		// remove step values that are not used, add one as step value where none is specified
		if(this->config.customCountersStep.size() > completeCounters)
			incompleteCounters = true;

		this->config.customCountersStep.resize(completeCounters, 1);

		// remove aliases that are not used, add empty aliases where none exist
		if(this->config.customCountersAlias.size() > completeCounters)
			incompleteCounters = true;

		this->config.customCountersAlias.resize(completeCounters);

		// remove alias summands that are not used, add zero as summand where none is specified
		if(this->config.customCountersAliasAdd.size() > completeCounters)
			incompleteCounters = true;

		this->config.customCountersAliasAdd.resize(completeCounters, 0);

		// warn about unused properties
		if(incompleteCounters)
			this->warning("Unused counter properties removed from configuration.");

		// check validity of counters (infinite counters are invalid, therefore the need to check for counter termination)
		for(unsigned long n = 1; n <= this->config.customCounters.size(); ++n) {
			const unsigned long i = n - 1;

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
				const std::string counterName(this->config.customCounters.at(i));

				// delete the invalid counter
				this->config.customCounters.erase(this->config.customCounters.begin() + i);
				this->config.customCountersStart.erase(this->config.customCountersStart.begin() + i);
				this->config.customCountersEnd.erase(this->config.customCountersEnd.begin() + i);
				this->config.customCountersStep.erase(this->config.customCountersStep.begin() + i);
				this->config.customCountersAlias.erase(this->config.customCountersAlias.begin() + i);
				this->config.customCountersAliasAdd.erase(this->config.customCountersAliasAdd.begin() + i);

				--n;

				this->warning(
						"Loop of counter \'"
						+ counterName
						+ "\' would be infinite, counter removed."
				);
			}
		}

		// check properties of tokens
		bool incompleteTokens = false;

		const unsigned long completeTokens = std::min({ // number of complete tokens (= minimum size of arrays)
				this->config.customTokens.size(),
				this->config.customTokensSource.size(),
				this->config.customTokensQuery.size()
		});

		// remove token variable names that are not used
		if(this->config.customTokens.size() > completeTokens) {
			this->config.customTokens.resize(completeTokens);

			incompleteTokens = true;
		}

		// remove token sources that are not used
		if(this->config.customTokensSource.size() > completeTokens) {
			this->config.customTokensSource.resize(completeTokens);

			incompleteTokens = true;
		}

		// remove token queries that are not used
		if(this->config.customTokensQuery.size() > completeTokens) {
			this->config.customTokensQuery.resize(completeTokens);

			incompleteTokens = true;
		}

		// warn about incomplete counters
		if(incompleteTokens) {
			this->warning(
					"\'custom.tokens\', \'.tokens.source\' and \'.tokens.query\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete token(s) removed from configuration.");

			incompleteTokens = false;
		}

		// remove cookie headers that are not used, set to empty string where none is specified
		if(this->config.customTokensCookies.size() > completeTokens)
			incompleteTokens = true;

		this->config.customTokensCookies.resize(completeTokens);

		// remove token expiration times that are not used, set to '0' where none is specified
		if(this->config.customTokensKeep.size() > completeTokens)
			incompleteTokens = true;

		this->config.customTokensKeep.resize(completeTokens, 0);

		// remove token POST options that are not used, set to 'false' where none is specified
		if(this->config.customTokensUsePost.size() > completeTokens)
			incompleteTokens = true;

		this->config.customTokensUsePost.resize(completeTokens, false);

		// warn about unused property
		if(incompleteTokens)
			this->warning("Unused token properties removed from configuration.");

		// check properties of variables for dynamic redirect
		bool incompleteVars = false;

		const unsigned long completeVars = std::min({ // number of complete variables (= minimum size of all arrays)
			this->config.redirectVarNames.size(),
			this->config.redirectVarQueries.size(),
			this->config.redirectVarSources.size()
		});

		// remove redirect variable names that are not used
		if(this->config.redirectVarNames.size() > completeVars) {
			this->config.redirectVarNames.resize(completeVars);

			incompleteVars = true;
		}

		// remove redirect queries that are not used
		if(this->config.redirectVarQueries.size() > completeVars) {
			this->config.redirectVarQueries.resize(completeVars);

			incompleteVars = true;
		}

		// remove redirect sources that are not used
		if(this->config.redirectVarSources.size() > completeVars) {
			this->config.redirectVarSources.resize(completeVars);

			incompleteVars = true;
		}

		// warn about incomplete counters
		if(incompleteVars) {
			this->warning(
					"\'redirect.var.names\', \'.var.sources\' and \'.var.queries\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete variable(s) removed form configuration.");
		}
	}

} /* crawlservpp::Module::Crawler */

#endif /* MODULE_CRAWLER_CONFIG_HPP_ */
