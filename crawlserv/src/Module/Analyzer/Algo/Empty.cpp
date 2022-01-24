/*
 *
 * ---
 *
 *  Copyright (C) 2022 Anselm Schmidt (ans[ät]ohai.su)
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
 * TODO: change name, date and description of the file
 *
 *  Created on: Feb 5, 2021
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
//		this->disallowPausing(); // disallow pausing while initializing
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
//		this->disallowPausing(); // disallow pausing while initializing
	}

	/*
	 * IMPLEMENTED GETTER
	 */

	//! Returns the name of the algorithm.
	/*!
	 * \returns A string view containing the
	 *   name of the implemented algorithm.
	 */
	std::string_view Empty::getName() const {
		/*
		 * TODO: Insert the name of the algorithm.
		 */
		return "Empty";
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
	void Empty::onAlgoInitTarget() {
		/*
		 * TODO: [OPTIONAL] set target fields
		 */

		// set target fields
//		std::vector<StringString> fields;
//
//		fields.reserve(...);
//
//		fields.emplace_back("[NAME]", "[TYPE]");
//		...

		/*
		 * TODO: [OPTIONAL] initialize target table
		 */

		// initialize target table
//		this->database.setTargetFields(fields);
//
//		this->database.initTargetTable(...);

	}

	//! Initializes the algorithm and processes its input.
	/*!
	 * \note When this function is called, both the
	 *   prepared SQL statements, and the queries have
	 *   already been initialized.
	 *
	 * TODO: change information about corpus exception
	 * \throws <algo>::Exception if the corpus is empty
	 *   / no non-empty corpus has been added.
	 *
	 * \sa initQueries
	 */
	void Empty::onAlgoInit() {
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

		/*
		 * TODO: [OPTIONAL] check sources
		 */

		// check your sources
//		this->log(generalLoggingVerbose, "checks sources...");
//
//		this->checkCorpusSources(statusSetter);

		/*
		 * TODO: [OPTIONAL] get filtered and combined text corpus or separate corpora
		 */

		// request text corpus
//		this->log(generalLoggingDefault, "gets text corpus...");
//
//		if(!(this->addCorpora(/*this->algoConfig.combineSources*/ true, statusSetter))) {
//			if(this->isRunning()) {
//				throw Exception("<algo>::onAlgoInit(): Corpus is empty");
//				throw Exception(
//						"<algo>::onAlgoInit():"
//						" No non-empty corpus has been added."
//				);
//			}
//
//			return;
//		}

		/*
		 * TODO: initialize algorithm
		 */

		// algorithm is ready
		this->log(generalLoggingExtended, "is ready.");

		/*
		 * NOTE: Do not set any thread status here, as the parent class
		 *        will revert to the original thread status after initialization.
		 */
	}

	/*
	 * TODO: add description to the function.
	 */
	//! Does nothing.
	void Empty::onAlgoTick() {
		/*
		 * TODO: [OPTIONAL] set new status
		 */

		/*
		 * TODO: algorithm tick
		 */
//		if(this->firstTick) {
//		    /* first tick first status */
//			this->setStatusMessage("...");
//
//			this->firstTick = false;
//
//			return;
//		}

		/*
		 * TODO: [OPTIONAL] insert data into target table
		 */
//		Data::InsertFieldsMixed data;
//
//		data.columns_types_values.reserve(...);
//
//		data.table = this->getTargetTableName();
//
//		data.columns_types_values.emplace_back(
//				"analyzed__" + ...,
//				Data::Type::...,
//				Data::Value(...)
//		);
//
//		...
//
//		this->database.insertCustomData(data);
//
//		this->database.updateTargetTable();

		/*
		 * TODO: [OPTIONAL] update progress
		 */

		//  update progress
//		this->setProgress(static_cast<float>(this->getLast()) / [TOTAL]);
	}

	/*
	 * TODO: add description to the function, if necessary.
	 */
	//! Does nothing.
	void Empty::onAlgoPause() {
		/*
		 * TODO: [OPTIONAL] pause algorithm
		 */
	}

	/*
	 * TODO: add description to the function, if necessary.
	 */
	//! Does nothing.
	void Empty::onAlgoUnpause() {
		/*
		 * TODO: [OPTIONAL] unpause algorithm
		 */
	}

	//! Resets the state of the algorithm.
	void Empty::onAlgoClear() {
		/*
		 * TODO: reset all variables used for the algorithm state
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
//		this->category(..);
//		this->option(name, member variable);
//		...
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

	//! Resets the configuration options for the algorithm.
	void Empty::resetAlgo() {
		this->algoConfig = {};

//		this->firstTick = true;
	}

	/*
	 * ALGORITHM FUNCTIONS (private)
	 */

	/*
	 * TODO: [OPTIONAL] add the implementations of internal algorithm functions here
	 */

	/*
	 * QUERY FUNCTIONS (private)
	 */

	/*
	 * TODO: [OPTIONAL] add query functionality
	 */

	/*
	// initialize algorithm-specific queries
	void Empty::initQueries() {
		//TODO: initialize queries

		// use this->addOptionalQuery(...) and/or
		// this->addQueries(..) to add queries,
		// if needed
	}

	// deletes algorithm-specific queries
	void Empty::deleteQueries() {
		//TODO: delete queries

		// empty vectors containing
		// QueryStructs and zero out
		// single QueryStructs in case
		// the algorithm is reset!
	}
	*/

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	/*
	 * TODO: [OPTIONAL] add internal helper functions
	 */

} /* namespace crawlservpp::Module::Analyzer::Algo */
