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
 * Empty.cpp
 *
 * Empty template class for adding new algorithms to the application.
 *  Duplicate this (and the header) file to implement a new algorithm,
 *  then add it to 'All.cpp' in the same directory.
 *
 * TODO: change name and description of the file
 *
 *  Created on: Jul 23, 2020
 *      Author: ans
 */

/*
 * TODO: change name of the header file
 */
#include "Empty.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	/*
	 * TODO: change names of the constructors
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	Empty::Empty(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions,
			const ThreadStatus& threadStatus
	) : Module::Analyzer::Thread(
				dbBase,
				threadOptions,
				threadStatus
			) {
		/*
		 * TODO: [OPTIONAL] disallow pausing while initializing
		 */
		//this->disallowPausing();
	}

	//! Starts a new algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&)
	 */
	Empty::Empty(Main::Database& dbBase,
							const ThreadOptions& threadOptions)
								:	Module::Analyzer::Thread(dbBase, threadOptions) {
		/*
		 * TODO: [OPTIONAL] disallow pausing while initializing
		 */
		//this->disallowPausing();
	}

	/*
	 * IMPLEMENTED ALGORITHM FUNCTIONS
	 */

	//! Initializes the algorithm and processes its input.
	void Empty::onAlgoInit() {
		/*
		 * TODO: [OPTIONAL] check sources
		 */

		// check your sources
		/*
		this->setStatusMessage("Checking sources...");

		this->log(generalLoggingVerbose, "checks sources...");

		this->database.checkSources(
				this->config.generalInputSources,
				this->config.generalInputTables,
				this->config.generalInputFields
		);
		*/

		/*
		 * TODO: [OPTIONAL] set target fields
		 */

		// set target fields
		/*
		std::vector<std::string> fields;
		std::vector<std::string> types;

		fields.reserve(...);
		types.reserve(...);

		fields.emplace_back(...);
		...
		types.emplace_back(...);
		...

		this->database.setTargetFields(fields, types);
		*/

		/*
		 * TODO: [OPTIONAL] initialize target table
		 */

		// initialize target table
		/*
		this->setStatusMessage("Creating target table...");

		this->log(generalLoggingVerbose, "creates target table...");

		this->database.initTargetTable(...);
		*/

		/*
		 * TODO: [OPTIONAL] get text corpus, filtered by date
		 */

		// get text corpus
		/*
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
		*/

		/*
		 * TODO: initialize algorithm
		 */

		// algorithm is ready
		this->log(generalLoggingExtended, "is ready.");
	}

	/*
	 * TODO: add description to the function.
	 */
	//! Does nothing.
	void Empty::onAlgoTick() {
		/*
		 * TODO: algorithm tick
		 */

		/*
		 * TODO: [OPTIONAL] insert data into target table
		 */
		/*
		Data::InsertFieldsMixed data;

		data.columns_types_values.reserve(...);

		data.table = "crawlserv_" + this->websiteNamespace + "_"
				+ this->urlListNamespace + "_analyzed_" + this->config.generalResultTable;

		data.columns_types_values.emplace_back(
				"analyzed__" + ...,
				Data::Type::...,
				Data::Value(...)
		);

		...

		this->database.insertCustomData(data);
		*/

		/*
		 * TODO: [OPTIONAL] update progress
		 */

		/*
		//  update progress
		this->setProgress(static_cast<float>(this->getLast()) / this->markovTextMax);
		*/
	}

	/*
	 * TODO: add description to the function.
	 */
	//! Does nothing.
	void Empty::onAlgoPause() {
		/*
		 * TODO: [OPTIONAL] pause algorithm
		 */
	}

	/*
	 * TODO: add description to the function.
	 */
	//! Does nothing.
	void Empty::onAlgoUnpause() {
		/*
		 * TODO: [OPTIONAL] unpause algorithm
		 */
	}

	/*
	 * TODO: add description to the function.
	 */
	//! Does nothing.
	void Empty::onAlgoClear() {
		/*
		 * TODO: [OPTIONAL] clear algorithm
		 */
	}

	/*
	 * IMPLEMENTED CONFIGURATION FUNCTIONS
	 */

	//! Parses a configuration option for the algorithm.
	void Empty::parseAlgoOption() {
		/*
		 * TODO: add configuration categories and options
		 */
		// algorithm options
		//this->category(..);
		//this->option(name, member variable);
		//...
	}

	/*
	 * TODO: update description of the function
	 */
	//! Checks the configuration options for the algorithm.
	/*!
	 * \throws Module::Analyzer::Thread::Exception
	 *   if ...
	 */
	void Empty::checkAlgoOptions() {
		/*
		 * TODO: check configuration
		 */

		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

	/*
	 * ALGORITHM FUNCTIONS (private)
	 */

	/*
	 * TODO: [OPTIONAL] add the implementations of internal algorithm functions here
	 */

} /* namespace crawlservpp::Module::Analyzer::Algo */
