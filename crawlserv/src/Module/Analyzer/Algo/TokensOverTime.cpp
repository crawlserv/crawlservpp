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
 * TokensOverTime.cpp
 *
 * Count occurrences of specific tokens in a text corpus over time.
 *
 *  Created on: Aug 2, 2020
 *      Author: ans
 */

#include "TokensOverTime.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	TokensOverTime::TokensOverTime(
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
	TokensOverTime::TokensOverTime(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions
	) : Module::Analyzer::Thread(dbBase, threadOptions) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	/*
	 * IMPLEMENTED GETTER
	 */

	//! Returns the name of the algorithm.
	/*!
	 * \returns A string view containing the
		 *   name of the implemented algorithm.
	 */
	std::string_view TokensOverTime::getName() const {
		return "TokensOverTime";
	}

	/*
	 * IMPLEMENTED ALGORITHM FUNCTIONS
	 */

	//! Initializes the target table for the algorithm.
	/*!
	 * \note When this function is called, neither the
	 *   prepared SQL statements, nor the queries have
	 *   been initialized yet.
	 */
	void TokensOverTime::onAlgoInitTarget() {
		//TODO: initialize target table
	}

	//! Generates the corpus.
	/*!
	 * \note When this function is called, both the
	 *   prepared SQL statements, and the queries have
	 *   already been initialized.
	 */
	void TokensOverTime::onAlgoInit() {
		StatusSetter statusSetter(
				"Initializing algorithm...",
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

		// check your sources
		this->log(generalLoggingVerbose, "checks sources...");

		this->checkCorpusSources(statusSetter);

		// request text corpus
		this->log(generalLoggingDefault, "gets text corpus...");

		for(std::size_t index{}; index < this->config.generalInputSources.size(); ++index) {
			this->addCorpus(index, statusSetter);
		}

		// algorithm is ready
		this->log(generalLoggingExtended, "is ready.");

		/*
		 * NOTE: Do not set any thread status here, as the parent class
		 *        will revert to the original thread status after initialization.
		 */
	}

	//! Counts the tokens in the text corpus.
	/*!
	 * One corpus will be processed in each tick.
	 *
	 * \note The corpus has already been genererated
	 *   on initialization.
	 *
	 * \sa onAlgoInit
	 *
	 * \todo Not implemented yet.
	 */
	void TokensOverTime::onAlgoTick() {
		if(this->currentCorpus < this->corpora.size()) {
			// set status message and reset progress
			std::string status{"occurrences"};

			if(this->corpora.size() > 1) {
				status += " in corpus #";
				status += std::to_string(this->currentCorpus + 1);
				status += "/";
				status += std::to_string(this->corpora.size());
			}

			status += "...";

			this->setStatusMessage("Counting " + status);
			this->setProgress(0.F);

			this->log(generalLoggingDefault, "counts " + status);

			const auto& corpus{this->corpora[this->currentCorpus]};

			++(this->currentCorpus);

			if(!corpus.hasDateMap()) {
				this->log(
						generalLoggingDefault,
						"WARNING: Corpus #"
						+ std::to_string(this->currentCorpus + 1)
						+ " does not have a date map and has been skipped."
				);

				return;
			}

			//TODO count occurrences (-> private member function)

			if(corpus.hasArticleMap()) {
				// count tokens per article and date
			}
			else {
				// count tokens per date only
			}
		}
		else {
			//TODO save result (-> private member function)

			this->finished();

			// sleep forever (i.e. until the thread is terminated)
			this->sleep(std::numeric_limits<std::uint64_t>::max());
		}
	}

	//! Does nothing.
	void TokensOverTime::onAlgoPause() {}

	//! Does nothing.
	void TokensOverTime::onAlgoUnpause() {}

	//! Does nothing.
	void TokensOverTime::onAlgoClear() {}

	/*
	 * IMPLEMENTED CONFIGURATION FUNCTIONS
	 */

	//! Parses a configuration option for the algorithm.
	void TokensOverTime::parseAlgoOption() {
		// algorithm options
		this->category("tokens");

		//TODO: add algo options
		//this->option("[...]", this->algoConfig.[...]);
	}

	//! Does nothing.
	void TokensOverTime::checkAlgoOptions() {
		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

	//! Resets the configuration options for the algorithm.
	void TokensOverTime::resetAlgo() {
		this->algoConfig = {};
	}

} /* namespace crawlservpp::Module::Analyzer::Algo */
