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

		// initialize queries
		this->initQueries();

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

		const auto numFields{
			this->categoryLabels.size()
			+ 2
		};

		fields.reserve(numFields);
		types.reserve(numFields);

		fields.emplace_back("date");
		fields.emplace_back("occurences");
		types.emplace_back("VARCHAR(10)");
		types.emplace_back("BIGINT UNSIGNED");

		for(const auto& label : this->categoryLabels) {
			fields.emplace_back(label);
			types.emplace_back("BIGINT UNSIGNED");
		}

		this->database.setTargetFields(fields, types);

		// initialize target table
		this->setStatusMessage("Creating target table...");

		this->log(generalLoggingVerbose, "creates target table...");

		this->database.initTargetTable(true);

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

			if(!(this->isRunning())) {
				return;
			}

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

			if(this->isRunning()) {
				this->sleep(std::numeric_limits<std::uint64_t>::max());
			}
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
		this->option("cat.labels", this->categoryLabels);
		this->option("cat.queries", this->categoryQueries);
		this->option("keyword", this->keyWordQuery);
		this->option("window.size", this->windowSize);
	}

	//! Checks the configuration options for the algorithm.
	/*!
	 * \throws Module::Analyzer::Thread::Exception
	 *   if no keyword or no category has been defined.
	 */
	void AssocOverTime::checkAlgoOptions() {
		// check keyword query
		if(this->keyWordQuery == 0) {
			throw Exception("No keyword defined");
		}

		// check categories
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

		const auto completeCategories{
			std::min( // number of complete categories (= min. size of all arrays)
					this->categoryLabels.size(),
					this->categoryQueries.size()
			)
		};

		bool incompleteCategories{false};

		// remove category labels or queries that are not used
		if(this->categoryLabels.size() > completeCategories) {
			this->categoryLabels.resize(completeCategories);

			incompleteCategories = true;
		}
		else if(this->categoryQueries.size() > completeCategories) {
			this->categoryQueries.resize(completeCategories);

			incompleteCategories = true;
		}

		if(incompleteCategories) {
			this->warning(
					"\'cat.labels\', \'.queries\'"
					" should have the same number of elements."
			);
		}

		// remove empty labels, invalid queries
		for(std::size_t index{0}; index < this->categoryLabels.size(); ++index) {
			if(
					this->categoryLabels[index].empty()
					|| this->categoryQueries[index] == 0
			) {
				this->categoryLabels.erase(this->categoryLabels.begin() + index);
				this->categoryQueries.erase(this->categoryQueries.begin() + index);

				incompleteCategories = true;
			}
		}

		// warn about incomplete categories
		if(incompleteCategories) {
			this->warning(
					"Incomplete categories removed from configuration."
			);
		}

		// check window size
		if(this->windowSize == 0) {
			throw Exception("Invalid window size");
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
		std::queue<std::string> warnings;

		const auto& corpus = this->corpora[this->currentCorpus];
		const auto& dateMap = corpus.getcDateMap();
		const auto& articleMap = corpus.getcArticleMap();
		const auto& tokens = corpus.getcTokens();

		std::size_t articleIndex{0};
		std::size_t tokenIndex{0};
		std::size_t dateCounter{0};

		// set status message and reset progress
		std::string status{"Processing corpus #"};

		status += std::to_string(this->currentCorpus + 1);
		status += "/";
		status += std::to_string(this->corpora.size());
		status += "...";

		this->setStatusMessage(status);
		this->setProgress(0.F);

		for(const auto& date : dateMap) {
			// skip articles without date
			while(
					articleIndex < articleMap.size()
					&& articleMap[articleIndex].pos < date.pos
			) {
				++articleIndex;
			}

			// reduce date for grouping
			std::string reducedDate{date.value};

			Helper::DateTime::reduceDate(
					reducedDate,
					this->config.groupDateResolution
			);

			// add date if still necessary, and save its iterator
			const auto dateIt{
				this->associations.insert(
						{
								reducedDate,
								{}
						}
				).first
			};

			// go through all articles of the current date
			while(
					articleIndex < articleMap.size()
					&& articleMap[articleIndex].pos < date.pos + date.length
			) {
				// add article if still necessary, and save its iterator
				const auto articleIt{
					dateIt->second.insert({articleMap[articleIndex].value, {}}).first
				};

				if(articleIt->second.categoriesPositions.empty()) {
					std::vector<std::vector<std::uint64_t>>(
							this->queriesCategories.size(),
							std::vector<std::uint64_t>{}
					).swap(
							articleIt->second.categoriesPositions
					);
				}

				// skip tokens without date
				while(
						tokenIndex < tokens.size()
						&& tokenIndex < articleMap[articleIndex].pos
				) {
					++tokenIndex;
				}

				// go through all tokens of the current article
				while(
						tokenIndex < tokens.size()
						&& tokenIndex < articleMap[articleIndex].pos + articleMap[articleIndex].length
				) {
					bool result{false};

					if(
							this->getBoolFromRegEx(
									this->queryKeyWord,
									tokens[tokenIndex],
									result,
									warnings
							)
							&& result
					) {
						// found keyword
						articleIt->second.keywordPositions.push_back(
								articleIt->second.offset
								+ tokenIndex
						);
					}
					else {
						for(std::size_t catIndex{0}; catIndex < this->queriesCategories.size(); ++catIndex) {
							bool result{false};

							if(
									this->getBoolFromRegEx(
											this->queriesCategories[catIndex],
											tokens[tokenIndex],
											result,
											warnings
									)
									&& result
							) {
								// found category
								articleIt->second.categoriesPositions[catIndex].push_back(
										articleIt->second.offset
										+ tokenIndex
								);
							}
						}
					}

					++tokenIndex;
				}

				// update offset of article
				articleIt->second.offset += articleMap[articleIndex].length;

				++articleIndex;
			}

			// log warnings if necessary
			while(!warnings.empty()) {
				this->log(generalLoggingExtended, warnings.front());

				warnings.pop();
			}

			// update progress
			++dateCounter;

			this->setProgress(static_cast<float>(dateCounter) / dateMap.size());

			if(!(this->isRunning())) {
				return;
			}
		}
	}

	// calculate and save associations
	void AssocOverTime::saveAssociations() {
		// set status message and reset progress
		this->setStatusMessage("Calculating and saving associations...");
		this->setProgress(0.F);

		std::size_t dateCounter{0};

		for(const auto& date : this->associations) {
			std::uint64_t occurences{0};
			std::vector<std::uint64_t> catsCounters(this->categoryLabels.size(), 0);

			for(const auto& article : date.second) {
				for(const auto occurence : article.second.keywordPositions) {
					++occurences;

					for(std::size_t cat{0}; cat < this->categoryLabels.size(); ++cat) {
						for(const auto catOccurence : article.second.categoriesPositions[cat]) {
							if(catOccurence > occurence + this->windowSize) {
								break;
							}

							if(
									occurence < this->windowSize
									|| catOccurence >= occurence - this->windowSize
							) {
								++(catsCounters[cat]);
							}
						}
					}
				}
			}

			//TODO DEBUG
			std::cout << "\n" << date.first << ':' << occurences;
			for(const auto counter : catsCounters) {
				std::cout << '|' << counter;
			}
			std::cout << std::flush;
			//TODO END DEBUG

			// update progress
			++dateCounter;

			this->setProgress(static_cast<float>(dateCounter) / this->associations.size());

			if(!(this->isRunning())) {
				return;
			}
		}

		// clear memory
		std::unordered_map<std::string, std::unordered_map<std::string, Associations>>().swap(
				this->associations
		);
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
