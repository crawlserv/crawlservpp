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
 * MarkovTweet.cpp
 *
 *  Created on: Jan 13, 2019
 *      Author: ans
 */

#include "MarkovTweet.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	MarkovTweet::MarkovTweet(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions,
			const ThreadStatus& threadStatus
		) : Module::Analyzer::Thread(
				dbBase,
				threadOptions,
				threadStatus
			) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	//! Starts a new algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&)
	 */
	MarkovTweet::MarkovTweet(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions
	) :			Module::Analyzer::Thread(dbBase, threadOptions) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	/*
	 * IMPLEMENTED ALGORITHM FUNCTIONS
	 */

	//! Initializes the algorithm and processes its input.
	/*!
	 * \note In the case of this algorithm,
	 *   most of the work will be done during
	 *   initialization which therefore may take
	 *   a while.
	 *
	 * \throws Module::Analyzer::Thread::Exception
	 *   if the compilation of the text corpus
	 *   to be used as source failed.
	 */
	void MarkovTweet::onAlgoInit() {
		// check your sources
		this->setStatusMessage("Checking sources...");

		this->log(generalLoggingVerbose, "checks sources...");

		this->database.checkSources(
				this->config.generalInputSources,
				this->config.generalInputTables,
				this->config.generalInputFields
		);

		// set target fields
		std::vector<std::string> fields;
		std::vector<std::string> types;

		fields.reserve(2);
		types.reserve(2);

		fields.emplace_back(this->markovTweetResultField);
		fields.emplace_back(this->markovTweetSourcesField);

		types.emplace_back("LONGTEXT NOT NULL");
		types.emplace_back("BIGINT UNSIGNED NOT NULL");

		this->database.setTargetFields(fields, types);

		// initialize target table
		this->setStatusMessage("Creating target table...");

		this->log(generalLoggingVerbose, "creates target table...");

		this->database.initTargetTable(true);

		// get text corpus
		this->log(generalLoggingVerbose, "gets text corpus...");

		for(std::size_t n{0}; n < this->config.generalInputSources.size(); ++n) {
			std::string dateFrom;
			std::string dateTo;
			std::size_t corpusSources{0};

			if(this->config.filterDateEnable) {
				dateFrom = this->config.filterDateFrom;
				dateTo = this->config.filterDateTo;
			}

			Data::Corpus corpus(this->config.generalCorpusChecks);

			std::string statusStr;

			if(this->config.generalInputSources.size() > 1) {
				std::ostringstream statusStrStr;

				statusStrStr.imbue(std::locale(""));

				statusStrStr << "Getting text corpus ";
				statusStrStr << n + 1;
				statusStrStr << "/";
				statusStrStr << this->config.generalInputSources.size();
				statusStrStr << "...";

				statusStr = statusStrStr.str();
			}
			else {
				statusStr = "Getting text corpus...";
			}

			StatusSetter statusSetter(
					statusStr,
					1.F,
					[this](const auto& status) {
						this->setStatusMessage(status);
					},
					[this](const auto progress) {
						this->setProgress(progress);
					}
			);

			this->database.getCorpus(
					CorpusProperties(
							this->config.generalInputSources.at(n),
							this->config.generalInputTables.at(n),
							this->config.generalInputFields.at(n)
					),
					dateFrom,
					dateTo,
					corpus,
					corpusSources,
					statusSetter
			);

			this->sources += corpusSources;

			this->generator.addCorpus(corpus.getcCorpus());
		}

		// set options
		this->setStatusMessage("Setting options...");

		this->log(generalLoggingVerbose, "sets options...");

		this->generator.setSpellChecking(this->markovTweetSpellcheck, this->markovTweetLanguage);
		this->generator.setVerbose(Module::Analyzer::generalLoggingVerbose);
		this->generator.setTiming(this->markovTweetTiming);

		// set callbacks (suppressing wrong error messages by Eclipse IDE)
		this->generator.setIsRunningCallback([this]() {
			return this->isRunning();
		});

		this->generator.setSetStatusCallback([this](const auto& status) {
			this->setStatusMessage(status);
		});

		this->generator.setSetProgressCallback([this](const auto progress) {
			this->setProgress(progress);
		});

		this->generator.setLogCallback([this](const auto level, const auto& entry) {
			this->log(level, entry);
		});

		// compile text corpus
		this->setStatusMessage("Compiling text corpus...");

		this->log(generalLoggingVerbose, "compiles text corpus...");

		if(!(this->generator.compile(this->markovTweetDimension))) {
			throw Exception("Error while compiling corpus for tweet generation");
		}

		// re-allow pausing the thread
		this->allowPausing();

		// algorithm is ready
		this->log(generalLoggingExtended, "is ready.");
	}

	//! Generates the requested texts.
	void MarkovTweet::onAlgoTick() {
		// check number of tweets (internally saved as "last") if necessary
		if(this->markovTweetMax > 0 && this->getLast() >= this->markovTweetMax) {
			this->finished();

			return;
		}

		// generate tweet
		this->setStatusMessage("Generating tweet...");

		const std::string tweet(
				this->generator.randomSentence(
						this->markovTweetLength
				)
		);

		// insert tweet into target table
		Data::InsertFieldsMixed data;

		data.columns_types_values.reserve(2);

		data.table =
				"crawlserv_"
				+ this->websiteNamespace
				+ "_"
				+ this->urlListNamespace
				+ "_analyzed_"
				+ this->config.generalTargetTable;

		data.columns_types_values.emplace_back(
				"analyzed__" + this->markovTweetResultField,
				Data::Type::_string,
				Data::Value(tweet)
		);

		data.columns_types_values.emplace_back(
				"analyzed__" + this->markovTweetSourcesField,
				Data::getTypeOfSizeT(),
				Data::Value(this->sources)
		);

		this->database.insertCustomData(data);

		// increase tweet count (internally saved as "last") and calculate progress if necessary
		if(this->markovTweetMax > 0) {
			this->incrementLast();

			this->setProgress(static_cast<float>(this->getLast()) / this->markovTweetMax);
		}

		// sleep if necessary
		if(this->markovTweetSleep > 0) {
			this->setStatusMessage("Sleeping...");

			this->sleep(this->markovTweetSleep);
		}
	}

	//! Does nothing.
	void MarkovTweet::onAlgoPause() {}

	//! Does nothing.
	void MarkovTweet::onAlgoUnpause() {}

	//! Does nothing.
	void MarkovTweet::onAlgoClear() {}

	/*
	 * IMPLEMENTED CONFIGURATION FUNCTIONS
	 */

	//! Parses a configuration option for the algorithm.
	void MarkovTweet::parseAlgoOption() {
		// algorithm options
		this->category("markov-tweet");
		this->option("dimension", this->markovTweetDimension);
		this->option("language", this->markovTweetLanguage);
		this->option("length", this->markovTweetLength);
		this->option("max", this->markovTweetMax);
		this->option("result.field", this->markovTweetResultField, StringParsingOption::SQL);
		this->option("sleep", this->markovTweetSleep);
		this->option("sources.field", this->markovTweetSourcesField, StringParsingOption::SQL);
		this->option("spellcheck", this->markovTweetSpellcheck);
		this->option("timing", this->markovTweetTiming);
	}

	//! Checks the configuration options for the algorithm.
	/*!
	 * \throws Module::Analyzer::Thread::Exception
	 *   if no input sources or no target table
	 *   are provided, if the given dimension
	 *   parameter is zero, or the length of the
	 *   texts to generate is zero.
	 */
	void MarkovTweet::checkAlgoOptions() {
		// algorithm options
		if(this->config.generalInputFields.empty()) {
			throw Exception(
					"Algo::MarkovTweet::checkOptions():"
					" No input sources have been provided"
			);
		}

		if(this->config.generalTargetTable.empty()) {
			throw Exception(
					"Algo::MarkovTweet::checkOptions():"
					" No target table has been specified"
			);
		}

		if(this->markovTweetDimension == 0) {
			throw Exception(
					"Algo::MarkovTweet::checkOptions():"
					" Markov chain dimension is zero"
			);
		}

		if(this->markovTweetLength == 0) {
			throw Exception(
					"Algo::MarkovTweet::checkOptions():"
					" Result tweet length is zero"
			);
		}

		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

}  /* namespace crawlservpp::Module::Analyzer::Algo */
