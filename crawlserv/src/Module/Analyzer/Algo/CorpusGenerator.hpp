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
 * CorpusGenerator.hpp
 *
 * This algorithm uses the built-in functionality for building text
 *  corpora from its input data.
 *
 * Additionally, it writes some basic statistics (number and length of
 *  tokens and sentences) to the target table.
 *
 *  Created on: Mar 5, 2020
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_CORPUSGENERATOR_HPP_
#define MODULE_ANALYZER_ALGO_CORPUSGENERATOR_HPP_

#include "../Thread.hpp"

#include "../../../Data/Corpus.hpp"
#include "../../../Helper/Math.hpp"
#include "../../../Helper/Memory.hpp"
#include "../../../Helper/Utf8.hpp"
#include "../../../Main/Database.hpp"
#include "../../../Struct/StatusSetter.hpp"
#include "../../../Struct/TextMap.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"

#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint64_t
#include <limits>		// std::numeric_limits
#include <string>		// std::string
#include <string_view>	// std::string_view
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Number of target fields
	inline constexpr auto corpusNumFields{9};

	//@}

	/*
	 * DECLARATION
	 */

	//! Algorithm building a text corpus and creating corpus statistics from the input data.
	/*!
	 * \note Empty tokens and sentences will be ignored.
	 */
	class CorpusGenerator final : public Module::Analyzer::Thread {
		// for convenience
		using DataType = Data::Type;
		using DataValue = Data::Value;

		using Exception = Module::Analyzer::Thread::Exception;

		using StatusSetter = Struct::StatusSetter;
		using TextMapEntry = Struct::TextMapEntry;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		using StringString = std::pair<std::string, std::string>;

	public:
		///@name Construction
		///@{

		CorpusGenerator(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		CorpusGenerator(
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
		// status message saved in-class
		std::string status;

		// internal static helper function
		static bool isSentenceEmpty(
				const std::pair<std::size_t, std::size_t>& sentence,
				const std::vector<std::string>& tokens
		);
	};

} /* namespace crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_CORPUSGENERATOR_HPP_ */
