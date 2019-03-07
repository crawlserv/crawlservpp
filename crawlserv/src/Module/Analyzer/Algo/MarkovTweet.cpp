/*
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

#include "MarkovTweet.h"

namespace crawlservpp::Module::Analyzer::Algo {

// constructor A: run previously interrupted algorithm run
MarkovTweet::MarkovTweet(Main::Database& dbBase, unsigned long analyzerId, const std::string& analyzerStatus,
		bool analyzerPaused, const ThreadOptions& threadOptions, unsigned long analyzerLast)
			: Module::Analyzer::Thread(dbBase, analyzerId, analyzerStatus, analyzerPaused, threadOptions, analyzerLast),
			  sources(0) {
	this->disallowPausing(); // disallow pausing while initializing
}

// constructor B: start a new algorithm run
MarkovTweet::MarkovTweet(Main::Database& dbBase, const ThreadOptions& threadOptions)
			: Module::Analyzer::Thread(dbBase, threadOptions), sources(0) {
	this->disallowPausing(); // disallow pausing while initializing
}

// destructor stub
MarkovTweet::~MarkovTweet() {}

// initialize algorithm run, throws std::runtime_error
void MarkovTweet::onAlgoInit(bool resumed) {
	// check options
	if(this->config.generalInputFields.empty()) throw std::runtime_error("No input sources provided");
	if(this->config.generalResultTable.empty()) throw std::runtime_error("No result table specified");
	if(!(this->config.markovTweetDimension)) throw std::runtime_error("Markov chain dimension is zero");
	if(!(this->config.markovTweetLength)) throw std::runtime_error("Result tweet length is zero");

	// set target fields
	std::vector<std::string> fields, types;
	fields.reserve(2);
	types.reserve(2);
	fields.emplace_back(this->config.markovTweetResultField);
	fields.emplace_back(this->config.markovTweetSourcesField);
	types.emplace_back("LONGTEXT NOT NULL");
	types.emplace_back("BIGINT UNSIGNED NOT NULL");
	this->database.setTargetFields(fields, types);

	// initialize target table
	this->setStatusMessage("Creating result table...");
	this->database.initTargetTable(true, std::bind(&MarkovTweet::isRunning, this)); // @suppress("Invalid arguments")

	// get text corpus
	this->setStatusMessage("Getting text corpus...");

	for(unsigned long n = 0; n < this->config.generalInputSources.size(); n++) {
		std::string corpus, dateFrom, dateTo;
		unsigned long corpusSources = 0;
		if(this->config.filterDateEnable) {
			dateFrom = this->config.filterDateFrom;
			dateTo = this->config.filterDateTo;
		}
		this->database.getCorpus(CorpusProperties(this->config.generalInputSources.at(n),
				this->config.generalInputTables.at(n), this->config.generalInputFields.at(n)), corpus, corpusSources, dateFrom, dateTo);
		this->generator.addCorpus(corpus);
		this->sources += corpusSources;
	}

	// set options
	this->generator.setSpellChecking(this->config.markovTweetSpellcheck, this->config.markovTweetLanguage);
	this->generator.setVerbose(this->config.generalLogging == Module::Analyzer::Config::generalLoggingVerbose);
	this->generator.setTiming(this->config.markovTweetTiming);

	// set callbacks (suppressing wrong error messages by Eclipse IDE)
	this->generator.setIsRunningCallback( // @suppress("Invalid arguments")
			std::bind(&MarkovTweet::_isRunning, this));
	this->generator.setSetStatusCallback( // @suppress("Invalid arguments")
			std::bind(&MarkovTweet::_setStatus, this, std::placeholders::_1));
	this->generator.setSetProgressCallback( // @suppress("Invalid arguments")
			std::bind(&MarkovTweet::_setProgress, this, std::placeholders::_1));
	this->generator.setLogCallback( // @suppress("Invalid arguments")
			std::bind(&MarkovTweet::_log, this, std::placeholders::_1));

	// compile text corpus
	if(!(this->generator.compile(this->config.markovTweetDimension)))
		throw std::runtime_error("Error while compiling corpus for tweet generation");

	// re-allow pausing the thread
	this->allowPausing();

	this->setStatusMessage("Generating tweets...");
}

// algorithm tick
void MarkovTweet::onAlgoTick() {
	// check number of tweets (internally saved as "last") if necessary
	if(this->config.markovTweetMax && this->getLast() >= this->config.markovTweetMax) {
		this->finished();
		return;
	}

	// generate tweet
	std::string tweet = this->generator.randomSentence(this->config.markovTweetLength);

	// insert tweet into result table in the database
	Main::Data::InsertFieldsMixed data;
	data.columns_types_values.reserve(2);
	data.table = "crawlserv_" + this->websiteNamespace + "_"
			+ this->urlListNamespace + "_analyzed_" + this->config.generalResultTable;
	data.columns_types_values.emplace_back("analyzed__"
			+ this->config.markovTweetResultField, DataType::_string, DataValue(tweet));
	data.columns_types_values.emplace_back("analyzed__"
			+ this->config.markovTweetSourcesField, DataType::_ulong, DataValue(this->sources));
	this->database.insertCustomData(data);

	// increase tweet count (internally saved as "last") and calculate progress if necessary
	if(this->config.markovTweetMax) {
		this->incrementLast();
		this->setProgress((float) this->getLast() / this->config.markovTweetMax);
	}

	// sleep if necessary
	if(this->config.markovTweetSleep) {
		std::this_thread::sleep_for(std::chrono::milliseconds(this->config.markovTweetSleep));
	}
}

// pause algorithm run
void MarkovTweet::onAlgoPause() {}

// unpause algorithm run
void MarkovTweet::onAlgoUnpause() {}

// clear algorithm run
void MarkovTweet::onAlgoClear(bool interrupted) {}

// external access to thread functionality for rawr
bool MarkovTweet::_isRunning() { return this->isRunning(); }
void MarkovTweet::_setStatus(const std::string& status) { this->setStatusMessage(status); }
void MarkovTweet::_setProgress(float progress) { this->setProgress(progress); }
void MarkovTweet::_log(const std::string& entry) {
	this->log(entry);
}

}
