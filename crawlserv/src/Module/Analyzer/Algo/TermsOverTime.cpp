/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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
 * TermsOverTime.cpp
 *
 * Count occurrences of specific terms in a text corpus over time.
 *
 *  Created on: Aug 2, 2020
 *      Author: ans
 */

#include "TermsOverTime.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	TermsOverTime::TermsOverTime(
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
	TermsOverTime::TermsOverTime(
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
	std::string_view TermsOverTime::getName() const {
		return "TermsOverTime";
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
	void TermsOverTime::onAlgoInitTarget() {
		//TODO: initialize target table
	}

	//! Generates the corpus.
	/*!
	 * \note When this function is called, both the
	 *   prepared SQL statements, and the queries have
	 *   already been initialized.
	 *
	 * \throws TermsOverTime::Exception if the corpus
	 *   is empty.
	 */
	void TermsOverTime::onAlgoInit() {
		//TODO
		throw Exception("This algorithm is not implemented yet.");

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

		if(!(this->addCorpora(true, statusSetter))) {
			if(this->isRunning()) {
				throw Exception("TermsOverTime::onAlgoInit(): Corpus is empty");
			}

			return;
		}

		// algorithm is ready
		this->log(generalLoggingExtended, "is ready.");

		/*
		 * NOTE: Do not set any thread status here, as the parent class
		 *        will revert to the original thread status after initialization.
		 */
	}

	//! Counts the terms in the text corpus.
	/*!
	 * One corpus will be processed in each tick.
	 *
	 * \note The corpus has already been genererated
	 *   on initialization.
	 *
	 * \throws TermsOverTime::Exception if the
	 *   corpus has no date map.
	 *
	 * \sa onAlgoInit
	 *
	 * \todo Not implemented yet.
	 */
	void TermsOverTime::onAlgoTick() {
		if(this->firstTick) {
			this->count();

			this->firstTick = false;

			return;
		}

		this->save();
		this->finished();
	}

	//! Does nothing.
	void TermsOverTime::onAlgoPause() {}

	//! Does nothing.
	void TermsOverTime::onAlgoUnpause() {}

	//! Does nothing.
	void TermsOverTime::onAlgoClear() {}

	/*
	 * IMPLEMENTED CONFIGURATION FUNCTIONS
	 */

	//! Parses a configuration option for the algorithm.
	void TermsOverTime::parseAlgoOption() {
		// algorithm options
		this->category("terms");

		//TODO: add algo options
		//this->option("[...]", this->algoConfig.[...]);
	}

	//! Does nothing.
	void TermsOverTime::checkAlgoOptions() {
		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

	//! Resets the algorithm.
	void TermsOverTime::resetAlgo() {
		this->algoConfig = {};

		this->firstTick = true;

		Helper::Memory::free(this->dateCounts);
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// count terms
	void TermsOverTime::count() {
		this->setStatusMessage("Counting occurrences...");
		this->setProgress(0.F);

		this->log(generalLoggingDefault, "counts occurrences...");

		const auto& corpus{this->corpora[0]};

		if(!corpus.hasDateMap()) {
			throw Exception(
					"TermsOverTime::count():"
					" Corpus has no date map"
			);
		}

		if(corpus.hasArticleMap()) {
			// TODO: count terms per article and date
		}
		else {
			// TODO: count terms per date only
		}
	}

	// save term counts to database
	void TermsOverTime::save() {
		//TODO: save result
	}

} /* namespace crawlservpp::Module::Analyzer::Algo */
