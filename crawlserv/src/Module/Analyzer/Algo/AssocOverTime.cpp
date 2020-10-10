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
 * AssocOverTime.cpp
 *
 * Algorithm counting associations between the keyword and
 *  different categories over time.
 *
 *  Created on: Oct 10, 2020
 *      Author: ans
 */

#include "AssocOverTime.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	AssocOverTime::AssocOverTime(
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
	AssocOverTime::AssocOverTime(Main::Database& dbBase,
							const ThreadOptions& threadOptions)
								:	Module::Analyzer::Thread(dbBase, threadOptions) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	/*
	 * IMPLEMENTED ALGORITHM FUNCTIONS
	 */

	//! Generates the corpus.
	void AssocOverTime::onAlgoInit() {
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

		std::size_t bytes{0};
		std::size_t sources{0};

		for(std::size_t n{0}; n < this->config.generalInputSources.size(); ++n) {
			std::size_t corpusSources{0};

			this->corpora.emplace_back(this->config.generalCorpusChecks);

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
							this->config.generalInputFields.at(n),
							this->config.tokenizerSentenceManipulators,
							this->config.tokenizerSentenceModels,
							this->config.tokenizerWordManipulators,
							this->config.tokenizerWordModels,
							this->config.tokenizerSavePoints,
							this->config.tokenizerFreeMemoryEvery
					),
					this->config.filterDateEnable ? this->config.filterDateFrom : std::string(),
					this->config.filterDateEnable ? this->config.filterDateTo : std::string(),
					this->corpora.back(),
					corpusSources,
					statusSetter
			);

			if(this->corpora.back().empty()) {
				this->corpora.pop_back();
			}

			bytes += this->corpora.back().size();

			sources += corpusSources;
		}

		// algorithm is ready
		this->log(generalLoggingExtended, "is ready.");

		this->setStatusMessage("Calculating associations...");
	}

	//! Calculates the associations in the text corpus.
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
	void AssocOverTime::onAlgoTick() {
		if(this->currentCorpus < this->corpora.size()) {
			this->addCurrent();

			++(this->currentCorpus);
		}
		else {
			this->saveAssociations();

			// sleep forever (i.e. until the thread is terminated)
			this->finished();
			this->sleep(std::numeric_limits<std::uint64_t>::max());
		}
	}

	//! Does nothing.
	void AssocOverTime::onAlgoPause() {}

	//! Does nothing.
	void AssocOverTime::onAlgoUnpause() {}

	//! Does nothing.
	void AssocOverTime::onAlgoClear() {}

	/*
	 * IMPLEMENTED CONFIGURATION FUNCTIONS
	 */

	//! Parses a configuration option for the algorithm.
	void AssocOverTime::parseAlgoOption() {
		// algorithm options
		this->category("associations");
		this->option("categories", this->categoryQueries);
		this->option("keyword", this->keyWordQuery);
		this->option("window.size", this->windowSize);
	}

	//! Checks the configuration options for the algorithm.
	/*!
	 * \throws Module::Analyzer::Thread::Exception
	 *   if no keyword or no category has been defined.
	 */
	void AssocOverTime::checkAlgoOptions() {
		if(this->keyWordQuery == 0) {
			throw Exception("No keyword defined");
		}

		if(
				this->categoryQueries.empty()
				|| std::find_if(
						this->categoryQueries.begin(),
						this->categoryQueries.end(),
						[](const auto query) {
							return query > 0;
						}
				) == this->categoryQueries.end()) {
			throw Exception("No category defined");
		}

		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

	/*
	 * ALGORITHM FUNCTIONS (private)
	 */

	// add terms and categories from current corpus
	void AssocOverTime::addCurrent() {
		const auto& corpus = this->corpora[this->currentCorpus];
		const auto& dateMap = corpus.getcDateMap();
		const auto& articleMap = corpus.getcArticleMap();
		const auto& tokens = corpus.getcTokens();
	}

	// calculate and save associations
	void AssocOverTime::saveAssociations() {
		//TODO
	}

	/*
	 * QUERY FUNCTIONS (private)
	 */

	// initialize algorithm-specific queries
	void AssocOverTime::initQueries() {
		this->addQueries(this->categoryQueries, this->queriesCategories);
		this->addOptionalQuery(this->keyWordQuery, this->queryKeyWord);
	}

	// add optional query
	void AssocOverTime::addOptionalQuery(std::uint64_t queryId, QueryStruct& propertiesTo) {
		if(queryId > 0) {
			QueryProperties properties;

			this->database.getQueryProperties(queryId, properties);

			propertiesTo = this->addQuery(properties);
		}
	}

	// add multiple queries at once, ignoring empty ones
	void AssocOverTime::addQueries(
			const std::vector<std::uint64_t>& queryIds,
			std::vector<QueryStruct>& propertiesTo
	) {
		// reserve memory first
		propertiesTo.reserve(queryIds.size());

		for(const auto& queryId : queryIds) {
			if(queryId > 0) {
				QueryProperties properties;

				this->database.getQueryProperties(queryId, properties);

				propertiesTo.emplace_back(this->addQuery(properties));
			}
		}
	}

} /* namespace crawlservpp::Module::Analyzer::Algo */
