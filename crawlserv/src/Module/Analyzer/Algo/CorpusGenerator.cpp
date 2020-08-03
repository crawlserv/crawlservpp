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
 * CorpusGenerator.cpp
 *
 * This algorithm just uses the built-in functionality for building text corpora from its input data and quits.
 *
 *
 *  Created on: Mar 5, 2020
 *      Author: ans
 */

#include "CorpusGenerator.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	CorpusGenerator::CorpusGenerator(
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
	CorpusGenerator::CorpusGenerator(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions
	) : Module::Analyzer::Thread(dbBase, threadOptions) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	/*
	 * IMPLEMENTED ALGORITHM FUNCTIONS
	 */

	//! Generates the corpus.
	void CorpusGenerator::onAlgoInit() {
		// reset progress
		this->setProgress(0.F);

		// check your sources
		this->setStatusMessage("Checking sources...");

		this->log(generalLoggingVerbose, "checks sources...");

		this->database.checkSources(
				this->config.generalInputSources,
				this->config.generalInputTables,
				this->config.generalInputFields
		);

		// request text corpus
		this->log(generalLoggingVerbose, "gets text corpus...");

		std::size_t corpora{0};
		std::size_t bytes{0};
		std::size_t sources{0};

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
				std::ostringstream corpusStatusStrStr;

				corpusStatusStrStr.imbue(std::locale(""));

				corpusStatusStrStr << "Getting text corpus #";
				corpusStatusStrStr << n + 1;
				corpusStatusStrStr << "/";
				corpusStatusStrStr << this->config.generalInputSources.size();
				corpusStatusStrStr << "...";

				statusStr = corpusStatusStrStr.str();
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
							this->config.generalInputFields.at(n)
					),
					dateFrom,
					dateTo,
					corpus,
					corpusSources,
					statusSetter
			);

			if(!corpus.empty()) {
				++corpora;

				bytes += corpus.size();

				sources += corpusSources;
			}
		}

		// algorithm has finished
		this->log(generalLoggingExtended, "has finished.");

		/*
		 * NOTE: The status will be saved in-class and not set here, because
		 * 	the parent class will revert to the original status after initialization
		 */

		std::ostringstream statusStrStr;

		statusStrStr.imbue(std::locale(""));

		if(corpora > 0) {
			statusStrStr << "IDLE ";

			if(corpora == 1) {
				statusStrStr << "Corpus of ";
			}
			else {
				statusStrStr << corpora << " corpora of ";
			}

			if(bytes == 1) {
				statusStrStr << "one byte";
			}
			else {
				statusStrStr << bytes << " bytes";
			}

			if(sources == 1) {
				statusStrStr << " from one source";
			}
			else if(sources > 1) {
				statusStrStr << " from " << sources << " sources";
			}

			this->status = statusStrStr.str();
		}
		else {
			this->status = "IDLE No corpus created.";
		}

	}

	//! Sleeps until the thread is terminated.
	/*!
	 * The corpus has already been genererated on
	 *  initialization.
	 *
	 * \sa onAlgoInit
	 */
	void CorpusGenerator::onAlgoTick() {
		this->setStatusMessage(this->status);

		// sleep forever (i.e. until the thread is terminated)
		this->sleep(std::numeric_limits<std::uint64_t>::max());
	}

	//! Does nothing.
	void CorpusGenerator::onAlgoPause() {}

	//! Does nothing.
	void CorpusGenerator::onAlgoUnpause() {}

	//! Does nothing.
	void CorpusGenerator::onAlgoClear() {}

	/*
	 * IMPLEMENTED CONFIGURATION FUNCTIONS
	 */

	//! Does nothing.
	void CorpusGenerator::parseAlgoOption() {}

	//! Does nothing.
	void CorpusGenerator::checkAlgoOptions() {
		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

} /* namespace crawlservpp::Module::Analyzer::Algo */
