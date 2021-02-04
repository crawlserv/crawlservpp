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
 * SentimentOverTime.hpp
 *
 * Calculate the average sentiment over time
 *  associated with specific categories
 *  using the VADER algorithm.
 *
 * If you use it, please cite:
 *
 *  Hutto, C.J. & Gilbert, E.E. (2014). VADER: A Parsimonious Rule-based Model for
 *  Sentiment Analysis of Social Media Text. Eighth International Conference on
 *  Weblogs and Social Media (ICWSM-14). Ann Arbor, MI, June 2014.
 *
 * !!! FOR ENGLISH LANGUAGE ONLY !!!
 *
 *  Created on: Dec 30, 2020
 *      Author: ans
 */

#include "SentimentOverTime.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	SentimentOverTime::SentimentOverTime(
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
	SentimentOverTime::SentimentOverTime(Main::Database& dbBase,
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
	std::string_view SentimentOverTime::getName() const {
		return "SentimentOverTime";
	}

	/*
	 * IMPLEMENTED ALGORITHM FUNCTIONS
	 */

	//! Initializes the target table for the algorithm.
	/*!
	 * \note When this function is called, neither the
	 *   prepared SQL statements, nor the queries have
	 *   been initialized yet.
	 *
	 * \sa initQueries
	 */
	void SentimentOverTime::onAlgoInitTarget() {
		// set target fields
		std::vector<StringString> fields;

		auto numFields{
			sentimentMinNumColumns
		};

		if(this->algoConfig.addArticleSentiment) {
			numFields += this->algoConfig.categoryLabels.size()
					* sentimentArticleColumnsPerCategory;
		}
		else {
			numFields += this->algoConfig.categoryLabels.size()
					* sentimentMinColumnsPerCategory;
		}

		fields.reserve(numFields);

		fields.emplace_back("date", "VARCHAR(10)");

		for(const auto& label : this->algoConfig.categoryLabels) {
			fields.emplace_back(label + "_N", "BIGINT UNSIGNED");
			fields.emplace_back(label, "FLOAT");

			if(this->algoConfig.addArticleSentiment) {
				fields.emplace_back(label + "_a_N", "BIGINT UNSIGNED");
				fields.emplace_back(label + "_a", "FLOAT");
			}
		}

		// initialize target table
		this->database.setTargetFields(fields);

		this->database.initTargetTable(true, true);
	}

	//! Generates the corpus.
	/*!
	 * \note When this function is called, both the
	 *   prepared SQL statements, and the queries have
	 *   already been initialized.
	 *
	 * \sa initQueries
	 */
	void SentimentOverTime::onAlgoInit() {
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

		// initialize sentiment analyzer
		std::string dict{Data::dictDir};
		std::string emojis{Data::dictDir};

		dict += Helper::FileSystem::getPathSeparator();
		emojis += Helper::FileSystem::getPathSeparator();

		dict += this->algoConfig.dictionary;
		emojis += this->algoConfig.emojis;

		this->sentimentAnalyzer = std::make_unique<Data::Sentiment>(dict, emojis);

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

	//! Calculates the sentence-based sentiment scores in the text corpus.
	/*!
	 * One corpus will be processed in each tick.
	 *
	 * If necessary, article-based sentiment scores
	 *  will be calculated after all corpora have
	 *  been processed, as all tokens need to have
	 *  been checked in order to identify all
	 *  articles that need to be included in the
	 *  analysis.
	 *
	 * \note The corpus has already been genererated
	 *   on initialization.
	 *
	 * \sa onAlgoInit
	 */
	void SentimentOverTime::onAlgoTick() {
		if(this->currentCorpus < this->corpora.size()) {
			this->addCurrent();

			++(this->currentCorpus);
		}
		else {
			this->saveSentiments();

			if(this->isRunning()) {
				this->finished();

				// sleep forever (i.e. until the thread is terminated)
				this->sleep(std::numeric_limits<std::uint64_t>::max());
			}
		}
	}


	//! Does nothing.
	void SentimentOverTime::onAlgoPause() {}

	//! Does nothing.
	void SentimentOverTime::onAlgoUnpause() {}

	//! Does nothing.
	void SentimentOverTime::onAlgoClear() {}

	/*
	 * IMPLEMENTED CONFIGURATION FUNCTIONS
	 */

	//! Parses a configuration option for the algorithm.
	void SentimentOverTime::parseAlgoOption() {
		// algorithm options
		this->category("sentiment");
		this->option("add.article.sentiment", this->algoConfig.addArticleSentiment);
		this->option("cat.labels", this->algoConfig.categoryLabels);
		this->option("cat.queries", this->algoConfig.categoryQueries);
		this->option("dictionary", this->algoConfig.dictionary);
		this->option("emojis", this->algoConfig.emojis);
		this->option("ignore.empty.date", this->algoConfig.ignoreEmptyDate);
		this->option("threshold", this->algoConfig.threshold);
		this->option("use.threshold", this->algoConfig.useThreshold);
	}

	//! Checks the configuration options for the algorithm.
	/*!
	 * \throws Module::Analyzer::Thread::Exception
	 *   if no category has been defined.
	 */
	void SentimentOverTime::checkAlgoOptions() {
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

		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

	//! Resets the configuration options for the algorithm.
	void SentimentOverTime::resetAlgo() {
		this->algoConfig = {};
	}

	/*
	 * ALGORITHM FUNCTIONS (private)
	 */

	// add dates, sentence-based sentiment scores and articles from current corpus, if necessary
	void SentimentOverTime::addCurrent() {
		// set status message and reset progress
		std::string status{"category occurrences"};

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
		const auto& sentenceMap = corpus.getcSentenceMap();
		const auto& articleMap = corpus.getcArticleMap();
		const auto& tokens = corpus.getcTokens();

		// check date and sentence map
		if(dateMap.empty()) {
			this->log(
					generalLoggingDefault,
					"WARNING: Corpus #"
					+ std::to_string(this->currentCorpus + 1)
					+ " does not have a date map and has been skipped."
			);

			return;
		}

		if(sentenceMap.empty()) {
			this->log(
					generalLoggingDefault,
					"WARNING: Corpus #"
					+ std::to_string(this->currentCorpus + 1)
					+ " does not have a sentence map and has been skipped."
			);

			return;
		}

		// check article map
		bool processArticles{this->algoConfig.addArticleSentiment};

		if(articleMap.empty() && processArticles) {
			this->log(
					generalLoggingDefault,
					"WARNING: Corpus #"
					+ std::to_string(this->currentCorpus + 1)
					+ " does not have an article map."
			);

			processArticles = false;
		}

		// select first or empty date and article
		std::size_t dateNumber{};
		std::size_t articleNumber{};

		bool lastDate{false};
		bool lastArticle{false};

		auto dateIt{this->dateData.begin()};

		// select and add first or empty first date
		if(SentimentOverTime::selectFirst(dateMap, dateNumber)) {
			std::string firstDateReduced{
				dateMap[0].value
			};

			Helper::DateTime::reduceDate(
					firstDateReduced,
					this->config.groupDateResolution
			);

			dateIt = this->addDate(firstDateReduced);
		}
		else {
			dateIt = this->addDate("");
		}

		if(processArticles) {
			SentimentOverTime::selectFirst(articleMap, articleNumber);
		}

		std::size_t statusCounter{};
		std::size_t resultCounter{};

		for(const auto& sentence : sentenceMap) {
			// identify date
			if(
					SentimentOverTime::identifyCurrent(
							sentence.first,
							dateNumber,
							dateMap,
							lastDate
					)
			) {
				// date changed: reduce new date for grouping
				std::string reducedDate{};

				if(dateNumber > 0) {
					reducedDate = dateMap[dateNumber - 1].value;

					Helper::DateTime::reduceDate(
							reducedDate,
							this->config.groupDateResolution
					);
				}

				// select date group
				dateIt = this->addDate(reducedDate);
			}

			if(this->algoConfig.ignoreEmptyDate && dateIt->first.empty()) {
				continue; // ignore empty dates
			}

			// identify article, if necessary
			std::string article;

			if(processArticles) {
				SentimentOverTime::identifyCurrent(
						sentence.first,
						articleNumber,
						articleMap,
						lastArticle
				);
			}

			if(articleNumber > 0) {
				article = articleMap[articleNumber - 1].value;
			}

			// process sentence
			this->processSentence(tokens, sentence, dateIt, article);

			// update status if necessary
			++statusCounter;
			++resultCounter;

			if(statusCounter == sentimentUpdateCalculateProgressEvery) {
				this->setProgress(
						static_cast<float>(resultCounter)
						/ sentenceMap.size()
				);

				statusCounter = 0;
			}

			if(!(this->isRunning())) {
				return;
			}
		}
	}

	// calculate and save final sentiment scores, including article-based sentiment scores, if necessary
	void SentimentOverTime::saveSentiments() {
		this->setStatusMessage("Calculating and saving results...");
		this->setProgress(0.F);

		this->log(generalLoggingDefault, "calculates and saves results...");

		auto resultNumColumns{
			sentimentMinNumColumns
		};

		if(this->algoConfig.addArticleSentiment) {
			resultNumColumns += this->algoConfig.categoryLabels.size()
					* sentimentArticleColumnsPerCategory;
		}
		else {
			resultNumColumns += this->algoConfig.categoryLabels.size()
					* sentimentMinColumnsPerCategory;
		}

		const auto resultTable{
			this->getTargetTableName()
		};

		std::size_t statusCounter{};
		std::size_t resultCounter{};

		for(const auto& date : this->dateData) {
			if(date.first.empty() && this->algoConfig.ignoreEmptyDate) {
				continue;
			}

			Data::InsertFieldsMixed data;

			data.columns_types_values.reserve(resultNumColumns);

			data.table = resultTable;

			data.columns_types_values.emplace_back(
					"analyzed__date",
					Data::Type::_string,
					Data::Value(date.first)
			);

			std::size_t categoryIndex{};

			for(const auto& category : date.second) {
				const auto& label{
					"analyzed__"
					+ this->algoConfig.categoryLabels[categoryIndex]
				};

				// calculate sentence-based sentiment
				double sentiment{};

				if(category.sentimentCount > 0) {
					sentiment = category.sentimentSum / category.sentimentCount;
				}

				data.columns_types_values.emplace_back(
					label + "_N",
					Data::Type::_uint64,
					Data::Value(category.sentimentCount)
				);

				data.columns_types_values.emplace_back(
					label,
					Data::Type::_double,
					Data::Value(sentiment)
				);

				if(this->algoConfig.addArticleSentiment) {
					const auto& articleData{
						this->calculateArticleSentiment(category.articles)
					};

					double articleSentiment{};

					if(articleData.second > 0) {
						articleSentiment = articleData.first / articleData.second;
					}

					data.columns_types_values.emplace_back(
						label + "_a_N",
						Data::Type::_uint64,
						Data::Value(articleData.second)
					);

					data.columns_types_values.emplace_back(
						label + "_a",
						Data::Type::_double,
						Data::Value(articleSentiment)
					);
				}

				++categoryIndex;
			}

			this->database.insertCustomData(data);

			// target table updated
			this->database.updateTargetTable();

			++statusCounter;
			++resultCounter;

			if(statusCounter == sentimentUpdateSavingProgressEvery) {
				this->setProgress(
						static_cast<float>(resultCounter)
						/ this->dateData.size()
				);

				statusCounter = 0;
			}

			if(!(this->isRunning())) {
				return;
			}
		}
	}

	/*
	 * QUERY FUNCTIONS (private)
	 */

	// initialize algorithm-specific queries
	void SentimentOverTime::initQueries() {
		this->addQueries(this->algoConfig.categoryQueries, this->queriesCategories);
	}

	// delete algorithm-specific queries
	void SentimentOverTime::deleteQueries() {
		this->queriesCategories.clear();
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// add date
	SentimentOverTime::DateData::iterator SentimentOverTime::addDate(const std::string& date) {
		auto it{this->dateData.insert({date, {}}).first};

		if(it->second.empty()) {
			it->second = std::vector<DateCategoryData>(
					this->algoConfig.categoryLabels.size(),
					DateCategoryData{}
			);
		}

		return it;
	}

	// process sentence
	void SentimentOverTime::processSentence(
			const std::vector<std::string>& tokens,
			const std::pair<std::size_t, std::size_t>& sentence,
			const DateData::iterator& dateIt,
			const std::string& article
	) {
		std::queue<std::string> warnings;
		auto end{sentence.first + sentence.second};
		bool toAnalyze{true};
		float sentiment{};
		bool meetsThreshold{false};

		if(end > tokens.size()) {
			end = tokens.size();
		}

		for(std::size_t category{}; category < this->algoConfig.categoryLabels.size(); ++category) {
			bool found{false};

			for(std::size_t word{sentence.first}; word < end; ++word) {
				bool result{false};

				if(
						this->getBoolFromRegEx(
								this->queriesCategories[category],
								tokens[word],
								result,
								warnings
						)
						&& result
				) {
					// found category
					found = true;

					break;
				}
			}

			if(found) {
				if(toAnalyze) {
					sentiment = this->getSentenceScore(sentence, tokens);

					if(this->algoConfig.useThreshold) {
						meetsThreshold = SentimentOverTime::meetsThreshold(
								sentiment,
								this->algoConfig.threshold
						);
					}

					toAnalyze = false;
				}

				auto& data{dateIt->second[category]};

				if(!(this->algoConfig.useThreshold) || meetsThreshold) {
					// add sentiment to category
					data.sentimentSum += sentiment;
					data.sentimentCount++;
				}

				if(!article.empty()) {
					data.articles.emplace(article);
				}
			}
		}

		while(!warnings.empty()) {
			this->log(generalLoggingDefault, "WARNING: " + warnings.front());

			warnings.pop();
		}
	}

	// get sentiment score of specific sentence
	float SentimentOverTime::getSentenceScore(
			const std::pair<std::size_t, std::size_t>& sentence,
			const std::vector<std::string>& tokens
	) {
		const auto end{sentence.first + sentence.second};
		std::vector<std::string> words;

		words.reserve(sentence.second);

		for(std::size_t word{sentence.first}; word < end; ++word) {
			words.emplace_back(tokens[word]);
		}

		return this->sentimentAnalyzer->analyze(words).compound;
	}

	// calculate article sentiment if not calculated yet
	SentimentOverTime::DoubleUInt SentimentOverTime::calculateArticleSentiment(
			const std::unordered_set<std::string>& articles
	) {
		DoubleUInt result{};

		for(const auto& article : articles) {
			auto data{
				this->articleData.insert({ article, {} })
			};

			if(data.second) {
				// article needs to be calculated
				data.first->second = this->calculateArticle(article);

				result.first += data.first->second.first;
			}
			else {
				// article already calculated
				result.first += data.first->second.first;
			}

			++result.second;
		}

		if(result.second > 1) {
			result.first /= result.second;
		}

		return result;
	}

	// calculate sentiment score for specific article
	SentimentOverTime::DoubleUInt SentimentOverTime::calculateArticle(
			const std::string& article
	) {
		DoubleUInt result{};

		for(const auto& corpus : this->corpora) {
			const auto& articleMap{corpus.getcArticleMap()};
			const auto& sentenceMap{corpus.getcSentenceMap()};
			const auto& tokens{corpus.getcTokens()};

			// find article
			const auto articleIt{
				std::find_if(
						articleMap.cbegin(),
						articleMap.cend(),
						[&article](const auto& entry) {
							return entry.value == article;
						}
				)
			};

			if(articleIt == articleMap.cend()) {
				continue;
			}

			const auto articleEnd{
				articleIt->pos + articleIt->length
			};

			// find first sentence of article
			auto sentenceIt{
				std::find_if(
						sentenceMap.cbegin(),
						sentenceMap.cend(),
						[&articleIt, &articleEnd](const auto& sentence) {
							return sentence.first >= articleIt->pos
									&& sentence.first + sentence.second <= articleEnd;
						}
				)
			};

			if(sentenceIt == sentenceMap.cend()) {
				continue;
			}

			// go through all sentences of article
			while(
					sentenceIt != sentenceMap.cend()
					&& sentenceIt->first + sentenceIt->second <= articleEnd
			) {
				float sentiment{this->getSentenceScore(*sentenceIt, tokens)};

				if(
						!(this->algoConfig.useThreshold)
						|| SentimentOverTime::meetsThreshold(
								sentiment,
								this->algoConfig.threshold
						)
				) {
					result.first += sentiment;

					++result.second;
				}

				++sentenceIt;
			}
		}

		if(result.second > 1) {
			result.first /= result.second;
		}

		return result;
	}

	/*
	 * INTERNAL STATIC HELPER FUNCTIONS (private)
	 */

	// select first or empty date or article, return whether first has been selected
	bool SentimentOverTime::selectFirst(
			const TextMap& map,
			std::size_t& numberTo
	) {
		if(!map.empty() && map[0].pos == 0) {
			numberTo = 1;

			return true;
		}

		return false;
	}

	// identify current date or article, return whether it changed
	bool SentimentOverTime::identifyCurrent(
			std::size_t sentenceBegin,
			std::size_t& numberFromTo,
			const TextMap& map,
			bool& isLastFromTo
	) {
		if(isLastFromTo) {
			// already behind last date or article
			return false;
		}

		std::size_t currentEnd{};
		bool changed{false};

		if(numberFromTo > 0) {
			currentEnd = map[numberFromTo - 1].pos + map[numberFromTo - 1].length;
		}

		while(sentenceBegin >= currentEnd && map.size() > numberFromTo) {
			++numberFromTo;
			changed = true;

			currentEnd = map[numberFromTo - 1].pos + map[numberFromTo - 1].length;
		}

		if(sentenceBegin >= currentEnd && currentEnd > 0) {
			numberFromTo = 0;
			isLastFromTo = true;
			changed = true;
		}

		return changed;
	}

	// check whether sentiment meets threshold
	bool SentimentOverTime::meetsThreshold(
			float sentiment,
			std::uint8_t threshold
	) {
		return static_cast<std::uint8_t>(
				std::round(
						std::fabs(sentiment)
						* sentimentPercentageFactor
				)
		) >= threshold;
	}

} /* namespace crawlservpp::Module::Analyzer::Algo */
