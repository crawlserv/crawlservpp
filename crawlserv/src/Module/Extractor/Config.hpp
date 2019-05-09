/*
 * Config.hpp
 *
 * Extracting configuration.
 *
 * WARNING:	Changing the configuration requires updating 'json/extractor.json' in crawlserv_frontend!
 * 			See there for details on the specific configuration entries.
 *
 *  Created on: May 9, 2019
 *      Author: ans
 */

#ifndef MODULE_EXTRACTOR_CONFIG_HPP_
#define MODULE_EXTRACTOR_CONFIG_HPP_

#include "../../Network/Config.hpp"

#include <algorithm>	// std::min
#include <queue>		// std::queue
#include <string>		// std::string
#include <vector>		// std::vector

namespace crawlservpp::Module::Extractor {

	/*
	 * DECLARATION
	 */

	class Config : public Network::Config {
		// for convenience
		typedef Struct::ConfigItem ConfigItem;

	public:
		Config() : crossDomain(false) {}
		virtual ~Config() {}

		// configuration constants
		static const unsigned char generalLoggingSilent = 0;
		static const unsigned char generalLoggingDefault = 1;
		static const unsigned char generalLoggingExtended = 2;
		static const unsigned char generalLoggingVerbose = 3;

		static const unsigned char variablesSourcesParsed = 0;
		static const unsigned char variablesSourcesContent = 1;
		static const unsigned char variablesSourcesCounter = 2;
		static const unsigned char variablesSourcesUrl = 3;

		static const unsigned char expectedSourceExtracting = 0;
		static const unsigned char expectedSourceParsed = 1;
		static const unsigned char expectedSourceContent = 2;

		// configuration entries
		struct Entries {
			// constructor with default values
			Entries();

			// general entries
			unsigned long generalCacheSize;
			bool generalExtractCustom;
			unsigned int generalLock;
			unsigned char generalLogging;
			bool generalReExtract;
			std::string generalResultTable;
			long generalReTries;
			unsigned long generalSleepError;
			unsigned long generalSleepHttp;
			unsigned long generalSleepIdle;
			unsigned long generalSleepMySql;
			bool generalTiming;

			// variables entries
			std::vector<std::string> variablesAlias;
			std::vector<long> variablesAliasAdd;
			std::vector<std::string> variablesColumn;
			std::vector<long> variablesCounterEnd;
			std::vector<long> variablesCounterStart;
			std::vector<long> variablesCounterStep;
			std::vector<std::string> variablesDateTimeFormat;
			std::vector<std::string> variablesName;
			std::vector<unsigned long> variablesQuery;
			std::vector<unsigned char> variablesSource;
			std::vector<std::string> variablesTable;
			std::vector<std::string> variablesTokensName;
			std::vector<unsigned long> variablesTokensQuery;
			std::vector<std::string> variablesTokensSource;
			std::vector<bool> variablesTokensUsePost;

			// paging entries
			long pagingFirst;
			std::string pagingFirstString;
			unsigned long pagingIsNext;
			unsigned long pagingNextFrom;
			long pagingStep;
			std::string pagingVariable;

			// source entries
			std::string sourceCookies;
			std::string sourceUrl;
			bool sourceUsePost;

			// extracting entries
			std::vector<std::string> extractingDateTimeFormats;
			std::vector<std::string> extractingDateTimeLocales;
			std::vector<unsigned long> extractingDateTimeQueries;
			std::vector<std::string> extractingFieldNames;
			std::vector<unsigned long> extractingFieldQueries;
			std::vector<bool> extractingFieldTidyTexts;
			std::vector<bool> extractingFieldWarningsEmpty;
			std::vector<unsigned long> extractingIdQueries;
			bool extractingRepairCData;

			// expected [number of results] entries
			bool expectedErrorIfLarger;
			bool expectedErrorIfSmaller;
			std::string expectedParsedColumn;
			std::string expectedParsedTable;
			unsigned long expectedQuery;
			unsigned char expectedSource;

		} config;

		// sub-class for Crawler::Config exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

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

	// configuration constructor: set default values
	inline Config::Entries::Entries() : generalCacheSize(2500),
										generalExtractCustom(false),
										generalLock(300),
										generalLogging(generalLoggingDefault),
										generalReExtract(false),
										generalReTries(720),
										generalSleepError(5000),
										generalSleepHttp(0),
										generalSleepIdle(5000),
										generalSleepMySql(20),
										generalTiming(false),
										pagingFirst(0),
										pagingIsNext(0),
										pagingNextFrom(0),
										pagingStep(1),
										pagingVariable("$p"),
										sourceUsePost(false),
										extractingRepairCData(true),
										expectedErrorIfLarger(false),
										expectedErrorIfSmaller(false),
										expectedQuery(0),
										expectedSource(expectedSourceExtracting) {}

	// parse extractor-specific configuration option
	inline void Config::parseOption() {
		// general options
		this->category("general");
		this->option("cache.size", this->config.generalCacheSize);
		this->option("extract.custom", this->config.generalExtractCustom);
		this->option("lock", this->config.generalLock);
		this->option("logging", this->config.generalLogging);
		this->option("reextract", this->config.generalReExtract);
		this->option("result.table", this->config.generalResultTable);
		this->option("retries", this->config.generalReTries);
		this->option("sleep.error", this->config.generalSleepError);
		this->option("sleep.http", this->config.generalSleepHttp);
		this->option("sleep.idle", this->config.generalSleepIdle);
		this->option("sleep.mysql", this->config.generalSleepMySql);
		this->option("sleep.timing", this->config.generalTiming);

		// variables
		this->category("variables");
		this->option("alias", this->config.variablesAlias);
		this->option("alias.add", this->config.variablesAliasAdd);
		this->option("column", this->config.variablesColumn);
		this->option("counter.end", this->config.variablesCounterEnd);
		this->option("counter.start", this->config.variablesCounterStart);
		this->option("counter.step", this->config.variablesCounterStep);
		this->option("datetime.format", this->config.variablesDateTimeFormat);
		this->option("name", this->config.variablesName);
		this->option("query", this->config.variablesQuery);
		this->option("source", this->config.variablesSource);
		this->option("table", this->config.variablesTable);
		this->option("tokens.name", this->config.variablesTokensName);
		this->option("tokens.query", this->config.variablesTokensQuery);
		this->option("tokens.source", this->config.variablesTokensSource);
		this->option("tokens.use.post", this->config.variablesTokensUsePost);

		// paging
		this->category("paging");
		this->option("paging.first", this->config.pagingFirst);
		this->option("paging.first.string", this->config.pagingFirstString);
		this->option("paging.is.next", this->config.pagingIsNext);
		this->option("paging.next.from", this->config.pagingNextFrom);
		this->option("paging.step", this->config.pagingStep);
		this->option("paging.variable", this->config.pagingVariable);

		// source
		this->category("source");
		this->option("source.cookies", this->config.sourceCookies);
		this->option("source.url", this->config.sourceUrl);
		this->option("source.use.post", this->config.sourceUsePost);

		// extracting
		this->category("extracting");
		this->option("extracting.datetime.formats", this->config.extractingDateTimeFormats);
		this->option("extracting.datetime.locales", this->config.extractingDateTimeLocales);
		this->option("extracting.datetime.queries", this->config.extractingDateTimeQueries);
		this->option("extracting.field.names", this->config.extractingFieldNames);
		this->option("extracting.field.queries", this->config.extractingFieldQueries);
		this->option("extracting.field.tidy.texts", this->config.extractingFieldTidyTexts);
		this->option("extracting.field.warnings.empty", this->config.extractingFieldWarningsEmpty);
		this->option("extracting.id.queries", this->config.extractingIdQueries);
		this->option("extracting.repair.cdata", this->config.extractingRepairCData);

		// expected [number of results]
		this->category("expected");
		this->option("error.if.larger", this->config.expectedErrorIfLarger);
		this->option("error.if.smaller", this->config.expectedErrorIfSmaller);
		this->option("parsed.column", this->config.expectedParsedColumn);
		this->option("parsed.table", this->config.expectedParsedTable);
		this->option("expected.query", this->config.expectedQuery);
		this->option("expected.source", this->config.expectedSource);
	}

	// check extracting-specific configuration, throws Config::Exception
	inline void Config::checkOptions() {
		// TODO
	}

} /* crawlservpp::Module::Extractor */

#endif /* MODULE_EXTRACTOR_CONFIG_HPP_ */
