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

#ifndef MODULE_ANALYZER_ALGO_SENTIMENTOVERTIME_HPP_
#define MODULE_ANALYZER_ALGO_SENTIMENTOVERTIME_HPP_

#include "../Thread.hpp"

#include "../../../Data/Data.hpp"
#include "../../../Data/Dictionary.hpp"
#include "../../../Data/Sentiment.hpp"
#include "../../../Helper/DateTime.hpp"
#include "../../../Helper/FileSystem.hpp"
#include "../../../Main/Database.hpp"
#include "../../../Struct/QueryStruct.hpp"
#include "../../../Struct/StatusSetter.hpp"
#include "../../../Struct/TextMap.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"

#include <algorithm>		// std::all_of, std::find_if
#include <cmath>			// std::fabs, std::round
#include <cstddef>			// std::size_t
#include <cstdint>			// std::uint8_t, std::uint64_t
#include <limits>			// std::numeric_limits
#include <map>				// std::map
#include <memory>			// std::make_unique, std::unique_ptr
#include <queue>			// std::queue
#include <string>			// std::string
#include <string_view>		// std::string_view, std::string_view_literals
#include <unordered_map>	// std::unordered_map
#include <unordered_set>	// std::unordered_set
#include <utility>			// std::pair
#include <vector>			// std::vector

namespace crawlservpp::Module::Analyzer::Algo {

	using std::string_view_literals::operator""sv;

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Indicates, while calculating, after how many sentences the progress of the thread will be updated.
	inline constexpr auto sentimentUpdateCalculateProgressEvery{250000};

	//! Indicates, while saving, after how many rows the progress of the thread will be updated.
	inline constexpr auto sentimentUpdateSavingProgressEvery{10};

	//! Number of default columns to be written to the target table.
	inline constexpr auto sentimentMinNumColumns{1};

	//! Number of columns per category if article-based sentiment is deactivated.
	inline constexpr auto sentimentMinColumnsPerCategory{2};

	//! Number of columns per category if article-based sentiment is activated.
	inline constexpr auto sentimentArticleColumnsPerCategory{4};

	//! The default threshold (sentiments lower than that number will be ignored).
	inline constexpr auto sentimentDefaultThreshold{10};

	//! The default sentiment dictionary to be used.
	inline constexpr auto sentimentDictionary{"sentiment-en"sv};

	//! The default emoji dictionary to be used.
	inline constexpr auto sentimentEmojis{"emojis-en"sv};

	//! Factor to convert value to percentage.
	inline constexpr auto sentimentPercentageFactor{100.F};

	//@}

	/*
	 * DECLARATION
	 */

	//! Sentiment analysis using the VADER algorithm.
	/*!
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
	 * \warning For English language only!
 	 *
	 */
	class SentimentOverTime final : public Module::Analyzer::Thread {
		// internal structure for temporarily saving data linked to a specific date and category
		struct DateCategoryData {
			// sum of all sentence-based sentiment scores
			double sentimentSum{};

			// count of all sentence-based sentiment scores
			std::uint64_t sentimentCount{};

			// (if needed) articles associated with this date and containing this category
			std::unordered_set<std::string> articles;
		};

		// for convenience
		using Exception = Module::Analyzer::Thread::Exception;

		using QueryStruct = Struct::QueryStruct;
		using StatusSetter = Struct::StatusSetter;
		using TextMap = Struct::TextMap;
		using TextMapEntry = Struct::TextMapEntry;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		using DoubleUInt = std::pair<double, std::uint64_t>;
		using StringString = std::pair<std::string, std::string>;

		using ArticleData = std::unordered_map<std::string, DoubleUInt>;
		using DateData = std::map<std::string, std::vector<DateCategoryData>>;

	public:
		///@name Construction
		///@{

		SentimentOverTime(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		SentimentOverTime(
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
		// algorithm options
		struct Entries {
			std::vector<std::string> categoryLabels;
			std::vector<std::uint64_t> categoryQueries;
			std::uint8_t threshold{sentimentDefaultThreshold};
			bool addArticleSentiment{false};
			bool ignoreEmptyDate{true};
			bool useThreshold{false};
			std::string dictionary{sentimentDictionary};
			std::string emojis{sentimentEmojis};
		} algoConfig;

		// sentiment analyzer
		std::unique_ptr<Data::Sentiment> sentimentAnalyzer;

		// algorithm queries
		std::vector<QueryStruct> queriesCategories;

		// algorithm state
		std::size_t currentCorpus{};
		DateData dateData;
		ArticleData articleData;

		// algorithm functions
		void addCurrent();
		void saveSentiments();

		// query function
		void initQueries() override;

		// internal helper functions
		[[nodiscard]] DateData::iterator addDate(const std::string& date);
		void processSentence(
				const std::vector<std::string>& tokens,
				const std::pair<std::size_t, std::size_t>& sentence,
				const DateData::iterator& dateIt,
				const std::string& article
		);
		[[nodiscard]] float getSentenceScore(
				const std::pair<std::size_t, std::size_t>& sentence,
				const std::vector<std::string>& tokens
		);
		[[nodiscard]] DoubleUInt calculateArticleSentiment(
				const std::unordered_set<std::string>& articles
		);
		[[nodiscard]] DoubleUInt calculateArticle(
				const std::string& article
		);

		// static internal helper functions
		static bool selectFirst(
				const TextMap& map,
				std::size_t& numberTo
		);
		static bool identifyCurrent(
				std::size_t sentenceBegin,
				std::size_t& numberFromTo,
				const TextMap& map,
				bool& isLastFromTo
		);
		[[nodiscard]] static bool meetsThreshold(
				float sentiment,
				std::uint8_t threshold
		);
	};

} /* namespace crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_SENTIMENTOVERTIME_HPP_ */
