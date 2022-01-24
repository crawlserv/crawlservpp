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
 * Assoc.hpp
 *
 * Algorithm counting associations between the keyword and
 *  different categories per article.
 *
 * Created on: Nov 12, 2021
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_ASSOC_HPP_
#define MODULE_ANALYZER_ALGO_ASSOC_HPP_

#include "../Thread.hpp"

#include "../../../Data/Data.hpp"
#include "../../../Helper/DateTime.hpp"
#include "../../../Helper/Memory.hpp"
#include "../../../Main/Database.hpp"
#include "../../../Struct/QueryStruct.hpp"
#include "../../../Struct/StatusSetter.hpp"
#include "../../../Struct/TextMap.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"

#include <algorithm>		// std::all_of, std::min, std::sort
#include <cstddef>			// std::size_t
#include <cstdint>			// std::uint16_t, std::uint64_t
#include <limits>			// std::numeric_limits
#include <queue>			// std::queue
#include <string>			// std::string, std::to_string
#include <string_view>		// std::string_view
#include <unordered_map>	// std::unordered_map
#include <utility>			// std::pair
#include <vector>			// std::vector

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Indicates, while saving, after how many articles the progress of the thread will be updated.
	inline constexpr auto assocUpdateProgressEvery{1000};

	//! Number of extra columns included in a dataset (except date).
	inline constexpr auto assocAddColumns{2}; /* ID of article, occurrences */

	//! Minimum number of columns included in a dataset (including date).
	inline constexpr auto assocMinColumns{assocAddColumns + 1 /* date */};

	//@}

	/*
	 * DECLARATION
	 */

	//! %Empty algorithm template.
	/*!
	 * This is an empty template class for adding
	 *  new algorithms to the application.
	 */
	class Assoc final : public Module::Analyzer::Thread {
		// for convenience
		using QueryStruct = Struct::QueryStruct;
		using StatusSetter = Struct::StatusSetter;
		using TextMap = Struct::TextMap;
		using TextMapEntry = Struct::TextMapEntry;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		using Exception = Module::Analyzer::Thread::Exception;

		using StringString = std::pair<std::string, std::string>;

	public:
		///@name Construction
		///@{

		Assoc(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		Assoc(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions
		);

		///@}
		///@name Implemented Getter
		///@{

		std::string_view getName() const override;

		///@}
		///@name Implemented Algorithm Functions
		///@{

		void onAlgoInitTarget() override;
		void onAlgoInit() override;
		void onAlgoTick() override;
		void onAlgoPause() override;
		void onAlgoUnpause() override;
		void onAlgoClear() override;

		///@}
		///@name Implemented Configuration Functions
		///@{

		void parseAlgoOption() override;
		void checkAlgoOptions() override;
		void resetAlgo() override;

		///@}

	private:
		// custom structures
		struct Associations {
			std::vector<std::uint64_t> keywordPositions;
			std::vector<std::vector<std::uint64_t>> categoriesPositions;
			std::uint64_t offset{};
		};

		struct Result {
			std::string articleId;
			std::string date;
			std::uint64_t nOccurences{};
			std::vector<std::uint64_t> catCounters;
		};

		using DateAssociationMap = std::unordered_map<std::string, std::unordered_map<std::string, Associations>>;
		using DateAssociation = std::pair<std::string, std::unordered_map<std::string, Associations>>;
		using ArticleAssociationMap = std::unordered_map<std::string, Associations>;
		using ArticleAssociation = std::pair<std::string, Associations>;

		// algorithm options
		struct Entries {
			std::vector<std::string> categoryLabels;
			std::vector<std::uint64_t> categoryQueries;
			bool combineSources{true};
			bool ignoreEmptyDate{true};
			std::uint64_t keyWordQuery{};
			std::uint16_t windowSize{1};
		} algoConfig;

		// algorithm queries
		QueryStruct queryKeyWord;

		std::vector<QueryStruct> queriesCategories;

		// algorithm state
		DateAssociationMap associations;

		std::string previousDate;

		std::size_t currentCorpus{};
		std::size_t dateCounter{};
		std::size_t firstDatePos{};
		std::size_t dateMapSize{};
		std::size_t articleIndex{};
		std::size_t tokenIndex{};
		std::size_t processedDates{};

		bool dateSaved{false};

		// algorithm functions
		void addCurrent();
		void saveAssociations();
		[[nodiscard]] std::vector<Result> processDates();
		void saveResults(const std::vector<Result>& results);

		// query functions
		void initQueries() override;
		void deleteQueries() override;

		// internal helper functions
		void addArticlesForDate(
				const TextMapEntry& date,
				DateAssociationMap::iterator& dateIt,
				const TextMap& articleMap,
				const std::vector<std::string>& tokens,
				std::queue<std::string>& warningsTo
		);
		DateAssociationMap::iterator addDate(const std::string& date);
		ArticleAssociationMap::iterator addArticleToDate(
				const std::string& article,
				DateAssociationMap::iterator date
		);
		void processToken(
				const std::string& token,
				Associations& associationsTo,
				std::queue<std::string>& warningsTo
		);
		void processDate(
				const DateAssociation& date,
				std::vector<Result>& resultsTo
		);
		void processArticle(
				const std::string& date,
				const ArticleAssociation& article,
				std::vector<Result>& resultsTo
		);
		void processTermOccurrence(
				const ArticleAssociation& article,
				std::uint64_t occurrence,
				std::size_t& occurrencesTo,
				std::vector<std::uint64_t>& catsCountersTo
		);
		void processCategory(
				const ArticleAssociation& article,
				std::uint64_t termOccurrence,
				std::size_t index,
				std::vector<std::uint64_t>& catsCountersTo
		);
		bool processCategoryOccurrence(
				std::uint64_t termOccurrence,
				std::uint64_t catOccurrence,
				std::size_t catIndex,
				std::vector<std::uint64_t>& catsCountersTo
		) const;

		void insertDataSet(
				const std::string& table,
				const Result& dataSet,
				std::size_t numColumns
		);
	};

} /* namespace crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_ASSOC_HPP_ */