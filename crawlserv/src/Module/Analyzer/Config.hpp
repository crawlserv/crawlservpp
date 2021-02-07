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
 * Analyzing configuration.
 *
 *  Created on: Oct 25, 2018
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_CONFIG_HPP_
#define MODULE_ANALYZER_CONFIG_HPP_

#include "../Config.hpp"

#include <algorithm>	// std::min
#include <cstdint>		// std::uint8_t, std::std::uint64_t
#include <string>		// std::string
#include <vector>		// std::vector

//! Namespace for analyzer classes.
namespace crawlservpp::Module::Analyzer {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! An analyzer uses a parsing table as data source.
	inline constexpr std::uint8_t generalInputSourcesParsing{0};

	//! An analyzer uses an extracting table as data source.
	inline constexpr std::uint8_t generalInputSourcesExtracting{1};

	//! An analyzer uses an analyzing table as data source.
	inline constexpr std::uint8_t generalInputSourcesAnalyzing{2};

	//! An analyzer uses a crawling table as data source.
	inline constexpr std::uint8_t generalInputSourcesCrawling{3};

	//! Logging is disabled.
	inline constexpr std::uint8_t generalLoggingSilent{0};

	//! Default logging is enabled.
	inline constexpr std::uint8_t generalLoggingDefault{1};

	//! Extended logging is enabled.
	inline constexpr std::uint8_t generalLoggingExtended{2};

	//! Verbose logging is enabled.
	inline constexpr std::uint8_t generalLoggingVerbose{3};

	//! Default time (in s) to wait before last try to re-connect to MySQL server.
	inline constexpr std::uint64_t defaultSleepMySqlS{60};

	//! Default time (in ms) to wait each tick when finished.
	inline constexpr std::uint64_t defaultSleepWhenFinishedMs{5000};

	//! Minimum percentage of the maximum length for corpus slices.
	inline constexpr auto minPercentageCorpusSlices{1};

	//! Maximum percentage of the maximum length for corpus slices.
	inline constexpr auto maxPercentageCorpusSlices{99};

	//! Default percentage of the maximum length for corpus slices.
	inline constexpr auto defaultPercentageCorpusSlices{30};

	//! Default number of processed bytes in a continuous corpus after which memory will be freed.
	inline constexpr auto defaultFreeMemoryEvery{100000000};

	///@}

	/*
	 * DECLARATION
	 */

	//! Abstract configuration for analyzers, to be implemented by algorithm classes.
	class Config : protected Module::Config {
	public:
		/*
		 * needs virtual (i.e. overridden) default destructor, because of virtual member functions
		 * 		-> needs deleted copy and move contructors/operators
		 * 		-> needs default constructor
		 */

		///@name Construction and Destruction
		///@{

		//! Default constructor.
		Config() = default;

		//! Default destructor.
		~Config() override = default;

		///@}
		///@name Configuration
		///@{

		//! Configuration entries for analyzer threads.
		/*!
		 * \warning Changing the configuration requires
		 *   updating @c json/analyzer.json in @c
		 *   crawlserv_frontend!
		 */
		struct Entries {
			///@name %Analyzer Configuration
			///@{

			//! Check the consistency of text corpora.
			bool generalCorpusChecks{true};

			//! Corpus chunk size in percent of the maximum allowed package size by the MySQL server.
			std::uint8_t generalCorpusSlicing{defaultPercentageCorpusSlices};

			//! Columns to be used from the input tables.
			std::vector<std::string> generalInputFields;

			//! Types of tables to be used as input.
			/*!
			 * \sa generalInputSourcesParsing,
			 *   generalInputSourcesExtracting,
			 *   generalInputSourcesAnalyzing,
			 *   generalInputSourcesCrawling
			 */
			std::vector<std::uint8_t> generalInputSources;

			//! Names of tables to be used as input.
			std::vector<std::string> generalInputTables;

			//! Level of logging activity.
			std::uint8_t generalLogging{generalLoggingDefault};

			//! Time (in s) to wait before last try to re-connect to mySQL server.
			std::uint64_t generalSleepMySql{defaultSleepMySqlS};

			//! Time (in ms) to wait each tick when finished.
			std::uint64_t generalSleepWhenFinished{defaultSleepWhenFinishedMs};

			//! Table name to save analyzed data to.
			std::string generalTargetTable;

			///@}
			///@name Group by Date
			///@{

			//! The resolution to be used when grouping dates.
			/*!
			 * \sa Helper::DateTime::dateWeeks,
			 *   Helper::DateTime::dateMinutes,
			 *   Helper::DateTime::dateHours,
			 *   Helper::DateTime::dateDays,
			 *   Helper::DateTime::dateMonths,
			 *   Helper::DateTime::dateYears,
			 *   Helper::DateTime::dateSeconds
			 */
			std::uint8_t groupDateResolution{};

			///@}
			///@name Filter by Date
			///@{

			//! Enable filtering source data by date (only applies to parsed data).
			bool filterDateEnable{false};

			//! The date from which to filter the parsed data.
			std::string filterDateFrom;

			//! The date until which to filter the parsed data.
			std::string filterDateTo;

			///@}
			///@name Filter by Query
			///@{

			//! Queries which need to be fulfilled for at least one word in an article in order to keep it.
			/*!
			 * If no queries are given, no filtering will take place.
			 */
			std::vector<std::uint64_t> filterQueryQueries;

			///@}
			///@name Corpus Tokenization
			///@{

			//! Steps after which the corpus will be stored in the database.
			/*!
			 * If zero, the unmanipulated corpus will be
			 *  stored. After that, the numbering starts
			 *  with the sentence manipulators, and
			 *  continues with the word manipulators.
			 *
			 * \note Savepoints will be ignored, if a
			 *   suitable savepoint has been found beyond
			 *   them.
			 */
			std::vector<std::uint16_t> tokenizerSavePoints{};

			//! Manipulators used on the sentences of the text corpus.
			/*!
			 * \sa Data::sentenceManipNone,
			 * 	 Data::sentenceManipTagger,
			 */
			std::vector<std::uint16_t> tokenizerSentenceManipulators;

			//! Model for the sentence manipulator with the same array index.
			/*!
			 * Empty strings will be ignored.
			 */
			std::vector<std::string> tokenizerSentenceModels;

			//! Manipulators used on the words of the text corpus.
			/*!
			 * \sa Data::wordManipNone,
			 *   Data::wordManipPorter2Stemmer,
			 *   Data::wordManipGermanStemmer
			 */
			std::vector<std::uint16_t> tokenizerWordManipulators;

			//! Model for the word manipulator with the same array index.
			/*!
			 * Empty strings will be ignored.
			 */
			std::vector<std::string> tokenizerWordModels;

			//! Number of processed bytes in a continuous corpus after which memory will be freed.
			/*!
			 * If zero, memory will only be freed after
			 *  processing is complete.
			 */
			std::uint64_t tokenizerFreeMemoryEvery{defaultFreeMemoryEvery};

			///@}
		}

		//! Configuration of the analyzer.
		config;

		///@}

		/**@name Copy and Move
		 * The class is neither copyable, nor movable.
		 */
		///@{

		//! Deleted copy constructor.
		Config(Config&) = delete;

		//! Deleted copy assignment operator.
		Config& operator=(Config&) = delete;

		//! Deleted move constructor.
		Config(Config&&) = delete;

		//! Deleted move assignment operator.
		Config& operator=(Config&&) = delete;

		///@}

	protected:
		///@name Analyzer-Specific Configuration Parsing
		///@{

		void parseOption() override;
		void checkOptions() override;
		void reset() override;

		///@}
		///@name Algorithm-Specific Configuration Parsing
		///@{

		//! Parses an algorithm-specific configuration entry.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific algorithm.
		 */
		virtual void parseAlgoOption() = 0;

		//! Checks the algorithm-specific configuration.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific algorithm.
		 */
		virtual void checkAlgoOptions() = 0;

		//! Resets the algorithm-specific configuration.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific algorithm.
		 */
		virtual void resetAlgo() = 0;

		///@}
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * ANALYZER-SPECIFIC CONFIGURATION PARSING
	 */

	//! Parses an analyzer-specific configuration option.
	inline void Config::parseOption() {
		// general options
		this->category("general");
		this->option("corpus.checks", this->config.generalCorpusChecks);
		this->option("corpus.slicing", this->config.generalCorpusSlicing);
		this->option(
				"input.fields",
				this->config.generalInputFields,
				StringParsingOption::SQL
		);
		this->option("input.sources", this->config.generalInputSources);
		this->option(
				"input.tables",
				this->config.generalInputTables,
				StringParsingOption::SQL
		);
		this->option("logging", this->config.generalLogging);
		this->option("sleep.mysql", this->config.generalSleepMySql);
		this->option("sleep.when.finished", this->config.generalSleepWhenFinished);
		this->option(
				"target.table",
				this->config.generalTargetTable,
				StringParsingOption::SQL
		);

		// group by date option
		this->category("group-date");
		this->option("resolution", this->config.groupDateResolution);

		// filter by date options
		this->category("filter-date");
		this->option("enable", this->config.filterDateEnable);
		this->option("from", this->config.filterDateFrom);
		this->option("to", this->config.filterDateTo);

		// filter by query option
		this->category("filter-query");
		this->option("queries", this->config.filterQueryQueries);

		// corpus tokenization options
		this->category("tokenizer");
		this->option("savepoints", this->config.tokenizerSavePoints);
		this->option("sentence.manipulators", this->config.tokenizerSentenceManipulators);
		this->option("sentence models", this->config.tokenizerSentenceModels);
		this->option("word.manipulators", this->config.tokenizerWordManipulators);
		this->option("word.models", this->config.tokenizerWordModels);
		this->option("free.memory.every", this->config.tokenizerFreeMemoryEvery);

		// parse algo options
		this->parseAlgoOption();
	}

	//! Checks the analyzer-specific configuration options.
	inline void Config::checkOptions() {
		// check corpus chunk size (in percent of the maximum packet size allowed by the MySQL server)
		if(
				this->config.generalCorpusSlicing < minPercentageCorpusSlices
				|| this->config.generalCorpusSlicing > maxPercentageCorpusSlices
		) {
			this->config.generalCorpusSlicing = defaultPercentageCorpusSlices;

			this->warning(
					"Invalid corpus chunk size reset to "
					+ std::to_string(defaultPercentageCorpusSlices)
					+ "% of the maximum packet size allowed by the MySQL server."
			);
		}

		// check properties of input fields
		const auto completeInputs{
			std::min({ // number of complete inputs (= min. size of all arrays)
				this->config.generalInputFields.size(),
				this->config.generalInputSources.size(),
				this->config.generalInputTables.size()
			})
		};

		bool incompleteInputs{false};

		// remove field names that are not used
		if(this->config.generalInputFields.size() > completeInputs) {
			this->config.generalInputFields.resize(completeInputs);

			incompleteInputs = true;
		}

		// remove field sources that are not used
		if(this->config.generalInputSources.size() > completeInputs) {
			this->config.generalInputSources.resize(completeInputs);

			incompleteInputs = true;
		}

		// remove field source tables that are not used
		if(this->config.generalInputTables.size() > completeInputs) {
			// remove sources of incomplete datetime queries
			this->config.generalInputTables.resize(completeInputs);

			incompleteInputs = true;
		}

		// warn about incomplete input fields
		if(incompleteInputs) {
			this->warning(
					"'input.fields', '.sources' and '.tables'"
					" should have the same number of elements."
			);

			this->warning("Incomplete input field(s) removed from configuration.");
		}

		// check number of corpus manipulators and their models
		if(
				this->config.tokenizerSentenceModels.size()
				> this->config.tokenizerSentenceManipulators.size()
		) {
			this->warning(
					"The configuration contains"
					" more sentence models than sentence"
					" manipulators, redundant models will"
					" be ignored."
			);
		}

		if(
				this->config.tokenizerWordModels.size()
				> this->config.tokenizerWordManipulators.size()
		) {
			this->warning(
					"The configuration contains"
					" more word models than word"
					" manipulators, redundant models will"
					" be ignored."
			);
		}

		// resize so that the numbers of models equals the numbers of manipulators
		this->config.tokenizerSentenceModels.resize(
				this->config.tokenizerSentenceManipulators.size()
		);

		this->config.tokenizerWordModels.resize(
				this->config.tokenizerWordManipulators.size()
		);

		// check algo options
		this->checkAlgoOptions();
	}

	//! Resets the analyzer-specific configuration options.
	inline void Config::reset() {
		this->config = {};

		// reset algo options
		this->resetAlgo();
	}

} /* namespace crawlservpp::Module::Analyzer */

#endif /* MODULE_ANALYZER_CONFIG_HPP_ */
