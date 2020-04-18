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
 * MarkovTweet.hpp
 *
 *  Markov Chain Tweet Generator algorithm implemented as child of the abstract Analyzer thread class.
 *
 *   This is a semi-serious proof-of-concept class for the crawlserv++ Analyzer module.
 *
 *   It uses the markov chain algorithm to generate random tweet texts from a large text corpus previously parsed.
 *
 *   The implementation of the algorithm itself is done by the slightly modified rawr class, originally by Kelly Rauchenberger
 *   at https://github.com/hatkirby/rawr-ebooks 👌
 *
 *   WARNING: This algorithm may use A LOT of memory when parsing large corpora, adjust your swap size accordingly to prevent
 *   		 	the server from being killed by the operating system!
 *
 *
 *  Created on: Jan 13, 2019
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_MARKOVTWEET_HPP_
#define MODULE_ANALYZER_ALGO_MARKOVTWEET_HPP_

#include "../Thread.hpp"

#include "../../../Data/Data.hpp"
#include "../../../Struct/CorpusProperties.hpp"
#include "../../../Struct/TextMap.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"
#include "../../../Timer/Simple.hpp"

#include "../../../_extern/rawr/rawr.h"

#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint8_t, std::uint64_t
#include <functional>	// std::bind, std::placeholders
#include <string>		// std::string
#include <vector>		// std::vector


namespace crawlservpp::Module::Analyzer::Algo {

	class MarkovTweet : public Module::Analyzer::Thread {
		// for convenience
		using DataType = Data::Type;
		using DataValue = Data::Value;

		using Exception = Module::Analyzer::Thread::Exception;

		using CorpusProperties = Struct::CorpusProperties;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

	public:
		MarkovTweet(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		MarkovTweet(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions
		);
		virtual ~MarkovTweet();

		// implemented algorithm functions
		void onAlgoInit() override;
		void onAlgoTick() override;
		void onAlgoPause() override;
		void onAlgoUnpause() override;
		void onAlgoClear() override;

		// overwritten configuration functions
		void parseAlgoOption() override;
		void checkAlgoOptions() override;

		// external access to thread functionality for rawr
		bool _isRunning();
		void _setStatus(const std::string& status);
		void _setProgress(float progress);
		void _log(unsigned short level, const std::string& entry);

	private:
		rawr generator;
		std::size_t sources;

		// algorithm options
		std::uint8_t markovTweetDimension;
		std::string markovTweetLanguage;
		std::uint64_t markovTweetLength;
		std::uint64_t markovTweetMax;
		std::string markovTweetResultField;
		std::uint64_t markovTweetSleep;
		std::string markovTweetSourcesField;
		bool markovTweetSpellcheck;
		bool markovTweetTiming;
	};

}  /* crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_MARKOVTWEET_HPP_ */
