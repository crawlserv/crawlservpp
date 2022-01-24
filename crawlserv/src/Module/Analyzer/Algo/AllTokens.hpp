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

#ifndef MODULE_ANALYZER_ALGO_ALLTOKENS_HPP_
#define MODULE_ANALYZER_ALGO_ALLTOKENS_HPP_

#include "../Thread.hpp"

#include "../../../Data/Corpus.hpp"
#include "../../../Data/Data.hpp"
#include "../../../Helper/Memory.hpp"
#include "../../../Main/Database.hpp"
#include "../../../Struct/StatusSetter.hpp"
#include "../../../Struct/TextMap.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"
#include "../../../Timer/Simple.hpp"

#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint32_t
#include <map>			// std::map
#include <string>		// std::string, std::to_string
#include <string_view>	// std::string_view
#include <utility>		// std::pair

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The number of columns in the tokens table.
	inline constexpr auto allTokensColumns{2};

	//! Indicates after how many dates the status will be updated, if a date map is available.
	inline constexpr auto allTokensUpdateEveryDate{100U};

	//! Indicates after how many articles the status will be updated, if no date map, but an article map is available.
	inline constexpr auto allTokensUpdateEveryArticle{1000U};

	//! Indicates after how many tokens the status will be updated, if no date and no article map is available.
	inline constexpr auto allTokensUpdateEveryToken{10000U};

	//! Indicates after how many rows the status will be updated while saving the results to the database.
	inline constexpr auto allTokensUpdateEveryRow{1000U};

	//@}

	/*
	 * DECLARATION
	 */

	//! Counts all tokens in a corpus.
	/*!
	 * Tokens will be counted by date and/or article, if possible.
	 */
	class AllTokens final : public Module::Analyzer::Thread {
		// for convenience
		using Exception = Module::Analyzer::Thread::Exception;

		using StatusSetter = Struct::StatusSetter;
		using TextMapEntry = Struct::TextMapEntry;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		using StringString = std::pair<std::string, std::string>;

		using TokenMap = std::map<std::string, std::size_t>;
		using TokenCounts = std::map<std::size_t, std::size_t>;
		using SingleMap = std::map<std::string, TokenCounts>;
		using DoubleMap = std::map<std::string, SingleMap>;

	public:
		///@name Construction
		///@{

		AllTokens(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		AllTokens(
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
			std::string countTable;
		} algoConfig;

		// algorithm state
		enum OrderBy {
			NONE = 0,
			ARTICLES,
			DATES
		} orderBy{NONE};

		std::size_t total{};
		std::size_t articleCount{};
		std::size_t count{};
		std::size_t updateCount{};
		std::size_t countsTable{};

		bool hasArticles{false};
		bool done{false};
		bool firstTick{true};

		// data
		TokenMap tokens;
		TokenCounts tokenCounts;
		SingleMap singleMap;
		DoubleMap doubleMap;

		// algorithm functions
		void nextDate();
		void nextArticle();
		void nextToken();
		void clearCorpus();
		void saveData();

		// internal helper functions
		void updateProgress(std::uint32_t every);
		void saveTokens();
		void saveCounts();
		void saveDouble();
		void saveSingle(const std::string& typeName);
		void saveTokenCounts();
		void initCountsTable();

		// static internal helper functions
		static void processSingle(
				const std::vector<std::string>& corpusTokens,
				const TextMapEntry& entry,
				TokenMap& tokenMap,
				SingleMap& to
		);
		static void processDouble(
				const TextMapEntry& entry,
				SingleMap& from,
				DoubleMap& to
		);
		static void processToken(
				const std::string& token,
				TokenMap& tokenMap,
				TokenCounts& to
		);
		static void addTokenCounts(
				const TokenCounts& from,
				Data::InsertFieldsMixed& to
		);

		// internal helper template
		template<typename T> bool isDone(const T& container) {
			if(this->count >= container.size()) {
				this->done = true;

				return true;
			}

			return false;
		}
	};

} /* namespace crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_ALLTOKENS_HPP_ */
