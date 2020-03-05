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

#include "MarkovText.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	// constructor A: run previously interrupted algorithm run
	MarkovText::MarkovText(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions,
			const ThreadStatus& threadStatus
	) : Module::Analyzer::Thread(
				dbBase,
				threadOptions,
				threadStatus
			),
			sources(0),
			markovTextDimension(3),
			markovTextLength(400),
			markovTextMax(0),
			markovTextResultField("text"),
			markovTextSleep(0),
			markovTextSourcesField("sources"),
			markovTextTiming(true) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	// constructor B: start a new algorithm run
	MarkovText::MarkovText(	Main::Database& dbBase,
							const ThreadOptions& threadOptions)
								:	Module::Analyzer::Thread(dbBase, threadOptions),
									sources(0),
									markovTextDimension(3),
									markovTextLength(400),
									markovTextMax(0),
									markovTextResultField("text"),
									markovTextSleep(0),
									markovTextSourcesField("sources"),
									markovTextTiming(true) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	// destructor stub
	MarkovText::~MarkovText() {}

	// initialize algorithm run
	void MarkovText::onAlgoInit() {
		// initialize random number generator
		srand(unsigned(time(nullptr)));

		// set target fields
		std::vector<std::string> fields, types;

		fields.reserve(2);
		types.reserve(2);

		fields.emplace_back(this->markovTextResultField);
		fields.emplace_back(this->markovTextSourcesField);
		types.emplace_back("LONGTEXT NOT NULL");
		types.emplace_back("BIGINT UNSIGNED NOT NULL");

		this->database.setTargetFields(fields, types);

		// initialize target table
		this->setStatusMessage("Creating result table...");

		this->database.initTargetTable(true);

		// get text corpus
		this->setStatusMessage("Getting text corpus...");

		for(unsigned long n = 0; n < this->config.generalInputSources.size(); ++n) {
			std::string dateFrom, dateTo;

			unsigned long corpusSources = 0;

			if(this->config.filterDateEnable) {
				dateFrom = this->config.filterDateFrom;
				dateTo = this->config.filterDateTo;
			}

			Data::Corpus corpus;

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

			this->source += corpus.getCorpus();

			this->source.push_back(' ');

			this->sources += corpusSources;
		}

		if(!(this->source.empty()))
			this->source.pop_back();

		// create dictionary
		this->setStatusMessage("Creating dictionary...");

		std::unique_ptr<Timer::Simple> timer;

		if(this->markovTextTiming)
			timer = std::make_unique<Timer::Simple>();

		this->createDictionary();

		if(this->isRunning()) {
			if(timer)
				this->log(Config::generalLoggingDefault, "created dictionary in " + timer->tickStr() + ".");

			// re-allow pausing the thread
			this->allowPausing();
		}

		// algorithm is ready
		this->log(Config::generalLoggingExtended, "is ready.");
	}

	// algorithm tick
	void MarkovText::onAlgoTick() {
		// check number of texts (internally saved as "last") if necessary
		if(this->markovTextMax && this->getLast() >= this->markovTextMax) {
			this->finished();

			return;
		}

		// generate text
		this->setStatusMessage("Generating text...");

		std::unique_ptr<Timer::Simple> timer;

		if(this->markovTextTiming)
			timer = std::make_unique<Timer::Simple>();

		const std::string text(this->createText());

		if(timer)
			this->log(Config::generalLoggingDefault, "created text in " + timer->tickStr() + ".");

		// insert text into result table in the database
		if(!text.empty()) {
			Data::InsertFieldsMixed data;

			data.columns_types_values.reserve(2);

			data.table = "crawlserv_" + this->websiteNamespace + "_"
					+ this->urlListNamespace + "_analyzed_" + this->config.generalResultTable;

			data.columns_types_values.emplace_back(
					"analyzed__" + this->markovTextResultField,
					DataType::_string,
					DataValue(text)
			);

			data.columns_types_values.emplace_back(
					"analyzed__" + this->markovTextSourcesField,
					DataType::_ulong,
					DataValue(this->sources)
			);

			this->database.insertCustomData(data);

			// increase text count and progress (internally saved as "last") if necessary
			if(this->markovTextMax) {
				this->incrementLast();

				this->setProgress(static_cast<float>(this->getLast()) / this->markovTextMax);
			}
		}
		else if(this->isRunning())
			this->log(1, "WARNING: Created text was empty.");

		// sleep if necessary
		if(this->markovTextSleep) {
			this->setStatusMessage("Sleeping...");

			this->sleep(this->markovTextSleep);
		}
	}

	// pause algorithm run
	void MarkovText::onAlgoPause() {}

	// unpause algorithm run
	void MarkovText::onAlgoUnpause() {}

	// clear algorithm run
	void MarkovText::onAlgoClear() {}

	// parse configuration option
	void MarkovText::parseAlgoOption() {
		// algorithm options
		this->category("markov-text");
		this->option("dimension", this->markovTextDimension);
		this->option("length", this->markovTextLength);
		this->option("max", this->markovTextMax);
		this->option("result.field", this->markovTextResultField, StringParsingOption::SQL);
		this->option("sleep", this->markovTextSleep);
		this->option("sources.field", this->markovTextSourcesField, StringParsingOption::SQL);
		this->option("timing", this->markovTextTiming);
	}

	// check configuration, throws Thread::Exception
	void MarkovText::checkAlgoOptions() {
		// algorithm options
		if(this->config.generalInputFields.empty())
			throw Exception("Algo::MarkovText::checkOptions(): No input sources provided");

		if(this->config.generalResultTable.empty())
			throw Exception("Algo::MarkovText::checkOptions(): No result table specified");

		if(!(this->markovTextDimension))
			throw Exception("Algo::MarkovText::checkOptions(): Markov chain dimension is zero");

		if(!(this->markovTextLength))
			throw Exception("Algo::MarkovText::checkOptions(): Result text length is zero");

		// check your sources
		this->database.checkSources(
				this->config.generalInputSources,
				this->config.generalInputTables,
				this->config.generalInputFields
		);
	}

	// create dictionary (code mostly from https://rosettacode.org/wiki/Markov_chain_text_generator)
	void MarkovText::createDictionary() {
		// *** added: get dimension from configuration + counter
		const unsigned int kl = this->markovTextDimension;
		unsigned int counter = 0;
		// ***

		std::string w1, key;

		size_t wc = 0, pos = 0, next = 0;

		next = this->source.find_first_not_of( 32, 0 );

		if(next == std::string::npos)
			return;

		while(wc < kl) {
			pos = this->source.find_first_of( ' ', next );
			w1 = this->source.substr( next, pos - next );

			key += w1 + " ";

			next = this->source.find_first_not_of( 32, pos + 1 );

			if(next == std::string::npos)
				return;

			++wc;
		}

		key = key.substr(0, key.size() - 1);

		while( true ) {
			next = this->source.find_first_not_of(32, pos + 1);

			if(next == std::string::npos)
				return;

			pos = this->source.find_first_of( 32, next );

			// *** added: safer no more spaces check
			if(pos == std::string::npos)
				pos = this->source.length();
			// ***

			w1 = this->source.substr(next, pos - next);

			if(w1.empty())
				break;

			if(
					std::find(
							dictionary[key].begin(),
							dictionary[key].end(),
							w1
					) == dictionary[key].end()
			)
				dictionary[key].emplace_back(w1);

			key = key.substr(key.find_first_of(32) + 1) + " " + w1;

			// *** added: counter + check whether thread is still running + set progress
			++counter;

			if(counter > 1000000) {
				if(!(this->isRunning()))
					return;

				this->setProgress(static_cast<float>(next) / this->source.length());

				counter = 0;
			}
			// ***
		}
	}

	// create text (code mostly from https://rosettacode.org/wiki/Markov_chain_text_generator),
	//  throws Thread::Exception
	std::string MarkovText::createText() {
		if(!dictionary.size())
			throw Exception("Dictionary is empty");

		std::string key, first, second, result;
		size_t next = 0;
		std::map<std::string, std::vector<std::string> >::iterator it = dictionary.begin();

		result.reserve(this->markovTextLength * 10); // guess average word length

		int w = this->markovTextLength - this->markovTextDimension;

		std::advance(it, rand() % dictionary.size());

		key = it->first;

		result += key;

		while(true) {
			std::vector<std::string> d = dictionary[key];

			if(d.empty())
				break;

			second = d[rand() % d.size()];

			if(second.empty())
				break;

			result.push_back(' ');

			result += second;

			if(--w < 0)
				break;

			next = key.find_first_of(32, 0);
			first = key.substr(next + 1);
			key = first + " " + second;

			if(!(this->isRunning()))
				return "";
		}

		return result;
	}

} /* crawlservpp::Module::Analyzer::Algo */
