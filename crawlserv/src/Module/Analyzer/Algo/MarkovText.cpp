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
 * MarkovText.cpp
 *
 *  Created on: Jan 13, 2019
 *      Author: ans
 */

#include "MarkovText.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	MarkovText::MarkovText(
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
	MarkovText::MarkovText(Main::Database& dbBase,
							const ThreadOptions& threadOptions)
								:	Module::Analyzer::Thread(dbBase, threadOptions) {
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
	 */
	void MarkovText::onAlgoInit() {
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

		fields.emplace_back(this->markovTextResultField);
		fields.emplace_back(this->markovTextSourcesField);
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
					},
					[this]() {
						return this->isRunning();
					}
			);

			this->database.getCorpus(
					CorpusProperties(
							this->config.generalInputSources.at(n),
							this->config.generalInputTables.at(n),
							this->config.generalInputFields.at(n),
							this->config.tokenizerSentenceManipulators,
							this->config.tokenizerSentenceModels,
							this->config.tokenizerWordManipulators,
							this->config.tokenizerWordModels,
							this->config.tokenizerSavePoints,
							this->config.tokenizerFreeMemoryEvery
					),
					dateFrom,
					dateTo,
					corpus,
					corpusSources,
					statusSetter
			);

			this->sources += corpusSources;
			this->source += corpus.getcCorpus();

			this->source.push_back(' ');
		}

		if(!(this->source.empty())) {
			this->source.pop_back();
		}

		// create dictionary
		this->setStatusMessage("Creating dictionary...");

		this->log(generalLoggingVerbose, "creates dictionary...");

		std::unique_ptr<Timer::Simple> timer;

		if(this->markovTextTiming) {
			timer = std::make_unique<Timer::Simple>();
		}

		this->createDictionary();

		if(this->isRunning()) {
			if(timer) {
				this->log(
						generalLoggingDefault,
						"created dictionary in "
						+ timer->tickStr()
						+ "."
				);
			}

			// re-allow pausing the thread
			this->allowPausing();
		}

		// algorithm is ready
		this->log(generalLoggingExtended, "is ready.");
	}

	//! Generates the requested texts.
	void MarkovText::onAlgoTick() {
		// check number of texts (internally saved as "last") if necessary
		if(
				this->markovTextMax > 0
				&& this->getLast() >= this->markovTextMax
		) {
			this->finished();

			return;
		}

		// generate text
		this->setStatusMessage("Generating text...");

		std::unique_ptr<Timer::Simple> timer;

		if(this->markovTextTiming) {
			timer = std::make_unique<Timer::Simple>();
		}

		const std::string text(this->createText());

		if(timer) {
			this->log(
					generalLoggingDefault,
					"created text in "
					+ timer->tickStr()
					+ "."
			);
		}

		// insert text into target table
		if(!text.empty()) {
			Data::InsertFieldsMixed data;

			data.columns_types_values.reserve(2);

			data.table = "crawlserv_" + this->websiteNamespace + "_"
					+ this->urlListNamespace + "_analyzed_" + this->config.generalTargetTable;

			data.columns_types_values.emplace_back(
					"analyzed__" + this->markovTextResultField,
					Data::Type::_string,
					Data::Value(text)
			);

			data.columns_types_values.emplace_back(
					"analyzed__" + this->markovTextSourcesField,
					Data::getTypeOfSizeT(),
					Data::Value(this->sources)
			);

			this->database.insertCustomData(data);

			// increase text count and progress (internally saved as "last") if necessary
			if(this->markovTextMax > 0) {
				this->incrementLast();

				this->setProgress(static_cast<float>(this->getLast()) / this->markovTextMax);
			}
		}
		else if(this->isRunning()) {
			this->log(1, "WARNING: Created text was empty.");
		}

		// sleep if necessary
		if(this->markovTextSleep > 0) {
			this->setStatusMessage("Sleeping...");

			this->sleep(this->markovTextSleep);
		}
	}

	//! Does nothing.
	void MarkovText::onAlgoPause() {}

	//! Does nothing.
	void MarkovText::onAlgoUnpause() {}

	//! Does nothing.
	void MarkovText::onAlgoClear() {}

	/*
	 * IMPLEMENTED CONFIGURATION FUNCTIONS
	 */

	//! Parses a configuration option for the algorithm.
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

	//! Checks the configuration options for the algorithm.
	/*!
	 * \throws Module::Analyzer::Thread::Exception
	 *   if no input sources or no target table
	 *   are provided, if the given dimension
	 *   parameter is zero, or the length of the
	 *   texts to generate is zero.
	 */
	void MarkovText::checkAlgoOptions() {
		// algorithm options
		if(this->config.generalInputFields.empty()) {
			throw Exception(
					"Algo::MarkovText::checkOptions():"
					" No input sources have been provided"
			);
		}

		if(this->config.generalTargetTable.empty()) {
			throw Exception(
					"Algo::MarkovText::checkOptions():"
					" No target table has been specified"
			);
		}

		if(this->markovTextDimension == 0) {
			throw Exception(
					"Algo::MarkovText::checkOptions():"
					" Markov chain dimension is zero"
			);
		}

		if(this->markovTextLength == 0) {
			throw Exception(
					"Algo::MarkovText::checkOptions():"
					" Result text length is zero"
			);
		}

		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

	/*
	 * ALGORITHM FUNCTIONS (private)
	 */

	// create dictionary (code mostly from https://rosettacode.org/wiki/Markov_chain_text_generator)
	void MarkovText::createDictionary() {
		// *** added: get dimension from configuration + counter
		const auto kl{this->markovTextDimension};
		std::uint32_t counter{0};
		// ***

		std::string w1;
		std::string key;

		std::uint64_t wc{0};
		std::uint64_t pos{0};
		std::uint64_t next{0};

		next = this->source.find_first_not_of(markovTextAsciiSpace, 0);

		if(next == std::string::npos) {
			return;
		}

		while(wc < kl) {
			pos = this->source.find_first_of(' ', next);
			w1 = this->source.substr(next, pos - next);

			key += w1 + " ";

			next = this->source.find_first_not_of(markovTextAsciiSpace, pos + 1);

			if(next == std::string::npos) {
				return;
			}

			++wc;
		}

		key = key.substr(0, key.size() - 1);

		while( true ) {
			next = this->source.find_first_not_of(markovTextAsciiSpace, pos + 1);

			if(next == std::string::npos) {
				return;
			}

			pos = this->source.find_first_of(markovTextAsciiSpace, next);

			// *** added: safer no more spaces check
			if(pos == std::string::npos) {
				pos = this->source.length();
			}
			// ***

			w1 = this->source.substr(next, pos - next);

			if(w1.empty()) {
				break;
			}

			if(
					std::find(
							dictionary[key].cbegin(),
							dictionary[key].cend(),
							w1
					) == dictionary[key].cend()
			) {
				dictionary[key].emplace_back(w1);
			}

			key = key.substr(key.find_first_of(markovTextAsciiSpace) + 1);
			key += " ";
			key += w1;

			// *** added: counter + check whether thread is still running + set progress
			++counter;

			if(counter > markovTextRefreshProgressEvery) {
				if(!(this->isRunning())) {
					return;
				}

				this->setProgress(
						static_cast<float>(next)
						/ this->source.length()
				);

				counter = 0;
			}
			// ***
		}
	}

	// create text (code mostly from https://rosettacode.org/wiki/Markov_chain_text_generator),
	//  throws Thread::Exception
	std::string MarkovText::createText() {
		if(dictionary.empty()) {
			throw Exception("Dictionary is empty");
		}

		std::string key;
		std::string first;
		std::string second;
		std::string result;
		std::size_t next{0};
		auto it{dictionary.cbegin()};

		result.reserve(this->markovTextLength * markovTextGuessedWordLength); // guess average word length

		int w{static_cast<int>(this->markovTextLength - this->markovTextDimension)};

		std::advance(it, this->randGenerator() % dictionary.size());

		key = it->first;

		result += key;

		while(true) {
			std::vector<std::string> d{dictionary[key]};

			if(d.empty()) {
				break;
			}

			second = d[this->randGenerator() % d.size()];

			if(second.empty()) {
				break;
			}

			result.push_back(' ');

			result += second;

			if(--w < 0) {
				break;
			}

			next = key.find_first_of(markovTextAsciiSpace, 0);
			first = key.substr(next + 1);

			key = first;
			key += " ";
			key += second;

			if(!(this->isRunning())) {
				return "";
			}
		}

		return result;
	}

} /* namespace crawlservpp::Module::Analyzer::Algo */
