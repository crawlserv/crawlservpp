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
 * CorpusGenerator.hpp
 *
 *   This algorithm just uses the built-in functionality for building text corpora from its input data and quits.
 *
 *
 *  Created on: Mar 5, 2020
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_CORPUSGENERATOR_HPP_
#define MODULE_ANALYZER_ALGO_CORPUSGENERATOR_HPP_

#include "../Thread.hpp"

#include "../../../Data/Corpus.hpp"
#include "../../../Main/Database.hpp"
#include "../../../Struct/CorpusProperties.hpp"
#include "../../../Struct/StatusSetter.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"

#include <cstddef>	// std::size_t
#include <cstdint>	// std::uint64_t
#include <limits>	// std::numeric_limits
#include <locale>	// std::locale
#include <sstream>	// std::ostringstream
#include <string>	// std::string

namespace crawlservpp::Module::Analyzer::Algo {

	//! Algorithm building a text corpus from the input data.
	class CorpusGenerator : public Module::Analyzer::Thread {
		// for convenience
		using DataType = Data::Type;
		using DataValue = Data::Value;

		using Exception = Module::Analyzer::Thread::Exception;

		using CorpusProperties = Struct::CorpusProperties;
		using StatusSetter = Struct::StatusSetter;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

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
		///@name Implemented Algorithm Functions
		///@{

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

		///@}

	private:
		std::string status;
	};

} /* namespace crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_CORPUSGENERATOR_HPP_ */
