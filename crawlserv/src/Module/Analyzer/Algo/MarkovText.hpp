/*
 *
 * ---
 *
 *  Copyright (C) 2019-2020 Anselm Schmidt (ans[ät]ohai.su)
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
 *   Markov Chain Text Generator algorithm implemented as child of the abstract Analyzer thread class.
 *
 *   This is a semi-serious proof-of-concept class for the crawlserv++ Analyzer module.
 *
 *   It uses the markov chain algorithm to generate random texts from a large text corpus previously parsed.
 *
 *   The implementation of the algorithm itself is taken from Rosetta Code at https://rosettacode.org/wiki/Markov_chain_text_generator
 *
 *
 *  Created on: Jan 13, 2019
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_MARKOVTEXT_HPP_
#define MODULE_ANALYZER_ALGO_MARKOVTEXT_HPP_

#include "../Thread.hpp"

#include "../../../Data/Corpus.hpp"
#include "../../../Data/Data.hpp"
#include "../../../Struct/CorpusProperties.hpp"
#include "../../../Struct/TextMap.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"
#include "../../../Timer/Simple.hpp"

#include <algorithm>	// std::find
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint8_t, std::uint64_t
#include <iterator>		// std::advance
#include <map>			// std::map
#include <memory>		// std::make_unique, std::unique_ptr
#include <string>		// std::string
#include <vector>		// std::vector

namespace crawlservpp::Module::Analyzer::Algo {

	class MarkovText : public Module::Analyzer::Thread {
		// for convenience
		using DataType = Data::Type;
		using DataValue = Data::Value;

		using Exception = Module::Analyzer::Thread::Exception;

		using CorpusProperties = Struct::CorpusProperties;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

	public:
		MarkovText(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		MarkovText(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions
		);
		virtual ~MarkovText();

		// implemented algorithm functions
		void onAlgoInit() override;
		void onAlgoTick() override;
		void onAlgoPause() override;
		void onAlgoUnpause() override;
		void onAlgoClear() override;

		// overwritten configuration functions
		void parseAlgoOption() override;
		void checkAlgoOptions() override;

	private:
		std::string source;
		std::map<std::string, std::vector<std::string>> dictionary;
		std::size_t sources;

		// algorithm options
		std::uint8_t markovTextDimension;
		std::uint64_t markovTextLength;
		std::uint64_t markovTextMax;
		std::string markovTextResultField;
		std::uint64_t markovTextSleep;
		std::string markovTextSourcesField;
		bool markovTextTiming;

		// algorithm functions
		std::string createText();
		void createDictionary();
	};

} /* crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_MARKOVTEXT_HPP_ */
