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

#include <algorithm>	// std::min, std::replace_if
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
			std::vector<long> variablesCounterEnd;
			std::vector<long> variablesCounterStart;
			std::vector<long> variablesCounterStep;
			std::vector<std::string> variablesDateTimeFormat;
			std::vector<std::string> variablesName;
			std::vector<std::string> variablesParsedColumn;
			std::vector<std::string> variablesParsedTable;
			std::vector<unsigned long> variablesQuery;
			std::vector<unsigned char> variablesSource;
			std::vector<std::string> variablesTokens;
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
										generalSleepError(10000),
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
		this->option("counter.end", this->config.variablesCounterEnd);
		this->option("counter.start", this->config.variablesCounterStart);
		this->option("counter.step", this->config.variablesCounterStep);
		this->option("datetime.format", this->config.variablesDateTimeFormat);
		this->option("name", this->config.variablesName);
		this->option("parsed.column", this->config.variablesParsedColumn);
		this->option("parsed.table", this->config.variablesParsedTable);
		this->option("query", this->config.variablesQuery);
		this->option("source", this->config.variablesSource);
		this->option("tokens", this->config.variablesTokens);
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
		// check properties of variables
		bool incompleteVariables = false;

		const unsigned long completeVariables = std::min({ // number of complete variables (= minimum size of arrays)
				this->config.variablesName.size(),
				this->config.variablesSource.size(),
				this->config.variablesQuery.size()
		});

		// remove variable names that are not used
		if(this->config.variablesName.size() > completeVariables) {
			this->config.variablesName.resize(completeVariables);

			incompleteVariables = true;
		}

		// remove variable sources that are not used
		if(this->config.variablesSource.size() > completeVariables) {
			this->config.variablesSource.resize(completeVariables);

			incompleteVariables = true;
		}

		// remove variable queries that are not used
		if(this->config.variablesQuery.size() > completeVariables) {
			this->config.variablesQuery.resize(completeVariables);

			incompleteVariables = true;
		}

		// warn about incomplete variables
		if(incompleteVariables) {
			this->warning(
					"\'variables.name\', \'.source\' and \'.query\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete variable(s) removed from configuration.");

			incompleteVariables = false;
		}

		// remove variable tables that are not used, add empty table where none is specified
		if(this->config.variablesParsedTable.size() > completeVariables)
			incompleteVariables = true;

		this->config.variablesParsedTable.resize(completeVariables);

		// remove variable columns that are not used, add empty column where none is specified
		if(this->config.variablesParsedColumn.size() > completeVariables)
			incompleteVariables = true;

		this->config.variablesParsedColumn.resize(completeVariables);

		// remove variable date/time formats that are not used, add empty format where none is specified
		if(this->config.variablesDateTimeFormat.size() > completeVariables)
			incompleteVariables = true;

		this->config.variablesDateTimeFormat.resize(completeVariables);

		// replace empty date/time formats with "%F %T"
		std::replace_if(
				this->config.variablesDateTimeFormat.begin(),
				this->config.variablesDateTimeFormat.end(),
				[](const auto& str) {
					return str.empty();
				},
				"%F %T"
		);

		// warn about unused properties
		if(incompleteVariables)
			this->warning("Unused variable properties removed from configuration.");

		// check validity of counters (infinite counters are invalid, therefore the need to check for counter termination)
		for(unsigned long n = 1; n <= this->config.variablesName.size(); ++n) {
			const unsigned long i = n - 1;

			if(
					(this->config.variablesSource.at(i) == Config::variablesSourcesCounter)
					&&
					((
							this->config.variablesCounterStep.at(i) <= 0
							&& this->config.variablesCounterStart.at(i) < this->config.variablesCounterEnd.at(i)
					)
					||
					(
							this->config.variablesCounterStep.at(i) >= 0
							&& this->config.variablesCounterStart.at(i) > this->config.variablesCounterEnd.at(i)
					))
			) {
				const std::string counterName(this->config.variablesName.at(i));

				// delete the invalid (counter) variable
				this->config.variablesName.erase(this->config.variablesName.begin() + i);
				this->config.variablesSource.erase(this->config.variablesSource.begin() + i);
				this->config.variablesQuery.erase(this->config.variablesQuery.begin() + i);
				this->config.variablesParsedTable.erase(this->config.variablesParsedTable.begin() + i);
				this->config.variablesParsedColumn.erase(this->config.variablesParsedColumn.begin() + i);
				this->config.variablesDateTimeFormat.erase(this->config.variablesDateTimeFormat.begin() + i);
				this->config.variablesCounterStart.erase(this->config.variablesCounterStart.begin() + i);
				this->config.variablesCounterEnd.erase(this->config.variablesCounterEnd.begin() + i);
				this->config.variablesCounterStep.erase(this->config.variablesCounterStep.begin() + i);
				this->config.variablesAlias.erase(this->config.variablesAlias.begin() + i);
				this->config.variablesAliasAdd.erase(this->config.variablesAliasAdd.begin() + i);

				--n;

				this->warning(
						"Loop of counter \'"
						+ counterName
						+ "\' would be infinite, variable removed."
				);
			}
		}

		// check properties of tokens
		bool incompleteTokens = false;

		const unsigned long completeTokens = std::min({ // number of complete tokens (= minimum size of arrays)
				this->config.variablesTokens.size(),
				this->config.variablesTokensSource.size(),
				this->config.variablesTokensQuery.size()
		});

		// remove token variable names that are not used
		if(this->config.variablesTokens.size() > completeTokens) {
			this->config.variablesTokens.resize(completeTokens);

			incompleteTokens = true;
		}

		// remove token sources that are not used
		if(this->config.variablesTokensSource.size() > completeTokens) {
			this->config.variablesTokensSource.resize(completeTokens);

			incompleteTokens = true;
		}

		// remove token queries that are not used
		if(this->config.variablesTokensQuery.size() > completeTokens) {
			this->config.variablesTokensQuery.resize(completeTokens);

			incompleteTokens = true;
		}

		// warn about incomplete counters
		if(incompleteTokens) {
			this->warning(
					"\'variables.tokens\', \'.tokens.source\' and \'.tokens.query\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete token(s) removed from configuration.");

			incompleteTokens = false;
		}

		// remove token POST options that are not used, set to 'false' where none is specified
		if(this->config.variablesTokensUsePost.size() > completeTokens)
			incompleteTokens = true;

		this->config.variablesTokensUsePost.resize(completeTokens, false);

		// warn about unused property
		if(incompleteTokens)
			this->warning("Unused token properties removed from configuration.");

		// check properties of date/time queries
		bool incompleteDateTimes = false;

		// remove date/time formats that are not used, add empty format where none is specified
		if(this->config.extractingDateTimeFormats.size() > this->config.extractingDateTimeQueries.size())
			incompleteDateTimes = true;

		this->config.extractingDateTimeFormats.resize(this->config.extractingDateTimeQueries.size());

		// remove date/time locales that are not used, add empty locale where none is specified
		if(this->config.extractingDateTimeLocales.size() > this->config.extractingDateTimeQueries.size())
			incompleteDateTimes = true;

		this->config.extractingDateTimeLocales.resize(this->config.extractingDateTimeQueries.size());

		// warn about unused properties
		if(incompleteDateTimes)
			this->warning("Unused date/time properties removed from configuration.");

		// replace empty date/time formats with "%F %T"
		std::replace_if(
				this->config.extractingDateTimeFormats.begin(),
				this->config.extractingDateTimeFormats.end(),
				[](const auto& str) {
					return str.empty();
				},
				"%F %T"
		);

		// check properties of fields
		const unsigned long completeFields = std::min(
				this->config.extractingFieldNames.size(),
				this->config.extractingFieldQueries.size()
		);

		bool incompleteFields = false;

		// remove field names or queries that are not used
		if(this->config.extractingFieldNames.size() > completeFields) {
			incompleteFields = true;

			this->config.extractingFieldNames.resize(completeFields);
		}
		else if(this->config.extractingFieldQueries.size() > completeFields) {
			incompleteFields = true;

			this->config.extractingFieldQueries.resize(completeFields);
		}

		// warn about incomplete fields
		if(incompleteFields) {
			this->warning(
					"\'variables.field.names\' and \'.field.queries\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete field(s) removed from configuration.");

			incompleteFields = false;
		}

		// remove 'tidy text' properties that are not used, set to 'false' where none is specified
		if(this->config.extractingFieldTidyTexts.size() > completeFields)
			incompleteFields = true;

		this->config.extractingFieldTidyTexts.resize(completeFields, false);

		// remove 'warning if empty' properties that are not used, set to 'false' where none is specified
		if(this->config.extractingFieldWarningsEmpty.size() > completeFields)
			incompleteFields = true;

		this->config.extractingFieldWarningsEmpty.resize(completeFields, false);

		// warn about unused properties
		if(incompleteFields)
			this->warning("Unused field properties removed from configuration.");
	}

} /* crawlservpp::Module::Extractor */

#endif /* MODULE_EXTRACTOR_CONFIG_HPP_ */
