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
 * TermsOverTime.hpp
 *
 * Count occurrences of specific terms in a text corpus over time.
 *
 *  Created on: Aug 2, 2020
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_TERMSOVERTIME_HPP_
#define MODULE_ANALYZER_ALGO_TERMSOVERTIME_HPP_

#include "../Thread.hpp"

#include "../../../Data/Corpus.hpp"
#include "../../../Helper/Memory.hpp"
#include "../../../Main/Database.hpp"
#include "../../../Struct/StatusSetter.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"

#include <cstdint>			// std::uint64_t
#include <limits>			// std::numeric_limits
#include <string>			// std::string
#include <string_view>		// std::string_view
#include <unordered_map>	// std::unordered_map
#include <vector>			// std::vector

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * DECLARATION
	 */

	//! Algorithm counting specific terms in a text corpus over time.
	/*
	 * \todo Not implemented yet.
	 */
	class TermsOverTime final : public Module::Analyzer::Thread {
		// for convenience
		using DataType = Data::Type;
		using DataValue = Data::Value;

		using Exception = Module::Analyzer::Thread::Exception;

		using StatusSetter = Struct::StatusSetter;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		using DateOccurrences = std::unordered_map<std::string, std::uint64_t>;
		using DateArticlesOccurrences = std::unordered_map<std::string, DateOccurrences>;

	public:
		///@name Construction
		///@{

		TermsOverTime(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		TermsOverTime(
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
			//TODO add additional algo options
		} algoConfig;

		// algorithm state
		bool firstTick{true};

		// counts
		std::vector<DateArticlesOccurrences> dateCounts;

		// internal helper functions
		void count();
		void save();
	};

} /* namespace crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_TERMSOVERTIME_HPP_ */
