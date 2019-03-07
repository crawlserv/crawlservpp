/*
 * MarkovText.cpp
 *
 *   Markov Chain Text Generator algorithm implemented as child of the abstract Analyzer thread class.
 *
 *   This is a semi-serious proof-of-concept class for the crawlserv++ Analyzer module.
 *
 *   It uses the markov chain algorithm to generate random texts from a large text corpus previously parsed.
 *
 *   The implementation of the algorithm itself is taken from Rosetta Code at https://rosettacode.org/wiki/Markov_chain_text_generator
 *
 *
 *  Created on: Jan 13, 2019
 *      Author: ans
 */

#include "MarkovText.h"

namespace crawlservpp::Module::Analyzer::Algo {

// constructor A: run previously interrupted algorithm run
MarkovText::MarkovText(Main::Database& dbBase, unsigned long analyzerId, const std::string& analyzerStatus,
		bool analyzerPaused, const ThreadOptions& threadOptions, unsigned long analyzerLast)
			: Module::Analyzer::Thread(dbBase, analyzerId, analyzerStatus, analyzerPaused, threadOptions, analyzerLast),
			  sources(0) {
	this->disallowPausing(); // disallow pausing while initializing
}

// constructor B: start a new algorithm run
MarkovText::MarkovText(Main::Database& dbBase, const ThreadOptions& threadOptions)
			: Module::Analyzer::Thread(dbBase, threadOptions), sources(0) {
	this->disallowPausing(); // disallow pausing while initializing
}

// destructor stub
MarkovText::~MarkovText() {}

// initialize algorithm run, throws std::runtime_error
void MarkovText::onAlgoInit(bool resumed) {
	// check options
	if(this->config.generalInputFields.empty()) throw std::runtime_error("No input sources provided");
	if(this->config.generalResultTable.empty()) throw std::runtime_error("No result table specified");
	if(!(this->config.markovTextDimension)) throw std::runtime_error("Markov chain dimension is zero");
	if(!(this->config.markovTextLength)) throw std::runtime_error("Result text length is zero");

	// initialize random number generator
	srand(unsigned(time(NULL)));

	// set target fields
	std::vector<std::string> fields, types;
	fields.reserve(2);
	types.reserve(2);
	fields.emplace_back(this->config.markovTextResultField);
	fields.emplace_back(this->config.markovTextSourcesField);
	types.emplace_back("LONGTEXT NOT NULL");
	types.emplace_back("BIGINT UNSIGNED NOT NULL");
	this->database.setTargetFields(fields, types);

	// initialize target table
	this->setStatusMessage("Creating result table...");
	this->database.initTargetTable(true, std::bind(&MarkovText::isRunning, this)); // @suppress("Invalid arguments")

	// get text corpus
	this->setStatusMessage("Getting text corpus...");

	for(unsigned long n = 0; n < this->config.generalInputSources.size(); n++) {
		std::string corpus, dateFrom, dateTo;
		unsigned long corpusSources = 0;
		if(this->config.filterDateEnable) {
			dateFrom = this->config.filterDateFrom;
			dateTo = this->config.filterDateTo;
		}
		this->database.getCorpus(
				CorpusProperties(this->config.generalInputSources.at(n), this->config.generalInputTables.at(n),
				this->config.generalInputFields.at(n)), corpus, corpusSources, dateFrom, dateTo
		);
		this->source += corpus;
		this->source.push_back(' ');
		this->sources += corpusSources;
	}
	if(!(this->source.empty())) this->source.pop_back();

	// create dictionary
	this->setStatusMessage("Creating dictionary...");
	std::unique_ptr<Timer::Simple> timer;
	if(this->config.generalLogging && this->config.markovTextTiming) timer = std::make_unique<Timer::Simple>();
	this->createDictionary();
	if(this->isRunning()) {
		if(timer) this->log("created dictionary in " + timer->tickStr() + ".");

		// re-allow pausing the thread
		this->allowPausing();

		this->setStatusMessage("Generating texts...");
	}
}

// algorithm tick
void MarkovText::onAlgoTick() {
	// check number of texts (internally saved as "last") if necessary
	if(this->config.markovTextMax && this->getLast() >= this->config.markovTextMax) {
		this->finished();
		return;
	}

	// generate text
	std::unique_ptr<Timer::Simple> timer;
	if(this->config.generalLogging && this->config.markovTextTiming) timer = std::make_unique<Timer::Simple>();
	std::string text = this->createText();
	if(timer) this->log("created text in " + timer->tickStr() + ".");

	// insert text into result table in the database
	if(!text.empty()) {
		Main::Data::InsertFieldsMixed data;
		data.columns_types_values.reserve(2);
		data.table = "crawlserv_" + this->websiteNamespace + "_"
				+ this->urlListNamespace + "_analyzed_" + this->config.generalResultTable;
		data.columns_types_values.emplace_back("analyzed__"
				+ this->config.markovTextResultField, DataType::_string, DataValue(text));
		data.columns_types_values.emplace_back("analyzed__"
				+ this->config.markovTextSourcesField, DataType::_ulong, DataValue(this->sources));
		this->database.insertCustomData(data);

		// increase text count and progress (internally saved as "last") if necessary
		if(this->config.markovTextMax) {
			this->incrementLast();
			this->setProgress((float) this->getLast() / this->config.markovTextMax);
		}
	}
	else if(this->isRunning() && this->config.generalLogging) this->log("WARNING: Created text was empty.");

	// sleep if necessary
	if(this->config.markovTextSleep) {
		std::this_thread::sleep_for(std::chrono::milliseconds(this->config.markovTextSleep));
	}
}

// pause algorithm run
void MarkovText::onAlgoPause() {}

// unpause algorithm run
void MarkovText::onAlgoUnpause() {}

// clear algorithm run
void MarkovText::onAlgoClear(bool interrupted) {}

// create dictionary (code mostly from https://rosettacode.org/wiki/Markov_chain_text_generator)
void MarkovText::createDictionary() {
	// *** added: get dimension from configuration + counter
	unsigned int kl = this->config.markovTextDimension, counter = 0;
	// ***
	std::string w1, key;
	size_t wc = 0, pos = 0, next = 0;
	next = this->source.find_first_not_of( 32, 0 );
	if( next == std::string::npos ) return;
	while( wc < kl ) {
		pos = this->source.find_first_of( ' ', next );
		w1 = this->source.substr( next, pos - next );
		key += w1 + " ";
		next = this->source.find_first_not_of( 32, pos + 1 );
		if( next == std::string::npos ) return;
		wc++;
	}
	key = key.substr( 0, key.size() - 1 );
	while( true ) {
		next = this->source.find_first_not_of( 32, pos + 1 );
		if( next == std::string::npos ) return;
		pos = this->source.find_first_of( 32, next );

		// *** added: safer no more spaces check
		if(pos == std::string::npos) pos = this->source.length();
		// ***

		w1 = this->source.substr( next, pos - next );
		if( w1.empty() ) break;
		if( std::find( dictionary[key].begin(), dictionary[key].end(), w1 ) == dictionary[key].end() )
			dictionary[key].emplace_back( w1 );
		key = key.substr( key.find_first_of( 32 ) + 1 ) + " " + w1;

		// *** added: counter + check whether thread is still running + set progress
		counter++;
		if(counter > 1000000) {
			if(!(this->isRunning())) return;
			this->setProgress((float) next / this->source.length());
			counter = 0;
		}
		// ***
	}
}

// create text (code mostly from https://rosettacode.org/wiki/Markov_chain_text_generator)
std::string MarkovText::createText() {
	if(!dictionary.size()) throw std::runtime_error("Dictionary is empty");

	std::string key, first, second, result;
	size_t next = 0;
	std::map<std::string, std::vector<std::string> >::iterator it = dictionary.begin();
	result.reserve(this->config.markovTextLength * 10); // guess average word length
	int w = this->config.markovTextLength - this->config.markovTextDimension;
	std::advance( it, rand() % dictionary.size() );
	key = ( *it ).first;
	result += key;
	while( true ) {
		std::vector<std::string> d = dictionary[key];
		if( d.empty() ) break;
		second = d[rand() % d.size()];
		if( second.empty() ) break;
		result.push_back(' ');
		result += second;
		if( --w < 0 ) break;
		next = key.find_first_of( 32, 0 );
		first = key.substr( next + 1 );
		key = first + " " + second;

		if(!(this->isRunning())) return "";
	}
	return result;
}

}
