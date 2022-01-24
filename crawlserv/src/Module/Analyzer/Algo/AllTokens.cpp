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
 * AllTokens.hpp
 *
 * Count all tokens in a corpus.
 *
 *  Tokens will be counted by date and/or article, if possible.
 *
 *  Created on: Mar 19, 2021
 *      Author: ans
 */

#include "AllTokens.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	AllTokens::AllTokens(
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
	AllTokens::AllTokens(Main::Database& dbBase,
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
	std::string_view AllTokens::getName() const {
		return "AllTokens";
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
	void AllTokens::onAlgoInitTarget() {
		this->database.setTargetFields({{"tid", "BIGINT UNSIGNED"}, {"token", "TEXT"}});

		this->database.initTargetTable(true, true);
	}

	//! Initializes the algorithm and processes its input.
	/*!
	 * \note When this function is called, both the
	 *   prepared SQL statements, and the queries have
	 *   already been initialized.
	 *
	 * \throws AllTokens::Exception if the corpus is empty.
	 *
	 * \sa initQueries
	 */
	void AllTokens::onAlgoInit() {
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
				throw Exception("AllTokens::onAlgoInit(): Corpus is empty");
			}

			return;
		}

		// initialize algorithm
		if(this->corpora[0].hasDateMap()) {
			this->orderBy = DATES;
			this->hasArticles = this->corpora[0].hasArticleMap();
			this->total = this->corpora[0].getcDateMap().size();
		}
		else if(this->corpora[0].hasArticleMap()) {
			this->orderBy = ARTICLES;
			this->total = this->corpora[0].getcArticleMap().size();
		}
		else {
			this->orderBy = NONE;
			this->total = this->corpora[0].getcTokens().size();
		}

		// algorithm is ready
		this->log(generalLoggingExtended, "is ready.");

		/*
		 * NOTE: Do not set any thread status here, as the parent class
		 *        will revert to the original thread status after initialization.
		 */
	}

	//! Counts tokens in the current date, article, or token.
	/*!
	 * If a date map exists, each tick the tokens for the
	 *  current date are counted. If no date map, but an
	 *  article map exists, each tick the tokens for the
	 *  current article are counted. If neither a date nor
	 *  an article map exists, one token is counted each tick.
	 */
	void AllTokens::onAlgoTick() {
		if(this->firstTick) {
			this->setStatusMessage("Counting tokens...");
			this->log(generalLoggingDefault, "counts tokens...");

			this->firstTick = false;
		}

		if(this->done) {
			return;
		}

		switch(this->orderBy) {
		case DATES:
			this->nextDate();

			this->updateProgress(allTokensUpdateEveryDate);

			break;

		case ARTICLES:
			this->nextArticle();

			this->updateProgress(allTokensUpdateEveryArticle);

			break;

		default:
			this->nextToken();

			this->updateProgress(allTokensUpdateEveryToken);
		}

		if(this->done) {
			this->clearCorpus();
			this->saveData();
			this->finished();

			return;
		}

		++(this->count);
	}

	//! Does nothing.
	void AllTokens::onAlgoPause() {}

	void AllTokens::onAlgoUnpause() {}

	//! Does nothing.
	void AllTokens::onAlgoClear() {}

	//! Parses a configuration option for the algorithm.
	void AllTokens::parseAlgoOption() {
		this->category("all-tokens");

		this->option("table", this->algoConfig.countTable);
	}

	//! Checks the configuration options for the algorithm.
	/*!
	 * \throws Analyzer::Thread::Exception if no
	 *   token count table has been specified.
	 */
	void AllTokens::checkAlgoOptions() {
		if(this->algoConfig.countTable.empty()) {
			throw Exception(
					"AllTokens::checkAlgoOptions():"
					" No token count table has been specified"
			);
		}

		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

	//! Resets the algorithm.
	void AllTokens::resetAlgo() {
		this->orderBy = NONE;

		this->total = 0;
		this->articleCount = 0;
		this->count = 0;
		this->updateCount = 0;
		this->countsTable = 0;

		this->hasArticles = false;
		this->done = false;
		this->firstTick = true;

		Helper::Memory::free(tokens);
		Helper::Memory::free(tokenCounts);
		Helper::Memory::free(singleMap);
		Helper::Memory::free(doubleMap);

	}

	/*
	 * ALGORITHM FUNCTIONS (private)
	 */

	// count tokens for next date (used if a date map exists)
	void AllTokens::nextDate() {
		if(this->isDone(this->corpora[0].getcDateMap())) {
			return;
		}

		const auto& date{this->corpora[0].getcDateMap()[this->count]};

		if(this->hasArticles) {
			const auto& articles{this->corpora[0].getcArticleMap()};

			while(
					this->articleCount < articles.size()
					&& TextMapEntry::end(articles[this->articleCount]) < TextMapEntry::pos(date)
			) {
				++(this->articleCount);
			}

			while(
					this->articleCount < articles.size()
					&& TextMapEntry::pos(articles[this->articleCount]) < TextMapEntry::end(date)
			) {
				AllTokens::processSingle(
						this->corpora[0].getcTokens(),
						articles[this->articleCount],
						this->tokens,
						this->singleMap
				);

				++(this->articleCount);

				if(!(this->isRunning())) {
					return;
				}
			}

			AllTokens::processDouble(
					date,
					this->singleMap,
					this->doubleMap
			);
		}
		else {
			AllTokens::processSingle(
					this->corpora[0].getcTokens(),
					date,
					this->tokens,
					this->singleMap
			);
		}
	}

	// count tokens in next article (used if no date map, but an article map exists)
	void AllTokens::nextArticle() {
		if(this->isDone(this->corpora[0].getcArticleMap())) {
			return;
		}

		AllTokens::processSingle(
				this->corpora[0].getcTokens(),
				this->corpora[0].getcArticleMap()[this->count],
				this->tokens,
				this->singleMap
		);
	}

	// count next token (used if neither a date nor an article map exists)
	void AllTokens::nextToken() {
		if(this->isDone(this->corpora[0].getcTokens())) {
			return;
		}

		AllTokens::processToken(
				this->corpora[0].getcTokens()[this->count],
				this->tokens,
				this->tokenCounts
		);
	}
	// free all memory associated with the corpus
	void AllTokens::clearCorpus() {
		Helper::Memory::free(this->corpora);
	}

	// save results to database
	void AllTokens::saveData() {
		this->saveTokens();
		this->saveCounts();
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// update thread progress, if necessary
	void AllTokens::updateProgress(std::uint32_t every) {
		++(this->updateCount);

		if(this->updateCount == every) {
			this->setProgress(static_cast<float>(this->count) / this->total);

			this->updateCount = 0;
		}
	}

	// save tokens into target table
	void AllTokens::saveTokens() {
		const auto tableName{this->getTargetTableName()};
		const auto rowTotal{this->tokens.size()};
		std::size_t rowCount{};
		std::size_t rowUpdateCount{};

		this->setStatusMessage("Saving all tokens...");
		this->setProgress(0.F);
		this->log(generalLoggingDefault, "saves all tokens...");

		Timer::Simple timer;

		for(const auto& token : this->tokens) {
			Data::InsertFieldsMixed data;

			data.columns_types_values.reserve(allTokensColumns);
			data.table = tableName;

			data.columns_types_values.emplace_back(
					"analyzed__tid",
					Data::Type::_uint64,
					token.second
			);

			data.columns_types_values.emplace_back(
					"analyzed__token",
					Data::Type::_string,
					token.first
			);

			this->database.insertCustomData(data);

			++rowCount;
			++rowUpdateCount;

			if(rowUpdateCount == allTokensUpdateEveryRow) {
				this->setProgress(static_cast<float>(rowCount) / rowTotal);

				rowUpdateCount = 0;
			}
		}

		this->database.updateTargetTable();

		this->log(generalLoggingDefault, "saved all tokens in " + timer.tickStr() + ".");
	}

	// save token counts into additional target table
	void AllTokens::saveCounts() {
		this->setStatusMessage("Saving token counts...");
		this->setProgress(0.F);
		this->log(generalLoggingDefault, "saves token counts...");

		Timer::Simple timer;

		this->initCountsTable();

		switch(this->orderBy) {
		case DATES:
			if(this->hasArticles) {
				this->saveDouble();
			}
			else {
				this->saveSingle("date");
			}

			break;

		case ARTICLES:
			this->saveSingle("article");

			break;

		default:
			this->saveTokenCounts();
		}

		this->log(generalLoggingDefault, "saved token counts in " + timer.tickStr() + ".");
	}

	// save double map to additional table
	void AllTokens::saveDouble() {
		const auto tableName{
			this->database.getAdditionalTableName(
					this->countsTable
			)
		};
		std::size_t saveCount{};

		for(const auto& date : this->doubleMap) {
			for(const auto& article : date.second) {
				Data::InsertFieldsMixed data;

				data.table = tableName;
				data.columns_types_values.emplace_back(
						"analyzed__date",
						Data::Type::_string,
						date.first
				);
				data.columns_types_values.emplace_back(
						"analyzed__article",
						Data::Type::_string,
						article.first
				);

				AllTokens::addTokenCounts(article.second, data);

				this->database.insertCustomData(data);
			}

			++saveCount;

			this->setProgress(static_cast<float>(saveCount) / this->total);
		}
	}

	// save single map to additional table
	void AllTokens::saveSingle(const std::string& typeName) {
		const auto tableName{
			this->database.getAdditionalTableName(
					this->countsTable
			)
		};
		std::size_t saveCount{};

		for(const auto& entry : this->singleMap) {
			Data::InsertFieldsMixed data;

			data.table = tableName;

			data.columns_types_values.emplace_back(
					"analyzed__" + typeName,
					Data::Type::_string,
					entry.first
			);

			AllTokens::addTokenCounts(entry.second, data);

			this->database.insertCustomData(data);

			++saveCount;

			this->setProgress(static_cast<float>(saveCount) / this->total);
		}
	}

	// save token counts to additional table
	void AllTokens::saveTokenCounts() {
		Data::InsertFieldsMixed data;

		data.table = this->database.getAdditionalTableName(
				this->countsTable
		);

		AllTokens::addTokenCounts(this->tokenCounts, data);

		this->database.insertCustomData(data);
	}

	// initialize additional table for token counts
	void AllTokens::initCountsTable() {
		std::vector<StringString> countTableFields;

		switch(this->orderBy) {
		case DATES:
			countTableFields.emplace_back("date", "VARCHAR(10)");

			if(this->hasArticles) {
				countTableFields.emplace_back("article", "TEXT");
			}

			break;

		case ARTICLES:
			countTableFields.emplace_back("article", "TEXT");

			break;

		default:
			break;
		}

		for(const auto& p : this->tokens) {
			countTableFields.emplace_back("t" + std::to_string(p.second), "BIGINT UNSIGNED");
		}

		this->countsTable = this->database.addAdditionalTable(
				this->algoConfig.countTable,
				countTableFields,
				false,
				true
		);
	}

	/*
	 * STATIC INTERNAL HELPER FUNCTIONS (private)
	 */

	// count tokens in the current entry and, using the name of the current entry, add them to the single map if necessary
	void AllTokens::processSingle(
			const std::vector<std::string>& corpusTokens,
			const TextMapEntry& entry,
			TokenMap& tokenMap,
			SingleMap& to
	) {
		for(auto i{TextMapEntry::pos(entry)}; i < TextMapEntry::end(entry); ++i) {
			AllTokens::processToken(
					corpusTokens[i],
					tokenMap,
					to[entry.value]
			);
		}
	}

	// using the name of the current entry, add single map to double map and free the single map
	void AllTokens::processDouble(
			const TextMapEntry& entry,
			SingleMap& from,
			DoubleMap& to
	) {
		to.emplace(entry.value, from);

		Helper::Memory::free(from);
	}

	// add token to token map, if necessary, and to token count
	void AllTokens::processToken(
			const std::string& token,
			TokenMap& tokenMap,
			TokenCounts& to
	) {
		auto tokenId{tokenMap.size()};
		const auto p{tokenMap.insert({token, tokenId})};

		if(!p.second) {
			// token already existed
			tokenId = p.first->second;
		}

		// update token count
		to[tokenId]++;
	}

	// add token counts to resulting table row
	void AllTokens::addTokenCounts(
			const TokenCounts& from,
			Data::InsertFieldsMixed& to
	) {
		to.columns_types_values.reserve(to.columns_types_values.size() + from.size());

		for(const auto& p : from) {
			to.columns_types_values.emplace_back(
				"analyzed__t" + std::to_string(p.second),
				Data::Type::_uint64,
				p.first
			);
		}
	}

} /* namespace crawlservpp::Module::Analyzer::Algo */
