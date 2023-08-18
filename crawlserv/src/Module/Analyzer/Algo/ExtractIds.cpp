/*
 *
 * ---
 *
 *  Copyright (C) 2023 Anselm Schmidt (ans[ät]ohai.su)
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
 * ExtractIds.cpp
 *
 * Extracts the parsed IDs from a filtered corpus.
 *
 *  Created on: Jul 31, 2023
 *      Author: ans
 */

#include "ExtractIds.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	ExtractIds::ExtractIds(
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
	ExtractIds::ExtractIds(Main::Database& dbBase,
							const ThreadOptions& threadOptions)
								:	Module::Analyzer::Thread(dbBase, threadOptions) {
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
	std::string_view ExtractIds::getName() const {
		return "ExtractIds";
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
	void ExtractIds::onAlgoInitTarget() {
		// set target fields
		std::vector<StringString> fields;

		fields.emplace_back("id", "text");

		// initialize target table
		this->database.setTargetFields(fields);

		this->database.initTargetTable(true, true);
	}

	//! Generates the corpus.
	/*!
	 * \throws ExtractIds::Exception if the corpus
	 *   is empty.
	 */
	void ExtractIds::onAlgoInit() {
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
				throw Exception("ExtractIds::onAlgoInit(): Corpus is empty");
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

	//! Extracts IDs from corpus.
	/*!
	 * One corpus will be processed in each tick.
	 *
	 * \note The corpus has already been genererated
	 *   on initialization.
	 *
	 * \throws ExtractIds::Exception if the
	 *   corpus has no article map.
	 *
	 * \sa onAlgoInit
	 */
	void ExtractIds::onAlgoTick() {
		if(this->firstTick) {
			this->extract();

			this->firstTick = false;

			return;
		}

		this->save();
		this->finished();
	}

	//! Does nothing.
	void ExtractIds::onAlgoPause() {}

	//! Does nothing.
	void ExtractIds::onAlgoUnpause() {}

	//! Does nothing
	void ExtractIds::onAlgoClear() {}

	/*
	 * IMPLEMENTED CONFIGURATION FUNCTIONS
	 */

	//! Does nothing.
	void ExtractIds::parseAlgoOption() {}

	//! Does nothing.
	void ExtractIds::checkAlgoOptions() {
		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

	//! Resets the algorithm.
	void ExtractIds::resetAlgo() {
		this->firstTick = true;

		Helper::Memory::free(this->results);
	}

	/*
	 * ALGORITHM FUNCTIONS (private)
	 */

	// identify IDs
	void ExtractIds::extract() {
		std::size_t statusCounter{};
		std::size_t resultCounter{};

		// check for corpora
		if(this->corpora.empty()) {
			throw Exception(
					"ExtractIds::count():"
					" No corpus set"
			);
		}

		// set status message and reset progress
		this->setStatusMessage("Extracting IDs...");
		this->setProgress(0.F);

		this->log(generalLoggingDefault, "extracts IDs...");

		// set current corpus
		const auto& corpus = this->corpora.back();
		const auto& articleMap = corpus.getcArticleMap();

		// check article map
		if(articleMap.empty()) {
			throw Exception(
					"ExtractIds::extract():"
					" Corpus has no article map"
			);
		}

		this->log(generalLoggingVerbose, "loops through articles...");

		for(const auto& article : articleMap) {
			// add ID to results
			this->results.emplace(article.value);

			// update status if necessary
			++statusCounter;
			++resultCounter;

			if(statusCounter == extractIdsUpdateProgressEvery) {
				this->setProgress(
						static_cast<float>(resultCounter)
						/ articleMap.size()
				);

				statusCounter = 0;
			}

			if(!(this->isRunning())) {
				return;
			}
		}
	}

	// save counts to database
	void ExtractIds::save() {
		// update status and write to log
		this->setStatusMessage("Saving results...");
		this->setProgress(0.F);

		this->log(generalLoggingDefault, "saves results...");

		// get target table to store results in
		const auto targetTable{
			this->getTargetTableName()
		};

		// go through all results
		std::size_t statusCounter{};
		std::size_t resultCounter{};
		bool updated{false};

		for(const auto& result : this->results) {
			// insert actual data set
			this->insertDataSet(targetTable, result);

			// update status if necessary
			++statusCounter;
			++resultCounter;

			if(statusCounter == extractIdsUpdateProgressEvery) {
				this->setProgress(
						static_cast<float>(resultCounter)
						/ this->results.size()
				);

				statusCounter = 0;
			}

			updated = true;

			if(!(this->isRunning())) {
				break;
			}
		}

		if(updated) {
			// target table updated
			this->database.updateTargetTable();
		}
	}

	// insert dataset into the target table
	void ExtractIds::insertDataSet(const std::string& table, const std::string& result) {
		Data::InsertFieldsMixed data;

		data.table = table;

		data.columns_types_values.emplace_back(
				"analyzed__id",
				Data::Type::_string,
				Data::Value(result)
		);

		this->database.insertCustomData(data);
	}

} /* namespace crawlservpp::Module::Analyzer::Algo */
