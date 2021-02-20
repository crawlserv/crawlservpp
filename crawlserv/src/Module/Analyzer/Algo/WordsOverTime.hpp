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
 * WordsOverTime.hpp
 *
 * Counts the occurrence of articles, sentences, and words in a corpus
 *  over time.
 *
 *  Created on: Jan 03, 2021
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_WORDSOVERTIME_HPP_
#define MODULE_ANALYZER_ALGO_WORDSOVERTIME_HPP_

#include "../Thread.hpp"

#include "../../../Data/Data.hpp"
#include "../../../Helper/DateTime.hpp"
#include "../../../Helper/Memory.hpp"
#include "../../../Main/Database.hpp"
#include "../../../Struct/StatusSetter.hpp"
#include "../../../Struct/TextMap.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"

#include <cstddef>			// std::size_t
#include <cstdint>			// std::uint64_t
#include <limits>			// std::numeric_limits
#include <map>				// std::map
#include <string>			// std::string
#include <string_view>		// std::string_view
#include <unordered_set>	// std::unordered_set
#include <utility>			// std::pair
#include <vector>			// std::vector

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Indicates after how many date groups the progress of the thread will be updated.
	inline constexpr auto wordsUpdateProgressEvery{100};

	//! The number of columns to write to the target table.
	inline constexpr auto wordsNumberOfColumns{4};

	//@}

	/*
	 * DECLARATION
	 */

	//! Counts the occurrence of articles, sentences, and words in a corpus over time.
	class WordsOverTime final : public Module::Analyzer::Thread {
		// structure for results
		struct DateResults {
			std::unordered_set<std::string> articles;
			std::uint64_t sentences{};
			std::uint64_t words{};
		};

		// for convenience
		using Exception = Module::Analyzer::Thread::Exception;

		//using QueryStruct = Struct::QueryStruct;
		using StatusSetter = Struct::StatusSetter;
		using TextMapEntry = Struct::TextMapEntry;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		using ResultMap = std::map<std::string, DateResults>;
		using StringString = std::pair<std::string, std::string>;

	public:
		///@name Construction
		///@{

		WordsOverTime(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		WordsOverTime(
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
		// algorithm state
		bool firstTick{true};

		// results
		std::map<std::string, DateResults> dateResults;

		// algorithm functions
		void count();
		void save();

		// internal helper functions
		ResultMap::iterator addDateGroup(const std::string& group);
	};

} /* namespace crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_WORDSOVERTIME_HPP_ */
