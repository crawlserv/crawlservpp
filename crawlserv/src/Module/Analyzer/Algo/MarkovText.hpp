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
 * MarkovText.hpp
 *
 *  Created on: Jan 13, 2019
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_MARKOVTEXT_HPP_
#define MODULE_ANALYZER_ALGO_MARKOVTEXT_HPP_

#include "../Thread.hpp"

#include "../../../Data/Corpus.hpp"
#include "../../../Data/Data.hpp"
#include "../../../Main/Database.hpp"
#include "../../../Struct/CorpusProperties.hpp"
#include "../../../Struct/TextMap.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"
#include "../../../Timer/Simple.hpp"

#include <algorithm>	// std::find
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint8_t, std::uint32_t, std::uint64_t
#include <iterator>		// std::advance
#include <map>			// std::map
#include <memory>		// std::make_unique, std::unique_ptr
#include <random>		// std::minstd_rand
#include <string>		// std::string
#include <vector>		// std::vector

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Default dimension parameter for the algorithm.
	constexpr auto markovTextDefaultDimension{3};

	//! Default length of the generated texts.
	constexpr auto markovTextDefaultLength{400};

	//! Default name of the column in the target table for the generated texts to be written to.
	constexpr auto markovTextDefaultResultField{"text"};

	//! Default name of the column in the target table for the number of source texts to be written to.
	constexpr auto markovTextDefaultSourcesField{"sources"};

	//! ASCII code for a space.
	constexpr auto markovTextAsciiSpace{32};

	//! Number of iterations of the algorithm loop before the progress will be refreshed.
	constexpr auto markovTextRefreshProgressEvery{1000000};

	//! Average word lenght the algorithm expects in the processed texts.
	constexpr auto markovTextGuessedWordLength{10};

	//@}

	/*
	 * DECLARATION
	 */

	//! Algorithm generating random texts from a text corpus.
	/*!
	 * This is a semi-serious proof-of-concept
	 *  class for the crawlserv++ Analyzer module.
	 *
	 * The implementation of the algorithm is
	 *  taken from
	 *  <a href="https://rosettacode.org/wiki/Markov_chain_text_generator>
	 *  Rosetta Code</a>.
	 *
	 *  \deprecated This class is deprecated
	 *    and will be removed in the future.
	 */
	class MarkovText : public Module::Analyzer::Thread {
		// for convenience
		using Exception = Module::Analyzer::Thread::Exception;

		using CorpusProperties = Struct::CorpusProperties;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

	public:
		///@name Construction
		///@{

		MarkovText(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		MarkovText(
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
		std::random_device randDevice;
		std::minstd_rand randGenerator{randDevice()};

		std::string source;
		std::map<std::string, std::vector<std::string>> dictionary;
		std::size_t sources{0};

		// algorithm options
		std::uint8_t markovTextDimension{markovTextDefaultDimension};
		std::uint64_t markovTextLength{markovTextDefaultLength};
		std::uint64_t markovTextMax{0};
		std::string markovTextResultField{markovTextDefaultResultField};
		std::uint64_t markovTextSleep{0};
		std::string markovTextSourcesField{markovTextDefaultSourcesField};
		bool markovTextTiming{true};

		// algorithm functions
		void createDictionary();
		std::string createText();
	};

} /* namespace crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_MARKOVTEXT_HPP_ */
