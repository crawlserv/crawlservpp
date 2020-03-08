/*
 *
 * ---
 *
 *  Copyright (C) 2019-2020 Anselm Schmidt (ans[ät]ohai.su)
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

#include <algorithm>	// std::min std::replace_if
#include <string>		// std::string
#include <vector>		// std::vector

namespace crawlservpp::Module::Extractor {

	/*
	 * DECLARATION
	 */

	class Config : protected Network::Config {
		// for convenience
		using ConfigItem = Struct::ConfigItem;

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
		static const unsigned char variablesSourcesUrl = 2;

		static const unsigned char expectedSourceExtracting = 0;
		static const unsigned char expectedSourceParsed = 1;
		static const unsigned char expectedSourceContent = 2;

		// configuration entries
		struct Entries {
			// constructor with default values
			Entries();

			// general entries
			size_t generalCacheSize;
			bool generalExtractCustom;
			unsigned int generalLock;
			unsigned char generalLogging;
			bool generalMinimizeMemory;
			bool generalReExtract;
			std::string generalResultTable;
			long generalReTries;
			std::vector<unsigned int> generalRetryHttp;
			size_t generalSleepError;
			size_t generalSleepHttp;
			size_t generalSleepIdle;
			size_t generalSleepMySql;
			unsigned int generalTidyErrors;
			bool generalTidyWarnings;
			bool generalTiming;

			// variables entries
			std::vector<std::string> variablesAlias;
			std::vector<long> variablesAliasAdd;
			std::vector<std::string> variablesDateTimeFormat;
			std::vector<std::string> variablesDateTimeLocale;
			std::vector<std::string> variablesName;
			std::vector<std::string> variablesParsedColumn;
			std::vector<std::string> variablesParsedTable;
			std::vector<size_t> variablesQuery;
			std::vector<unsigned char> variablesSource;
			std::vector<std::string> variablesTokens;
			std::vector<std::string> variablesTokensCookies;
			std::vector<size_t> variablesTokensQuery;
			std::vector<std::string> variablesTokensSource;
			std::vector<std::string> variablesTokenHeaders;
			std::vector<bool> variablesTokensUsePost;

			// paging entries
			std::string pagingAlias;
			long pagingAliasAdd;
			long pagingFirst;
			std::string pagingFirstString;
			size_t pagingIsNextFrom;
			size_t pagingNextFrom;
			size_t pagingNumberFrom;
			long pagingStep;
			std::string pagingVariable;

			// source entries
			std::string sourceCookies;
			std::vector<std::string> sourceHeaders;
			std::string sourceUrl;
			std::string sourceUrlFirst;
			bool sourceUsePost;

			// extracting entries
			std::vector<size_t> extractingDataSetQueries;
			std::vector<std::string> extractingDateTimeFormats;
			std::vector<std::string> extractingDateTimeLocales;
			std::vector<size_t> extractingDateTimeQueries;
			std::vector<size_t> extractingErrorFail;
			std::vector<size_t> extractingErrorRetry;
			std::vector<std::string> extractingFieldDateTimeFormats;
			std::vector<std::string> extractingFieldDateTimeLocales;
			std::vector<char> extractingFieldDelimiters;
			std::vector<bool> extractingFieldIgnoreEmpty;
			std::vector<bool> extractingFieldJSON;
			std::vector<std::string> extractingFieldNames;
			std::vector<size_t> extractingFieldQueries;
			std::vector<bool> extractingFieldTidyTexts;
			std::vector<bool> extractingFieldWarningsEmpty;
			std::vector<std::string> extractingIdIgnore;
			std::vector<size_t> extractingIdQueries;
			bool extractingOverwrite;
			std::vector<size_t> extractingRecursive;
			size_t extractingRecursiveMaxDepth;
			bool extractingRemoveDuplicates;
			bool extractingRepairCData;
			bool extractingRepairComments;

			// expected [number of results] entries
			bool expectedErrorIfLarger;
			bool expectedErrorIfSmaller;
			std::string expectedParsedColumn;
			std::string expectedParsedTable;
			size_t expectedQuery;
			unsigned char expectedSource;

		} config;

		//class for Crawler::Config exceptions
		MAIN_EXCEPTION_CLASS();

	protected:
		// parsing of crawling-specific configuration
		void parseOption() override;
		void checkOptions() override;

	private:
		bool crossDomain;

		static void removeProtocolsFromUrl(std::string& inOut);
	};

	/*
	 * IMPLEMENTATION
	 */

	// configuration constructor: set default values
	inline Config::Entries::Entries() : generalCacheSize(2500),
										generalExtractCustom(false),
										generalLock(300),
										generalLogging(generalLoggingDefault),
										generalMinimizeMemory(false),
										generalReExtract(false),
										generalReTries(720),
										generalSleepError(10000),
										generalSleepHttp(0),
										generalSleepIdle(5000),
										generalSleepMySql(20),
										generalTidyErrors(0),
										generalTidyWarnings(false),
										generalTiming(false),
										pagingAliasAdd(0),
										pagingFirst(0),
										pagingIsNextFrom(0),
										pagingNextFrom(0),
										pagingNumberFrom(0),
										pagingStep(1),
										pagingVariable("$p"),
										sourceUsePost(false),
										extractingOverwrite(true),
										extractingRecursiveMaxDepth(100),
										extractingRemoveDuplicates(true),
										extractingRepairCData(true),
										extractingRepairComments(true),
										expectedErrorIfLarger(false),
										expectedErrorIfSmaller(false),
										expectedQuery(0),
										expectedSource(expectedSourceExtracting) {
		this->generalRetryHttp.push_back(429);
		this->generalRetryHttp.push_back(502);
		this->generalRetryHttp.push_back(503);
		this->generalRetryHttp.push_back(504);
	}

	// parse extractor-specific configuration option
	inline void Config::parseOption() {
		// general options
		this->category("general");
		this->option("cache.size", this->config.generalCacheSize);
		this->option("extract.custom", this->config.generalExtractCustom);
		this->option("lock", this->config.generalLock);
		this->option("logging", this->config.generalLogging);
		this->option("minimize.memory", this->config.generalMinimizeMemory);
		this->option("reextract", this->config.generalReExtract);
		this->option("result.table", this->config.generalResultTable);
		this->option("retries", this->config.generalReTries);
		this->option("retry.http", this->config.generalRetryHttp);
		this->option("sleep.error", this->config.generalSleepError);
		this->option("sleep.http", this->config.generalSleepHttp);
		this->option("sleep.idle", this->config.generalSleepIdle);
		this->option("sleep.mysql", this->config.generalSleepMySql);
		this->option("tidy.errors", this->config.generalTidyErrors);
		this->option("tidy.warnings", this->config.generalTidyWarnings);
		this->option("timing", this->config.generalTiming);

		// variables
		this->category("variables");
		this->option("alias", this->config.variablesAlias);
		this->option("alias.add", this->config.variablesAliasAdd);
		this->option("datetime.format", this->config.variablesDateTimeFormat);
		this->option("datetime.locale", this->config.variablesDateTimeLocale);
		this->option("name", this->config.variablesName);
		this->option("parsed.column", this->config.variablesParsedColumn);
		this->option("parsed.table", this->config.variablesParsedTable);
		this->option("query", this->config.variablesQuery);
		this->option("source", this->config.variablesSource);
		this->option("tokens", this->config.variablesTokens);
		this->option("tokens.cookies", this->config.variablesTokensCookies);
		this->option("tokens.query", this->config.variablesTokensQuery);
		this->option("tokens.source", this->config.variablesTokensSource);
		this->option("tokens.use.post", this->config.variablesTokensUsePost);
		this->option("token.headers", this->config.variablesTokenHeaders); // NOTE: to be used for ALL tokens

		// paging
		this->category("paging");
		this->option("alias", this->config.pagingAlias);
		this->option("alias.add", this->config.pagingAliasAdd);
		this->option("first", this->config.pagingFirst);
		this->option("first.string", this->config.pagingFirstString);
		this->option("is.next.from", this->config.pagingIsNextFrom);
		this->option("next.from", this->config.pagingNextFrom);
		this->option("number.from", this->config.pagingNumberFrom);
		this->option("step", this->config.pagingStep);
		this->option("variable", this->config.pagingVariable);

		// source
		this->category("source");
		this->option("cookies", this->config.sourceCookies);
		this->option("headers", this->config.sourceHeaders);
		this->option("url", this->config.sourceUrl);
		this->option("url.first", this->config.sourceUrlFirst);
		this->option("use.post", this->config.sourceUsePost);

		// extracting
		this->category("extracting");
		this->option("dataset.queries", this->config.extractingDataSetQueries);
		this->option("datetime.formats", this->config.extractingDateTimeFormats);
		this->option("datetime.locales", this->config.extractingDateTimeLocales);
		this->option("datetime.queries", this->config.extractingDateTimeQueries);
		this->option("error.fail", this->config.extractingErrorFail);
		this->option("error.retry", this->config.extractingErrorRetry);
		this->option("field.datetime.formats", this->config.extractingFieldDateTimeFormats);
		this->option("field.datetime.locales", this->config.extractingFieldDateTimeLocales);
		this->option("field.delimiters", this->config.extractingFieldDelimiters, CharParsingOption::FromString);
		this->option("field.ignore.empty", this->config.extractingFieldIgnoreEmpty);
		this->option("field.json", this->config.extractingFieldJSON);
		this->option("field.names", this->config.extractingFieldNames);
		this->option("field.queries", this->config.extractingFieldQueries);
		this->option("field.tidy.texts", this->config.extractingFieldTidyTexts);
		this->option("field.warnings.empty", this->config.extractingFieldWarningsEmpty);
		this->option("id.ignore", this->config.extractingIdIgnore);
		this->option("id.queries", this->config.extractingIdQueries);
		this->option("overwrite", this->config.extractingOverwrite);
		this->option("recursive", this->config.extractingRecursive);
		this->option("recursive.max.depth", this->config.extractingRecursiveMaxDepth);
		this->option("remove.duplicates", this->config.extractingRemoveDuplicates);
		this->option("repair.cdata", this->config.extractingRepairCData);
		this->option("repair.comments", this->config.extractingRepairComments);

		// expected [number of results]
		this->category("expected");
		this->option("error.if.larger", this->config.expectedErrorIfLarger);
		this->option("error.if.smaller", this->config.expectedErrorIfSmaller);
		this->option("parsed.column", this->config.expectedParsedColumn);
		this->option("parsed.table", this->config.expectedParsedTable);
		this->option("query", this->config.expectedQuery);
		this->option("source", this->config.expectedSource);
	}

	// check extracting-specific configuration, throws Config::Exception
	inline void Config::checkOptions() {
		// remove obvious protocols from given URLs
		for(auto& url : this->config.variablesTokensSource)
			Config::removeProtocolsFromUrl(url);

		Config::removeProtocolsFromUrl(this->config.sourceUrl);
		Config::removeProtocolsFromUrl(this->config.sourceUrlFirst);

		// check properties of variables
		bool incompleteVariables = false;

		const size_t completeVariables = std::min({
				/* number of complete variables (= minimum size of name and source arrays) */
				this->config.variablesName.size(),
				this->config.variablesSource.size()
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

		// warn about incomplete variables
		if(incompleteVariables) {
			this->warning(
					"\'variables.name\' and \'.source\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete variable(s) removed from configuration.");

			incompleteVariables = false;
		}

		// remove variable queries that are not used, add empty query where none is specified
		if(this->config.variablesQuery.size() > completeVariables)
			incompleteVariables = true;

		this->config.variablesQuery.resize(completeVariables);

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

		// remove variable date/time locales that are not used, add empty locale where none is specified
		if(this->config.variablesDateTimeLocale.size() > completeVariables)
			incompleteVariables = true;

		this->config.variablesDateTimeLocale.resize(completeVariables);

		// remove variable aliases that are not used, add empty alias where none is specified
		if(this->config.variablesAlias.size() > completeVariables)
			incompleteVariables = true;

		this->config.variablesAlias.resize(completeVariables);

		// remove variable alias values that are not used, add empty alias value where none is specified
		if(this->config.variablesAliasAdd.size() > completeVariables)
			incompleteVariables = true;

		this->config.variablesAliasAdd.resize(completeVariables);

		// warn about unused properties
		if(incompleteVariables)
			this->warning("Unused variable properties removed from configuration.");

		// check properties of tokens
		bool incompleteTokens = false;

		const size_t completeTokens = std::min({
				/* number of complete tokens (= minimum size of arrays) */
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

		// warn about incomplete tokens
		if(incompleteTokens) {
			this->warning(
					"\'variables.tokens\', \'.tokens.source\' and \'.tokens.query\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete token(s) removed from configuration.");

			incompleteTokens = false;
		}

		// remove cookie headers that are not used, set to empty string where none is specified
		if(this->config.variablesTokensCookies.size() > completeTokens)
			incompleteTokens = true;

		this->config.variablesTokensCookies.resize(completeTokens);

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
		const size_t completeFields = std::min(
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

		// remove date/time formats that are not used, add empty format where none is specified
		if(this->config.extractingFieldDateTimeFormats.size() > completeFields)
			incompleteFields = true;

		this->config.extractingFieldDateTimeFormats.resize(completeFields);

		// remove date/time locales that are not used, add empty locale where none is specified
		if(this->config.extractingFieldDateTimeLocales.size() > completeFields)
			incompleteFields = true;

		this->config.extractingFieldDateTimeLocales.resize(completeFields);

		// remove field delimiters that are not used, add empty delimiter (\0) where none is specified
		if(this->config.extractingFieldDelimiters.size() > completeFields)
			incompleteFields = true;

		this->config.extractingFieldDelimiters.resize(completeFields, '\0');

		// replace all empty field delimiters with '\n'
		std::replace_if(
				this->config.extractingFieldDelimiters.begin(),
				this->config.extractingFieldDelimiters.end(),
				[](char c) {
					return c == '\0';
				},
				'\n'
		);

		// remove 'ignore empty values' properties that are not used, set to 'true' where none is specified
		if(this->config.extractingFieldIgnoreEmpty.size() > completeFields)
			incompleteFields = true;

		this->config.extractingFieldIgnoreEmpty.resize(completeFields, true);

		// remove 'save field as JSON' properties that are not used, set to 'false' where none is specified
		if(this->config.extractingFieldJSON.size() > completeFields)
			incompleteFields = true;

		this->config.extractingFieldJSON.resize(completeFields, false);

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

	// remove obvious protocol(s) from beginning of URL
	inline void Config::removeProtocolsFromUrl(std::string& inOut) {
		while(inOut.length() > 6 && inOut.substr(0, 7) == "http://")
			inOut = inOut.substr(7);

		while(inOut.length() > 7 && inOut.substr(0, 8) == "https://")
			inOut = inOut.substr(8);
	}

} /* crawlservpp::Module::Extractor */

#endif /* MODULE_EXTRACTOR_CONFIG_HPP_ */
