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
	 * IMPLEMENTED GETTER
	 */

	//! Returns the name of the algorithm.
	/*!
	 * \returns A string view containing the
	 *   name of the implemented algorithm.
	 */
	std::string_view AssocOverTime::getName() const {
		return "AssocOverTime";
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
	void AssocOverTime::onAlgoInitTarget() {
		// set target fields
		std::vector<StringString> fields;

		const auto numFields{
			assocOverTimeMinColumns
			+ this->algoConfig.categoryLabels.size()
		};

		fields.reserve(numFields);

		fields.emplace_back("date", "VARCHAR(10)");
		fields.emplace_back("n", "BIGINT UNSIGNED");
		fields.emplace_back("occurrences", "BIGINT UNSIGNED");

		for(const auto& label : this->algoConfig.categoryLabels) {
			fields.emplace_back(label, "BIGINT UNSIGNED");
		}

		this->database.setTargetFields(fields);

		// initialize target table
		this->database.initTargetTable(true, true);
	}

	//! Generates the corpus.
	/*!
	 * \note When this function is called, both the
	 *   prepared SQL statements, and the queries have
	 *   already been initialized.
	 *
	 * \throws AssocOverTime::Exception if no non-
	 *   empty corpus has been added.
	 *
	 * \sa initQueries
	 */
	void AssocOverTime::onAlgoInit() {
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

		if(!(this->addCorpora(this->algoConfig.combineSources, statusSetter))) {
			if(this->isRunning()) {
				throw Exception(
						"AssocOverTime::onAlgoInit():"
						" No non-empty corpus has been added"
				);
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

	//! Calculates the associations in the text corpus.
	/*!
	 * One corpus will be processed in each tick.
	 *
	 * \note The corpus has already been genererated
	 *   on initialization.
	 *
	 * \sa onAlgoInit
	 */
	void AssocOverTime::onAlgoTick() {
		if(this->currentCorpus < this->corpora.size()) {
			this->addCurrent();

			++(this->currentCorpus);

			return;
		}

		this->saveAssociations();

		// sleep forever (i.e. until the thread is terminated)
		this->finished();
		this->sleep(std::numeric_limits<std::uint64_t>::max());
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
		this->option("cat.labels", this->algoConfig.categoryLabels);
		this->option("cat.queries", this->algoConfig.categoryQueries);
		this->option("combine.sources", this->algoConfig.combineSources);
		this->option("keyword", this->algoConfig.keyWordQuery);
		this->option("ignore.empty.date", this->algoConfig.ignoreEmptyDate);
		this->option("window.size", this->algoConfig.windowSize);
	}

	//! Checks the configuration options for the algorithm.
	/*!
	 * \throws Module::Analyzer::Thread::Exception
	 *   if no keyword or no category has been defined.
	 */
	void AssocOverTime::checkAlgoOptions() {
		// check keyword query
		if(this->algoConfig.keyWordQuery == 0) {
			throw Exception("No keyword defined");
		}

		// check categories
		if(
				std::all_of(
						this->algoConfig.categoryQueries.cbegin(),
						this->algoConfig.categoryQueries.cend(),
						[](const auto query) {
							return query == 0;
						}
				)
		) {
			throw Exception("No category defined");
		}

		const auto completeCategories{
			std::min( // number of complete categories (= min. size of all arrays)
					this->algoConfig.categoryLabels.size(),
					this->algoConfig.categoryQueries.size()
			)
		};

		bool incompleteCategories{false};

		// remove category labels or queries that are not used
		if(this->algoConfig.categoryLabels.size() > completeCategories) {
			this->algoConfig.categoryLabels.resize(completeCategories);

			incompleteCategories = true;
		}
		else if(this->algoConfig.categoryQueries.size() > completeCategories) {
			this->algoConfig.categoryQueries.resize(completeCategories);

			incompleteCategories = true;
		}

		if(incompleteCategories) {
			this->warning(
					"'cat.labels', '.queries'"
					" should have the same number of elements."
			);
		}

		// remove empty labels, invalid queries
		for(std::size_t index{}; index < this->algoConfig.categoryLabels.size(); ++index) {
			if(
					this->algoConfig.categoryLabels[index].empty()
					|| this->algoConfig.categoryQueries[index] == 0
			) {
				this->algoConfig.categoryLabels.erase(this->algoConfig.categoryLabels.begin() + index);
				this->algoConfig.categoryQueries.erase(this->algoConfig.categoryQueries.begin() + index);

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
		if(this->algoConfig.windowSize == 0) {
			throw Exception("Invalid window size");
		}

		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

	//! Resets the algorithm.
	void AssocOverTime::resetAlgo() {
		this->algoConfig = {};
		this->queryKeyWord = {};

		Helper::Memory::free(this->queriesCategories);
		Helper::Memory::free(this->associations);
		Helper::Memory::free(this->previousDate);

		this->currentCorpus = 0;
		this->dateCounter = 0;
		this->firstDatePos = 0;
		this->dateSaved = false;
		this->dateMapSize = 0;
		this->articleIndex = 0;
		this->tokenIndex = 0;
		this->processedDates = 0;
	}

	/*
	 * ALGORITHM FUNCTIONS (private)
	 */

	// add terms and categories from current corpus
	void AssocOverTime::addCurrent() {
		std::queue<std::string> warnings;

		// set status message and reset progress
		std::string status{"term and category occurrences"};

		if(this->corpora.size() > 1) {
			status += " in corpus #";
			status += std::to_string(this->currentCorpus + 1);
			status += "/";
			status += std::to_string(this->corpora.size());
		}

		status += "...";

		this->setStatusMessage("Identifying " + status);
		this->setProgress(0.F);

		this->log(generalLoggingDefault, "identifies " + status);

		// set current corpus
		const auto& corpus = this->corpora[this->currentCorpus];
		const auto& dateMap = corpus.getcDateMap();
		const auto& articleMap = corpus.getcArticleMap();
		const auto& tokens = corpus.getcTokens();

		// set initial state
		this->dateCounter = 0;
		this->firstDatePos = tokens.size();
		this->dateSaved = false;
		this->dateMapSize = dateMap.size();
		this->articleIndex = 0;
		this->tokenIndex = 0;

		Helper::Memory::free(this->previousDate);

		auto dateIt{this->associations.begin()};

		// check date map
		if(dateMap.empty()) {
			this->log(
					generalLoggingDefault,
					"WARNING: Corpus #"
					+ std::to_string(this->currentCorpus + 1)
					+ " does not have a date map and has been skipped."
			);

			return;
		}

		// handle articles without date
		this->firstDatePos = TextMapEntry::pos(dateMap.front());

		if(this->firstDatePos > 0 && !(this->algoConfig.ignoreEmptyDate)) {
			if(articleMap.empty()) {
				// no date and no article map: treat as one article
				dateIt = this->addDate("");

				const auto articleIt{
					this->addArticleToDate("", dateIt)
				};

				// go through all tokens without date
				for(; this->tokenIndex < this->firstDatePos; ++(this->tokenIndex)) {
					this->processToken(
							tokens[this->tokenIndex],
							articleIt->second,
							warnings
					);
				}
			}
			else {
				// handle articles without date
				while(
						this->articleIndex < articleMap.size()
						&& TextMapEntry::pos(articleMap[this->articleIndex]) < this->firstDatePos
				) {
					// add empty date if still necessary
					if(!(this->dateSaved)) {
						dateIt = this->addDate("");

						this->dateSaved = true;
					}

					// add article if still necessary, and save its iterator
					const auto articleIt{
						this->addArticleToDate(articleMap[this->articleIndex].value, dateIt)
					};

					// go through all tokens of the current article
					while(
							this->tokenIndex < tokens.size()
							&& this->tokenIndex < TextMapEntry::end(articleMap[this->articleIndex])
					) {
						this->processToken(
								tokens[this->tokenIndex],
								articleIt->second,
								warnings
						);

						++(this->tokenIndex);
					}

					// update offset of article
					articleIt->second.offset += TextMapEntry::length(articleMap[this->articleIndex]);

					++(this->articleIndex);
				}
			}

			// log warnings if necessary
			while(!warnings.empty()) {
				this->log(generalLoggingExtended, warnings.front());

				warnings.pop();
			}
		}

		// process dates
		for(const auto& date : dateMap) {
			this->addArticlesForDate(
					date,
					dateIt,
					articleMap,
					tokens,
					warnings
			);
		}
	}

	// calculate and save associations
	void AssocOverTime::saveAssociations() {
		// set status message and reset progress
		this->setStatusMessage("Calculating associations...");
		this->setProgress(0.F);

		// process dates
		const auto results{this->processDates()};

		// set status message and reset progress
		this->setStatusMessage("Writing results to database...");
		this->setProgress(0.F);

		// save results to target table
		this->saveResults(results);
	}

	// process dates in order to save associations
	AssocOverTime::Results AssocOverTime::processDates() {
		Results results;

		this->processedDates = 0;

		for(const auto& date : this->associations) {
			this->processDate(date, results);
		}

		// sort results
		std::sort(results.begin(), results.end(), [](const auto& a, const auto&b) {
			return a.first < b.first;
		});

		return results;
	}

	// save results to database
	void AssocOverTime::saveResults(const Results& results) {
		const auto resultNumColumns{
			assocOverTimeMinColumns
			+ this->algoConfig.categoryLabels.size()
		};

		const auto targetTable{
			this->getTargetTableName()
		};

		std::size_t statusCounter{};
		std::size_t resultCounter{};
		bool updated{false};

		this->previousDate.clear();

		for(const auto& result : results) {
			// check for empty date
			if(this->algoConfig.ignoreEmptyDate && result.first.empty()) {
				continue; /* ignore empty date */
			}

			// fill gap between previous and current date, if necessary
			this->fillGap(targetTable, result.first, resultNumColumns);

			// insert actual data set
			this->insertDataSet(targetTable, result.first, result.second, resultNumColumns);

			// update status if necessary
			++statusCounter;
			++resultCounter;

			if(statusCounter == assocOverTimeUpdateProgressEvery) {
				this->setProgress(
						static_cast<float>(resultCounter)
						/ results.size()
				);

				statusCounter = 0;
			}

			updated = true;

			if(!(this->isRunning())) {
				return;
			}
		}

		if(updated) {
			// target table updated
			this->database.updateTargetTable();
		}

		Helper::Memory::free(this->associations);
	}

	/*
	 * QUERY FUNCTIONS (private)
	 */

	// initialize algorithm-specific queries
	void AssocOverTime::initQueries() {
		this->addQueries(this->algoConfig.categoryQueries, this->queriesCategories);

		this->addOptionalQuery(this->algoConfig.keyWordQuery, this->queryKeyWord);
	}

	// delete algorithm-specific queries
	void AssocOverTime::deleteQueries() {
		this->queriesCategories.clear();

		this->queryKeyWord = {};
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// add the articles of a specific date
	void AssocOverTime::addArticlesForDate(
			const TextMapEntry& date,
			DateAssociationMap::iterator& dateIt,
			const TextMap& articleMap,
			const std::vector<std::string>& tokens,
			std::queue<std::string>& warningsTo
	) {
		// skip articles without date
		if(this->firstDatePos > 0 && this->algoConfig.ignoreEmptyDate) {
			while(
					this->articleIndex < articleMap.size()
					&& TextMapEntry::pos(articleMap[this->articleIndex]) < TextMapEntry::pos(date)
			) {
				++(this->articleIndex);
			}
		}

		// reduce date for grouping
		std::string reducedDate{date.value};

		Helper::DateTime::reduceDate(
				reducedDate,
				this->config.groupDateResolution
		);

		// add date if still necessary, and save its iterator
		if(!(this->dateSaved) || this->previousDate != reducedDate) {
			dateIt = this->addDate(reducedDate);

			this->previousDate = reducedDate;
			this->dateSaved = true;
		}

		// go through all articles of the current date
		while(
				this->articleIndex < articleMap.size()
				&& TextMapEntry::pos(articleMap[this->articleIndex]) < TextMapEntry::end(date)
		) {
			// add article if still necessary, and save its iterator
			const auto articleIt{
				this->addArticleToDate(articleMap[this->articleIndex].value, dateIt)
			};

			// skip tokens without date
			if(this->algoConfig.ignoreEmptyDate) {
				while(
						this->tokenIndex < tokens.size()
						&& this->tokenIndex < TextMapEntry::pos(articleMap[this->articleIndex])
				) {
					++(this->tokenIndex);
				}
			}

			// go through all tokens of the current article
			while(
					this->tokenIndex < tokens.size()
					&& this->tokenIndex < TextMapEntry::end(articleMap[this->articleIndex])
			) {
				this->processToken(
						tokens[this->tokenIndex],
						articleIt->second,
						warningsTo
				);

				++(this->tokenIndex);
			}

			// update offset of article
			articleIt->second.offset += TextMapEntry::length(articleMap[this->articleIndex]);

			++(this->articleIndex);
		}

		// log warnings if necessary
		while(!warningsTo.empty()) {
			this->log(generalLoggingExtended, warningsTo.front());

			warningsTo.pop();
		}

		// update progress
		++(this->dateCounter);

		this->setProgress(static_cast<float>(this->dateCounter) / this->dateMapSize);

		if(!(this->isRunning())) {
			return;
		}
	}

	// add date
	AssocOverTime::DateAssociationMap::iterator AssocOverTime::addDate(const std::string& date) {
		return this->associations.insert({date, {}}).first;
	}

	// add article to date (and initialize its categories) if necessary
	AssocOverTime::ArticleAssociationMap::iterator AssocOverTime::addArticleToDate(
			const std::string& article,
			DateAssociationMap::iterator date
	) {
		const auto articleIt{date->second.insert({article, {}}).first};

		if(articleIt->second.categoriesPositions.empty()) {
			std::vector<std::vector<std::uint64_t>>(
					this->queriesCategories.size(),
					std::vector<std::uint64_t>{}
			).swap(
					articleIt->second.categoriesPositions
			);
		}

		return articleIt;
	}

	// process token and add as keyword or category if necessary
	void AssocOverTime::processToken(
			const std::string& token,
			Associations& associationsTo,
			std::queue<std::string>& warningsTo
	) {
		// ignore empty tokens
		if(token.empty()) {
			return;
		}

		// look for keyword
		bool result{false};

		if(
				this->getBoolFromRegEx(
						this->queryKeyWord,
						token,
						result,
						warningsTo
				)
				&& result
		) {
			// found keyword
			associationsTo.keywordPositions.push_back(
					associationsTo.offset
					+ this->tokenIndex
			);
		}
		else {
			// look for categories
			for(
					std::size_t catIndex{};
					catIndex < this->queriesCategories.size();
					++catIndex
			) {
				bool result{false};

				if(
						this->getBoolFromRegEx(
								this->queriesCategories[catIndex],
								token,
								result,
								warningsTo
						)
						&& result
				) {
					// found category
					associationsTo.categoriesPositions[catIndex].push_back(
							associationsTo.offset
							+ this->tokenIndex
					);
				}
			}
		}
	}

	// process date
	void AssocOverTime::processDate(const DateAssociation& date, Results& resultsTo) {
		std::uint64_t occurrences{};
		std::vector<std::uint64_t> catsCounters(this->algoConfig.categoryLabels.size(), 0);

		for(const auto& article : date.second) {
			this->processArticle(article, occurrences, catsCounters);
		}

		// add row to results
		resultsTo.emplace_back(
				date.first,
				std::vector<std::uint64_t>{}
		);

		resultsTo.back().second.push_back(date.second.size());
		resultsTo.back().second.push_back(occurrences);

		for(const auto& counter : catsCounters) {
			resultsTo.back().second.push_back(counter);
		}

		// update progress
		++(this->processedDates);

		this->setProgress(static_cast<float>(this->processedDates) / this->associations.size());

		if(!(this->isRunning())) {
			return;
		}
	}

	// process article
	void AssocOverTime::processArticle(
			const ArticleAssociation& article,
			std::size_t& occurrencesTo,
			std::vector<std::uint64_t>& catsCountersTo
	) {
		for(const auto& occurrence : article.second.keywordPositions) {
			this->processTermOccurrence(article, occurrence, occurrencesTo, catsCountersTo);
		}
	}

	// process single term occurrence
	void AssocOverTime::processTermOccurrence(
			const ArticleAssociation& article,
			std::uint64_t occurrence,
			std::size_t& occurrencesTo,
			std::vector<std::uint64_t>& catsCountersTo
	) {
		++occurrencesTo;

		for(std::size_t cat{}; cat < this->algoConfig.categoryLabels.size(); ++cat) {
			this->processCategory(article, occurrence, cat, catsCountersTo);
		}
	}

	// process category
	void AssocOverTime::processCategory(
			const ArticleAssociation& article,
			std::uint64_t termOccurrence,
			std::size_t index,
			std::vector<std::uint64_t>& catsCountersTo
	) {
		for(const auto catOccurrence : article.second.categoriesPositions[index]) {
				if(!(this->processCategoryOccurrence(
						termOccurrence,
						catOccurrence,
						index,
						catsCountersTo
				))) {
					break;
				}
		}
	}

	// process single category occurrence, return false if the end of the window has been reached
	bool AssocOverTime::processCategoryOccurrence(
			std::uint64_t termOccurrence,
			std::uint64_t catOccurrence,
			std::size_t catIndex,
			std::vector<std::uint64_t>& catsCountersTo
	) const {
		if(catOccurrence > termOccurrence + this->algoConfig.windowSize) {
			return false;
		}

		if(
				termOccurrence < this->algoConfig.windowSize
				|| catOccurrence >= termOccurrence - this->algoConfig.windowSize
		) {
			++(catsCountersTo[catIndex]);
		}

		return true;
	}

	// fill gap between previous and current date, if necessary
	void AssocOverTime::fillGap(const std::string& table, const std::string& date, std::size_t numColumns) {
		if(!(this->config.groupDateFillGaps)) {
			return; /* filling gaps is disabled */
		}

		if(this->previousDate.empty()) {
			// first date: store date and return
			this->previousDate = date;

			return;
		}

		// retrieve and fill gap between previous and current date
		const auto missingDates{
			Helper::DateTime::getDateGap(
					this->previousDate,
					date,
					this->config.groupDateResolution
			)
		};

		for(const auto& missingDate : missingDates) {
			this->insertDataSet(
					table,
					missingDate,
					std::vector<std::uint64_t>(
							assocOverTimeAddColumns
							+ this->algoConfig.categoryLabels.size()
					),
					numColumns
			);
		}

		this->previousDate = date;
	}

	// insert dataset into target table
	void AssocOverTime::insertDataSet(
			const std::string& table,
			const std::string& date,
			const std::vector<std::uint64_t>& dataSet,
			std::size_t numColumns
	) {
		Data::InsertFieldsMixed data;

		data.columns_types_values.reserve(numColumns);

		data.table = table;

		data.columns_types_values.emplace_back(
				"analyzed__date",
				Data::Type::_string,
				Data::Value(date)
		);

		std::size_t n{};

		for(const auto& number : dataSet) {
			std::string column;

			switch(n) {
			case 0:
				column = "analyzed__n";

				break;

			case 1:
				column = "analyzed__occurrences";

				break;

			default:
				column = "analyzed__" + this->algoConfig.categoryLabels.at(n - assocOverTimeAddColumns);
			}

			data.columns_types_values.emplace_back(
				column,
				Data::Type::_uint64,
				Data::Value(number)
			);

			++n;
		}

		this->database.insertCustomData(data);
	}

} /* namespace crawlservpp::Module::Analyzer::Algo */
