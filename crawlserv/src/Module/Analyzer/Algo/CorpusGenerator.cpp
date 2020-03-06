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
 *   This algorithm just uses the built-in functionality for building text corpora from its input data and quits.
 *
 *
 *  Created on: Mar 5, 2020
 *      Author: ans
 */

#include "CorpusGenerator.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	// constructor A: run previously interrupted algorithm run
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

	// constructor B: start a new algorithm run
	CorpusGenerator::CorpusGenerator(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions
	) : Module::Analyzer::Thread(dbBase, threadOptions) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	// destructor stub
	CorpusGenerator::~CorpusGenerator() {}

	// initialize algorithm run
	void CorpusGenerator::onAlgoInit() {
		// reset progress
		this->setProgress(0.f);

		// check your sources
		this->setStatusMessage("Checking sources...");

		this->log(Config::generalLoggingVerbose, "checks sources...");

		this->database.checkSources(
				this->config.generalInputSources,
				this->config.generalInputTables,
				this->config.generalInputFields
		);

		// request text corpus
		this->setStatusMessage("Requesting text corpus...");

		this->log(Config::generalLoggingVerbose, "requests text corpus...");

		size_t corpora = 0;
		size_t bytes = 0;
		size_t sources = 0;

		for(unsigned long n = 0; n < this->config.generalInputSources.size(); ++n) {
			unsigned long corpusSources = 0;

			Data::Corpus corpus;

			this->database.getCorpus(
					CorpusProperties(
							this->config.generalInputSources.at(n),
							this->config.generalInputTables.at(n),
							this->config.generalInputFields.at(n)
					),
					std::string(),
					std::string(),
					corpus,
					corpusSources
			);

			if(!corpus.empty()) {
				++corpora;

				bytes += corpus.size();

				sources += corpusSources;
			}
		}

		// algorithm has finished
		this->log(Config::generalLoggingExtended, "has finished.");

		this->setProgress(1.f);

		/*
		 * NOTE: The status will be saved in-class and not set here, because
		 * 	the parent class will revert to the original status after initialization
		 */

		std::ostringstream statusStrStr;

		statusStrStr.imbue(std::locale(""));

		if(corpora) {
			statusStrStr << "IDLE ";

			if(corpora == 1)
				statusStrStr << "Corpus of ";
			else
				statusStrStr << corpora << " corpora of ";

			if(bytes == 1)
				statusStrStr << "one byte";
			else
				statusStrStr << bytes << " bytes";

			if(sources == 1)
				statusStrStr << " from one source";
			else if(sources > 1)
				statusStrStr << " from " << sources << " sources";

			this->status = statusStrStr.str();
		}
		else
			this->status = "IDLE No corpus created.";

	}

	// algorithm tick
	void CorpusGenerator::onAlgoTick() {
		this->setStatusMessage(this->status);

		this->sleep(std::numeric_limits<unsigned long>::max());
	}

	// pause algorithm run
	void CorpusGenerator::onAlgoPause() {}

	// unpause algorithm run
	void CorpusGenerator::onAlgoUnpause() {}

	// clear algorithm run
	void CorpusGenerator::onAlgoClear() {}

	// parse configuration option
	void CorpusGenerator::parseAlgoOption() {}

	// check configuration, throws Thread::Exception
	void CorpusGenerator::checkAlgoOptions() {
		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

} /* crawlservpp::Module::Analyzer::Algo */
