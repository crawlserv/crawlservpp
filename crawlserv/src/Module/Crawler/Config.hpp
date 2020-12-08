/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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
 *  Created on: Oct 25, 2018
 *      Author: ans
 */

#ifndef MODULE_CRAWLER_CONFIG_HPP_
#define MODULE_CRAWLER_CONFIG_HPP_

#include "../../Main/Exception.hpp"
#include "../../Network/Config.hpp"

#include <algorithm>	// std::min
#include <cstddef>		// std::size_t
#include <cstdint>		// std::int64_t, std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <string>		// std::string
#include <vector>		// std::vector

//! Namespace for crawler classes.
namespace crawlservpp::Module::Crawler {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Logging is disabled.
	inline constexpr std::uint8_t crawlerLoggingSilent{0};

	//! Default logging is enabled.
	inline constexpr std::uint8_t crawlerLoggingDefault{1};

	//! Extended logging is enabled.
	inline constexpr std::uint8_t crawlerLoggingExtended{2};

	//! Verbose logging is enabled.
	inline constexpr std::uint8_t crawlerLoggingVerbose{3};

	//! Performing a query on the URL of a crawled web page to determine whether to redirect.
	inline constexpr std::uint8_t redirectSourceUrl{0};

	//! Performing a query on the content of a crawled web page to determine whether to redirect.
	inline constexpr std::uint8_t redirectSourceContent{1};

	//! Default time to lock URLs that are being processed, in seconds.
	inline constexpr std::uint32_t defaultCrawlerLockS{300};

	//! Default number of URLs to be processed in one MySQL query.
	inline constexpr std::uint16_t defaultMaxBatchSize{500};

	//! Default number of re-tries on connection errors.
	inline constexpr std::int64_t defaultReTries{720};

	//! HTTP errors that will be handled like connection errors by default.
	inline constexpr std::array defaultRetryHttp{429, 502, 503, 504, 521, 522, 524};

	//! Default sleeping time on connection errors, in milliseconds.
	inline constexpr std::uint64_t defaultSleepErrorMs{10000};

	//! Default time that will be waited between HTTP requests, in milliseconds.
	inline constexpr std::uint64_t defaultSleepHttpMs{0};

	//! Default time that will be waited before checking for new URLs when all URLs have been crawled, in milliseconds.
	inline constexpr std::uint64_t defaultSleepIdleMs{5000};

	//! Default time to wait before the first try to re-connect to the MySQL server, in seconds.
	inline constexpr std::uint64_t defaultSleepMySqlS{20};

	//! Default number of crawled URLs to be processed at once without possible interruption.
	inline constexpr std::uint64_t defaultUrlChunks{5000};

	//! Default maximum length of URLs to add.
	inline constexpr std::uint16_t defaultUrlMaxLength{2000};

	///@}

	/*
	 * DECLARATION
	 */

	//! Configuration for crawlers.
	class Config : protected Network::Config {
	public:

		///@name Setter
		///@{

		void setCrossDomain(bool isCrossDomain);

		///@}

		///@name Configuration
		///@{

		//! Configuration entries for crawler threads.
		/*!
		 * \warning Changing the configuration requires
		 *   updating @c json/crawler.json in @c
		 *   crawlserv_frontend!
		 */
		struct Entries {
			///@name Crawler Configuration
			///@{

			//! Specifies whether to crawl archived pages.
			bool crawlerArchives{false};

			//! Names of archives to crawl.
			std::vector<std::string> crawlerArchivesNames{"archives.org"};

			//! Memento %URI templates for archives to crawl.
			/*!
			 * To be followed by @c YYYYMMDDHHMMSS/URI
			 *  (date and %URI) of the memento to crawl.
			 */
			std::vector<std::string> crawlerArchivesUrlsMemento{"http://web.archive.org/web/"};

			//! Memento URIs that will always be skipped.
			std::vector<std::string> crawlerArchivesUrlsSkip;

			//! Timemap %URI template for archives to crawl.
			/*!
			 * To be followed by the %URI of the page to
			 *  crawl.
			 */
			std::vector<std::string> crawlerArchivesUrlsTimemap{"http://web.archive.org/web/timemap/link/"};

			//! Time for which to lock URLs that are currently being processed, in seconds.
			std::uint32_t crawlerLock{defaultCrawlerLockS};

			//! Level of logging acivitiy.
			/*!
			 * \sa crawlerLoggingSilent, crawlerLoggingDefault
			 *   crawlerLoggingExtended, crawlerLoggingVerbose
			 */
			std::uint8_t crawlerLogging{crawlerLoggingDefault};

			//! Maximum number of URLs processed in one MySQL query.
			std::uint16_t crawlerMaxBatchSize{defaultMaxBatchSize};

			//! URL parameters that will be added shortly before retrieving the content.
			/*!
			 * \note These parameters will not be saved in
			 *   the URL list, i.e. in the database.
			 */
			std::vector<std::string> crawlerParamsAdd;

			//! Parameters in URLs that will be ignored.
			/*!
			 * \note This option will be ignored, if
			 *   Entries::crawlerParamsWhiteList is used.
			 */
			std::vector<std::string> crawlerParamsBlackList;

			//! Parameters in URLs that will not be ignored.
			/*!
			 * \note If this option is used,
			 *   Entries::crawlerParamsAdd will be ignored.
			 */
			std::vector<std::string> crawlerParamsWhiteList;

			//! Content matching one of these queries will not be crawled.
			/*!
			 * \note This option will be ignored, if
			 *   Entries::crawlerQueriesLinksWhiteListContent
			 *   is used.
			 */
			std::vector<std::uint64_t> crawlerQueriesBlackListContent;

			//! Content types matching one of these queries will not be crawled.
			/*!
			 * \note This option will be ignored, if
			 *   Entries::crawlerQueriesLinksWhiteListTypes
			 *   is used.
			 */
			std::vector<std::uint64_t> crawlerQueriesBlackListTypes;

			//! URLs matching one of these queries will not be crawled.
			/*!
			 * \note This option will be ignored, if
			 *   Entries::crawlerQueriesLinksWhiteListUrls
			 *   is used.
			 */
			std::vector<std::uint64_t> crawlerQueriesBlackListUrls;

			//! Queries on content to find URLs.
			std::vector<std::uint64_t> crawlerQueriesLinks;

			//! Content matching one of these queries will not be used for link extraction.
			/*!
			 * \note This option will be ignored, if
			 *   Entries::crawlerQueriesLinksWhiteListContent
			 *   is used.
			 */
			std::vector<std::uint64_t> crawlerQueriesLinksBlackListContent;

			//! Content types matching one of these queries will not be used for link extraction.
			/*!
			 * \note This option will be ignored, if
			 *   Entries::crawlerQueriesLinksWhiteListTypes
			 *   is used.
			 */
			std::vector<std::uint64_t> crawlerQueriesLinksBlackListTypes;

			//! URLs matching one of these queries will not be used for link extraction.
			/*!
			 * \note This option will be ignored, if
			 *   Entries::crawlerQueriesLinksWhiteListUrls
			 *   is used.
			 */
			std::vector<std::uint64_t> crawlerQueriesLinksBlackListUrls;

			//! If not empty, only content matching one of these queries will be used for link extraction.
			/*!
			 * \note If this option is used,
			 *   Entries::crawlerQueriesLinksBlackListContent
			 *   will be ignored.
			 */
			std::vector<std::uint64_t> crawlerQueriesLinksWhiteListContent;

			//! If not empty, only content types matching one of these queries will be used for link extraction.
			/*!
			 * \note If this option is used,
			 *   Entries::crawlerQueriesLinksBlackListTypes
			 *   will be ignored.
			 */
			std::vector<std::uint64_t> crawlerQueriesLinksWhiteListTypes;

			//! If not empty, only URLs matching one of these queries will be used for link extraction.
			/*!
			 * \note If this option is used,
			 *   Entries::crawlerQueriesLinksBlackListUrls
			 *   will be ignored.
			 */
			std::vector<std::uint64_t> crawlerQueriesLinksWhiteListUrls;

			//! If not empty, only content matching one of these queries will be crawled.
			/*!
			 * \note If this option is used,
			 *   Entries::crawlerQueriesBlackListContent
			 *   will be ignored.
			 */
			std::vector<std::uint64_t> crawlerQueriesWhiteListContent;

			//! If not empty, only content types matching one of these queries will be crawled.
			/*!
			 * \note If this option is used,
			 *   Entries::crawlerQueriesBlackListTypes
			 *   will be ignored.
			 */
			std::vector<std::uint64_t> crawlerQueriesWhiteListTypes;

			//! If not empty, only URLs matching one of these queries will be crawled.
			/*!
			 * \note If this option is used,
			 *   Entries::crawlerQueriesBlackListUrls
			 *   will be ignored.
			 */
			std::vector<std::uint64_t> crawlerQueriesWhiteListUrls;

			//! Specifies whether to re-crawl already crawled URLs.
			bool crawlerReCrawl{false};

			//! List of URLs that will always be re-crawled.
			std::vector<std::string> crawlerReCrawlAlways;

			//! Specifies whether to re-crawl the start page every time to keep the URL list up-to-date.
			bool crawlerReCrawlStart{true};

			//! Specifies whether to (try to) repair CData when parsing HTML/XML.
			bool crawlerRepairCData{true};

			//! Specifies whether to (try to) repair broken HTML/XML comments.
			bool crawlerRepairComments{true};

			//! Specifies whether to remove XML processing instructions (@c <?xml:...>) before parsing HTML content.
			bool crawlerRemoveXmlInstructions{true};

			//! Number of re-tries on connection errors (-1=infinite).
			std::int64_t crawlerReTries{defaultReTries};

			//! Specifies whether to re-try when retrieving the archived pages fails.
			bool crawlerRetryArchive{true};

			//! Specifies whether empty responses will be handled like connection errors.
			bool crawlerRetryEmpty{true};

			//! HTTP errors that will be handled like connection errors.
			std::vector<std::uint32_t> crawlerRetryHttp{defaultRetryHttp.cbegin(), defaultRetryHttp.cend()};

			//! Sleeping time on connection errors, in milliseconds.
			std::uint64_t crawlerSleepError{defaultSleepErrorMs};

			//! Time that will be waited between HTTP requests, in milliseconds
			std::uint64_t crawlerSleepHttp{defaultSleepHttpMs};

			//! Time that will be waited before checking for new URLs when all URLs have been crawled, in milliseconds.
			std::uint64_t crawlerSleepIdle{defaultSleepIdleMs};

			//! Time to wait before trying to re-connect to the MySQL server, in seconds.
			/*!
			 * \note After that time, the current
			 *   database operation will be lost.
			 */
			std::uint64_t crawlerSleepMySql{defaultSleepMySqlS};

			//! Starting point for crawling (should start with / except for cross-domain websites).
			std::string crawlerStart{"/"};

			//! Specifies whether to not crawl the start page.
			bool crawlerStartIgnore{false};

			//! Number  of @c tidyhtml errors to log (if logging is enabled).
			std::uint32_t crawlerTidyErrors{0};

			//! Specifies whether to log @c tidyhtml warnings (if logging is enabled).
			bool crawlerTidyWarnings{false};

			//! Specifies whether to calculate timing statistics for the crawler.
			bool crawlerTiming{false};

			//! Specifies whether URLs are case-sensitive.
			/*!
			 * \warning Changes invalidate the hashs of existing URLs!
			 */
			bool crawlerUrlCaseSensitive{true};

			//! Number of crawled URLs to be processed at once without possible interruption.
			std::uint64_t crawlerUrlChunks{defaultUrlChunks};

			//! Specifies whether to perform additional check for duplicates after URL insertion.
			/*!
			 * For debugging purposes only.
			 */
			bool crawlerUrlDebug{false};

			//! Maximum length of URLs to add.
			std::uint16_t crawlerUrlMaxLength{defaultUrlMaxLength};

			//! Specifies whether to check the URL list before starting to crawl.
			bool crawlerUrlStartupCheck{true};

			//! Specifies whether to warn when files are found (as opposed to folders).
			bool crawlerWarningsFile{false};

			//! Specifies whether to always save crawled content as cleaned XML.
			bool crawlerXml{false};

			///@}
			///@name Custom URLs
			///@{

			//! List of counter variables to be replaced in custom URLs.
			std::vector<std::string> customCounters;

			//! Alias for the counter variable with the same array index.
			std::vector<std::string> customCountersAlias;

			//! Value to add to the counter alias with the same array index.
			std::vector<std::uint64_t> customCountersAliasAdd;

			//! End value for the counter with the same array index.
			std::vector<std::int64_t> customCountersEnd;

			//! Specifies whether to use every counter for all custom URLs.
			/*!
			 * Otherwise a counters will only be used for URLs with the same array index.
			 */
			bool customCountersGlobal{true};

			//! Start value for the counter with the same array index.
			std::vector<std::int64_t> customCountersStart;

			//! Step value for the counter with the same array index.
			std::vector<std::int64_t> customCountersStep;

			//! Specifies whether to always re-crawl custom URLs.
			bool customReCrawl{true};

			//! Specifies whether to add the sitemaps specified in @c robots.txt as custom URLs.
			bool customRobots{false};

			//! Custom HTTP headers to be used for ALL tokens.
			std::vector<std::string> customTokenHeaders;

			//! List of token variables to be replaced in custom URLs.
			std::vector<std::string> customTokens;

			//! Custom HTTP @c Cookie header for the token with the same array index.
			std::vector<std::string> customTokensCookies;

			//! Time until the token with the same array index gets invalid, in seconds.
			std::vector<std::uint32_t> customTokensKeep;

			//! Query to extract the token with the same array index.
			std::vector<std::uint64_t> customTokensQuery;

			//! Determines whether an error occurs if the token with the same array index is empty.
			std::vector<bool> customTokensRequired;

			//! Source URL for the token with the same array index (absolute link without protocol).
			std::vector<std::string> customTokensSource;

			//! Use HTTP POST instead of GET for the token with the same array index.
			std::vector<bool> customTokensUsePost;

			//! Custom URLs for crawling (should all start with @c / except for cross-domain websites).
			std::vector<std::string> customUrls;

			//! Specifies whether to use HTTP POST instead of HTTP GET to retrieve custom URLs.
			/*!
			 * Has no effect after dynamic redirects.
			 *
			 * \sa redirectUsePost
			 */
			bool customUsePost{false};

			///@}

			///@name Expected Number of Results
			///@{

			//! Specifies whether to throw an exception when number of expected URLs is exceeded.
			bool expectedErrorIfLarger{false};

			//! Specifies whether to throw an exception when number of expected URLs is subceeded.
			bool expectedErrorIfSmaller{false};

			//! Query to be performed on content to retrieve the expected number of URLs.
			std::uint64_t expectedQuery{0};

			///@}
			///@name Dynamic Redirect
			///@{

			//! Custom HTTP @c Cookie header on dynamic redirect.
			std::string redirectCookies;

			//! Custom HTTP headers on dynamic redirect.
			std::vector<std::string> redirectHeaders;

			//! Query on content that specifies whether to redirect to different URL.
			std::uint64_t redirectQueryContent{0};

			//! Query on URL that specifies whether to redirect to different URL.
			std::uint64_t redirectQueryUrl{0};

			//! Sub-URL (for cross-domain websites: URL without protocol) to redirect to.
			std::string redirectTo;

			//! Specifies whether to use HTTP POST instead of HTTP GET to retrieve a URL after redirect.
			bool redirectUsePost{false};

			//! Variable names to be replaced on redirect.
			/*!
			 * Names will be replaced in the values
			 * of Config::Entries::redirectTo,
			 * Config::Entries::redirectCookies,
			 * and Config::Entries::redirectHeaders.
			 */
			std::vector<std::string> redirectVarNames;

			//! Query on variable source to retrieve the value of the variable with the same index.
			std::vector<std::uint64_t> redirectVarQueries;

			//! Source type of the variable with the same index.
			/*!
			 * \sa redirectSourceUrl,
			 *   redirectContentUrl
			 */
			std::vector<std::uint8_t> redirectVarSources;

			///@}
		}

		//! Configuration of the crawler.
		config;

		///@}

		//! Class for crawler configuration exceptions.
		MAIN_EXCEPTION_CLASS();

	protected:
		///@name Crawler-Specific Configuration Parsing
		///@{

		void parseOption() override;
		void checkOptions() override;
		void reset() override;

		///@}

	private:
		bool crossDomain{false};
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * SETTER
	 */

	//! Sets whether the corresponding website is cross-domain.
	/*!
	 * \param isCrossDomain Set whether the website
	 *   crawled by this crawler is cross-domain.
	 */
	inline void Config::setCrossDomain(bool isCrossDomain) {
		this->crossDomain = isCrossDomain;
	}

	/*
	 * CRAWLER-SPECIFIC CONFIGURATION PARSING
	 */

	//! Parses an crawler-specific configuration option.
	inline void Config::parseOption() {
		// crawler options
		this->category("crawler");
		this->option("archives", this->config.crawlerArchives);
		this->option("archives.names", this->config.crawlerArchivesNames);
		this->option("archives.urls.memento", this->config.crawlerArchivesUrlsMemento);
		this->option("archives.urls.skip", this->config.crawlerArchivesUrlsSkip);
		this->option("archives.urls.timemap", this->config.crawlerArchivesUrlsTimemap);
		this->option("lock", this->config.crawlerLock);
		this->option("logging", this->config.crawlerLogging);
		this->option("max.batch.size", this->config.crawlerMaxBatchSize);
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
		this->option("remove.xml.instructions", this->config.crawlerRemoveXmlInstructions);
		this->option("repair.cdata", this->config.crawlerRepairCData);
		this->option("repair.comments", this->config.crawlerRepairComments);
		this->option("retries", this->config.crawlerReTries);
		this->option("retry.archive", this->config.crawlerRetryArchive);
		this->option("retry.empty", this->config.crawlerRetryEmpty);
		this->option("retry.http", this->config.crawlerRetryHttp);
		this->option("sleep.error", this->config.crawlerSleepError);
		this->option("sleep.http", this->config.crawlerSleepHttp);
		this->option("sleep.idle", this->config.crawlerSleepIdle);
		this->option("sleep.mysql", this->config.crawlerSleepMySql);
		this->option(
				"start",
				this->config.crawlerStart,
				this->crossDomain ?
						StringParsingOption::URL : StringParsingOption::SubURL
		);
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

		// custom URLs options
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
		this->option("tokens.required", this->config.customTokensRequired);
		this->option("tokens.source", this->config.customTokensSource);
		this->option("tokens.use.post", this->config.customTokensUsePost);
		this->option("token.headers", this->config.customTokenHeaders);	// NOTE: to be used for ALL tokens
		this->option(
				"urls",
				this->config.customUrls,
				this->crossDomain ?
						StringParsingOption::URL : StringParsingOption::SubURL
		);
		this->option("use.post", this->config.customUsePost);

		// dynamic redirect options
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

		// expected number of results options
		this->category("expected");
		this->option("query", this->config.expectedQuery);
		this->option("error.if.larger", this->config.expectedErrorIfLarger);
		this->option("error.if.smaller", this->config.expectedErrorIfSmaller);
	}

	//! Checks the crawler-specific configuration options.
	/*!
	 * \throws Module::Crawler::Config::Exception
	 *   if no link extraction query has been
	 *   specified.
	 */
	inline void Config::checkOptions() {
		// check for link extraction query
		if(this->config.crawlerQueriesLinks.empty()) {
			throw Exception(
					"Crawler::Config::checkOptions():"
					" No link extraction query has been specified"
			);
		}

		// check number of URLs to crawl at once
		if(this->config.crawlerUrlChunks == 0) {
			this->config.crawlerUrlChunks = defaultUrlChunks;

			this->warning(
					"Invalid value for 'url.chunks' ignored (was zero),"
					"default value used"
			);
		}

		// check properties of archives
		bool incompleteArchives{false};

		const auto completeArchives{
			std::min({ // number of complete archives (= min. size of all arrays)
				this->config.crawlerArchivesNames.size(),
				this->config.crawlerArchivesUrlsMemento.size(),
				this->config.crawlerArchivesUrlsTimemap.size()
			})
		};

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
					"'archives.names', '.urls.memento' and '.urls.timemap'"
					" should have the same number of elements."
			);

			this->warning("Incomplete archive(s) removed from configuration.");
		}

		// check properties of counters
		bool incompleteCounters{false};

		const auto completeCounters{
			std::min({ // number of complete counters (= min. size of arrays)
				this->config.customCounters.size(),
				this->config.customCountersStart.size(),
				this->config.customCountersEnd.size()
			})
		};

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
					"'custom.counters', '.counters.start',"
					" '.counters.end' and '.counters.step'"
					" should have the same number of elements."
			);

			this->warning("Incomplete counter(s) removed from configuration.");

			incompleteCounters = false;
		}

		// remove step values that are not used, add one as step value where none is specified
		if(this->config.customCountersStep.size() > completeCounters) {
			incompleteCounters = true;
		}

		this->config.customCountersStep.resize(completeCounters, 1);

		// remove aliases that are not used, add empty aliases where none exist
		if(this->config.customCountersAlias.size() > completeCounters) {
			incompleteCounters = true;
		}

		this->config.customCountersAlias.resize(completeCounters);

		// remove alias summands that are not used, add zero as summand where none is specified
		if(this->config.customCountersAliasAdd.size() > completeCounters) {
			incompleteCounters = true;
		}

		this->config.customCountersAliasAdd.resize(completeCounters, 0);

		// warn about unused properties
		if(incompleteCounters) {
			this->warning("Unused counter properties removed from configuration.");
		}

		// check validity of counters
		//	(infinite counters are invalid, therefore the need to check for counter termination)
		for(std::size_t n{1}; n <= this->config.customCounters.size(); ++n) {
			const auto index{n - 1};

			if(
					(
							this->config.customCountersStep.at(index) <= 0
							&& this->config.customCountersStart.at(index)
							< this->config.customCountersEnd.at(index)
					)
					||
					(
							this->config.customCountersStep.at(index) >= 0
							&& this->config.customCountersStart.at(index)
							> this->config.customCountersEnd.at(index)
					)
			) {
				const std::string counterName(this->config.customCounters.at(index));

				// delete the invalid counter
				this->config.customCounters.erase(this->config.customCounters.begin() + index);
				this->config.customCountersStart.erase(this->config.customCountersStart.begin() + index);
				this->config.customCountersEnd.erase(this->config.customCountersEnd.begin() + index);
				this->config.customCountersStep.erase(this->config.customCountersStep.begin() + index);
				this->config.customCountersAlias.erase(this->config.customCountersAlias.begin() + index);
				this->config.customCountersAliasAdd.erase(this->config.customCountersAliasAdd.begin() + index);

				--n;

				this->warning(
						"Loop of counter '"
						+ counterName
						+ "' would be infinite, counter removed."
				);
			}
		}

		// check properties of tokens
		bool incompleteTokens{false};

		const auto completeTokens{
			std::min({ // number of complete tokens (= min. size of arrays)
				this->config.customTokens.size(),
				this->config.customTokensSource.size(),
				this->config.customTokensQuery.size()
			})
		};

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
					"'custom.tokens', '.tokens.source' and '.tokens.query'"
					" should have the same number of elements."
			);

			this->warning("Incomplete token(s) removed from configuration.");

			incompleteTokens = false;
		}

		// remove cookie headers that are not used, set to empty string where none is specified
		if(this->config.customTokensCookies.size() > completeTokens) {
			incompleteTokens = true;
		}

		this->config.customTokensCookies.resize(completeTokens);

		// remove token expiration times that are not used, set to '0' where none is specified
		if(this->config.customTokensKeep.size() > completeTokens) {
			incompleteTokens = true;
		}

		this->config.customTokensKeep.resize(completeTokens, 0);

		// remove token POST options that are not used, set to 'false' where none is specified
		if(this->config.customTokensUsePost.size() > completeTokens) {
			incompleteTokens = true;
		}

		this->config.customTokensUsePost.resize(completeTokens, false);

		// remove token requirements that are not used, set to 'false' where none is specified
		if(this->config.customTokensRequired.size() > completeTokens) {
			incompleteTokens = true;
		}

		this->config.customTokensRequired.resize(completeTokens, false);

		// warn about unused property
		if(incompleteTokens) {
			this->warning("Unused token properties removed from configuration.");
		}

		// check properties of variables for dynamic redirect
		bool incompleteVars{false};

		const auto completeVars{
			std::min({ // number of complete variables (= min. size of all arrays)
				this->config.redirectVarNames.size(),
				this->config.redirectVarQueries.size(),
				this->config.redirectVarSources.size()
			})
		};

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
					"'redirect.var.names', '.var.sources' and '.var.queries'"
					" should have the same number of elements."
			);

			this->warning("Incomplete variable(s) removed form configuration.");
		}
	}

	//! Resets the crawler-specific configuration options.
	inline void Config::reset() {
		this->config = {};
	}

} /* namespace crawlservpp::Module::Crawler */

#endif /* MODULE_CRAWLER_CONFIG_HPP_ */
