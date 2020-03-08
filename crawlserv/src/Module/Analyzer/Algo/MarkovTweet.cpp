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
 * MarkovTweet.cpp
 *
 * Markov Chain Tweet Generator algorithm implemented as child of the abstract Analyzer thread class.
 *
 *   This is a semi-serious proof-of-concept class for the crawlserv++ Analyzer module.
 *
 *   It uses the markov chain algorithm to generate random tweets from a large text corpus previously parsed.
 *
 *   The implementation of the algorithm itself is done by the slightly modified rawr class, originally by Kelly Rauchenberger
 *   at https://github.com/hatkirby/rawr-ebooks 👌
 *
 *   WARNING: This algorithm may use A LOT of memory when parsing large corpora, adjust your swap size accordingly to prevent
 *   		 	the server from being killed by the operating system!
 *
 *  Created on: Jan 13, 2019
 *      Author: ans
 */

#include "MarkovTweet.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	// constructor A: run previously interrupted algorithm run
	MarkovTweet::MarkovTweet(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions,
			const ThreadStatus& threadStatus
		) : Module::Analyzer::Thread(
				dbBase,
				threadOptions,
				threadStatus
			),
			sources(0),
			markovTweetDimension(5),
			markovTweetLanguage("en_US"),
			markovTweetLength(140),
			markovTweetMax(0),
			markovTweetResultField("tweet"),
			markovTweetSleep(0),
			markovTweetSourcesField("sources"),
			markovTweetSpellcheck(true),
			markovTweetTiming(true) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	// constructor B: start a new algorithm run
	MarkovTweet::MarkovTweet(	Main::Database& dbBase,
								const ThreadOptions& threadOptions)
									:	Module::Analyzer::Thread(dbBase, threadOptions),
										sources(0),
										markovTweetDimension(5),
										markovTweetLanguage("en_US"),
										markovTweetLength(140),
										markovTweetMax(0),
										markovTweetResultField("tweet"),
										markovTweetSleep(0),
										markovTweetSourcesField("sources"),
										markovTweetSpellcheck(true),
										markovTweetTiming(true) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	// destructor stub
	MarkovTweet::~MarkovTweet() {}

	// initialize algorithm run, throws Thread::Exception
	void MarkovTweet::onAlgoInit() {
		// check your sources
		this->setStatusMessage("Creating result table...");

		this->log(Config::generalLoggingVerbose, "checks sources...");

		this->database.checkSources(
				this->config.generalInputSources,
				this->config.generalInputTables,
				this->config.generalInputFields
		);

		// set target fields
		std::vector<std::string> fields, types;
		std::vector<Struct::TextMapEntry> articleMap, dateMap;

		fields.reserve(2);
		types.reserve(2);

		fields.emplace_back(this->markovTweetResultField);
		fields.emplace_back(this->markovTweetSourcesField);

		types.emplace_back("LONGTEXT NOT NULL");
		types.emplace_back("BIGINT UNSIGNED NOT NULL");

		this->database.setTargetFields(fields, types);

		// initialize target table
		this->setStatusMessage("Creating result table...");

		this->log(Config::generalLoggingVerbose, "creates result table...");

		this->database.initTargetTable(true);

		// get text corpus
		this->setStatusMessage("Getting text corpus...");

		this->log(Config::generalLoggingVerbose, "gets text corpus...");

		for(size_t n = 0; n < this->config.generalInputSources.size(); ++n) {
			std::string dateFrom, dateTo;
			Data::Corpus corpus;
			size_t corpusSources = 0;

			if(this->config.filterDateEnable) {
				dateFrom = this->config.filterDateFrom;
				dateTo = this->config.filterDateTo;
			}

			this->database.getCorpus(
					CorpusProperties(
							this->config.generalInputSources.at(n),
							this->config.generalInputTables.at(n),
							this->config.generalInputFields.at(n)
					),
					dateFrom,
					dateTo,
					corpus,
					corpusSources
			);

			this->generator.addCorpus(corpus.getCorpus());

			this->sources += corpusSources;
		}

		// set options
		this->setStatusMessage("Setting options...");

		this->log(Config::generalLoggingVerbose, "sets options...");

		this->generator.setSpellChecking(this->markovTweetSpellcheck, this->markovTweetLanguage);
		this->generator.setVerbose(Module::Analyzer::Config::generalLoggingVerbose);
		this->generator.setTiming(this->markovTweetTiming);

		// set callbacks (suppressing wrong error messages by Eclipse IDE)
		this->generator.setIsRunningCallback( // @suppress("Invalid arguments")
				std::bind(&MarkovTweet::_isRunning, this)
		);

		this->generator.setSetStatusCallback( // @suppress("Invalid arguments")
				std::bind(&MarkovTweet::_setStatus, this, std::placeholders::_1)
		);

		this->generator.setSetProgressCallback( // @suppress("Invalid arguments")
				std::bind(&MarkovTweet::_setProgress, this, std::placeholders::_1)
		);

		this->generator.setLogCallback( // @suppress("Invalid arguments")
				std::bind(&MarkovTweet::_log, this, std::placeholders::_1, std::placeholders::_2)
		);

		// compile text corpus
		this->setStatusMessage("Compiling text corpus...");

		this->log(Config::generalLoggingVerbose, "compiles text corpus...");

		if(!(this->generator.compile(this->markovTweetDimension)))
			throw Exception("Error while compiling corpus for tweet generation");

		// re-allow pausing the thread
		this->allowPausing();

		// algorithm is ready
		this->log(Config::generalLoggingExtended, "is ready.");
	}

	// algorithm tick
	void MarkovTweet::onAlgoTick() {
		// check number of tweets (internally saved as "last") if necessary
		if(this->markovTweetMax && this->getLast() >= this->markovTweetMax) {
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

		// insert tweet into result table in the database
		Data::InsertFieldsMixed data;

		data.columns_types_values.reserve(2);

		data.table =
				"crawlserv_"
				+ this->websiteNamespace
				+ "_"
				+ this->urlListNamespace
				+ "_analyzed_"
				+ this->config.generalResultTable;

		data.columns_types_values.emplace_back(
				"analyzed__" + this->markovTweetResultField,
				DataType::_string,
				DataValue(tweet)
		);

		data.columns_types_values.emplace_back(
				"analyzed__" + this->markovTweetSourcesField,
				DataType::_ulong,
				DataValue(this->sources)
		);

		this->database.insertCustomData(data);

		// increase tweet count (internally saved as "last") and calculate progress if necessary
		if(this->markovTweetMax) {
			this->incrementLast();

			this->setProgress(static_cast<float>(this->getLast()) / this->markovTweetMax);
		}

		// sleep if necessary
		if(this->markovTweetSleep) {
			this->setStatusMessage("Sleeping...");

			this->sleep(this->markovTweetSleep);
		}
	}

	// pause algorithm run
	void MarkovTweet::onAlgoPause() {}

	// unpause algorithm run
	void MarkovTweet::onAlgoUnpause() {}

	// clear algorithm run
	void MarkovTweet::onAlgoClear() {}

	// parse algorithm option
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

	// check algorithm options, throws Thread::Exception
	void MarkovTweet::checkAlgoOptions() {
		// algorithm options
		if(this->config.generalInputFields.empty())
			throw Exception("Algo::MarkovTweet::checkOptions(): No input sources provided");

		if(this->config.generalResultTable.empty())
			throw Exception("Algo::MarkovTweet::checkOptions(): No result table specified");

		if(!(this->markovTweetDimension))
			throw Exception("Algo::MarkovTweet::checkOptions(): Markov chain dimension is zero");

		if(!(this->markovTweetLength))
			throw Exception("Algo::MarkovTweet::checkOptions(): Result tweet length is zero");

		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

	// external access to thread functionality for rawr
	bool MarkovTweet::_isRunning() {
		return this->isRunning();
	}

	void MarkovTweet::_setStatus(const std::string& status) {
		this->setStatusMessage(status);
	}

	void MarkovTweet::_setProgress(float progress) {
		this->setProgress(progress);
	}

	void MarkovTweet::_log(unsigned short level, const std::string& entry) {
		this->log(level, entry);
	}

}  /* crawlservpp::Module::Analyzer::Algo */
