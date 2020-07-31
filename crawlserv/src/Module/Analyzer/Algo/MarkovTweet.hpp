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
 *  Created on: Jan 13, 2019
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_MARKOVTWEET_HPP_
#define MODULE_ANALYZER_ALGO_MARKOVTWEET_HPP_

#include "../Thread.hpp"

#include "../../../Data/Data.hpp"
#include "../../../Main/Database.hpp"
#include "../../../Struct/CorpusProperties.hpp"
#include "../../../Struct/TextMap.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"
#include "../../../Timer/Simple.hpp"

#include "../../../_extern/rawr/rawr.h"

#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint8_t, std::uint16_t, std::uint64_t
#include <string>		// std::string
#include <vector>		// std::vector


namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Default dimension parameter for the algorithm.
	constexpr auto markovTweetDefaultDimension{5};

	//! Default language for the algorithm.
	constexpr auto markovTweetDefaultLanguage{"en_US"};

	//! Default length of the generated texts.
	constexpr auto markovTweetDefaultLength{140};

	//! Default name of the column in the target table for the generated texts to be written to.
	constexpr auto markovTweetDefaultResultField{"tweet"};

	//! Default name of the column in the target table for the number of source texts to be written to.
	constexpr auto markovTweetDefaultSourcesField{"sources"};

	//@}

	/*
	 * DECLARATION
	 */

	//! Algorithm generate random tweet texts from a text corpus.
	/*!
	 * This is a semi-serious proof-of-concept
	 *  class for the crawlserv++ Analyzer module.
	 *
	 * The implementation of the algorithm itself
	 *  is done by the slightly modified @c rawr
	 *  class, originally by Kelly Rauchenberger
	 *   – see the
	 *   <a href="https://github.com/hatkirby/rawr-ebooks">
	 *   GitHub repository</a>. 👌
	 *
	 * \warning This algorithm may use A LOT of
	 *   memory when parsing large corpora, adjust
	 *   your swap size accordingly to prevent the
	 *   server from being killed by the operating
	 *   system!
	 */
	class MarkovTweet : public Module::Analyzer::Thread {
		// for convenience
		using Exception = Module::Analyzer::Thread::Exception;

		using CorpusProperties = Struct::CorpusProperties;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

	public:
		///@name Construction
		///@{

		MarkovTweet(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		MarkovTweet(
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
		rawr generator;
		std::size_t sources{0};

		// algorithm options
		std::uint8_t markovTweetDimension{markovTweetDefaultDimension};
		std::string markovTweetLanguage{markovTweetDefaultLanguage};
		std::uint64_t markovTweetLength{markovTweetDefaultLength};
		std::uint64_t markovTweetMax{0};
		std::string markovTweetResultField{markovTweetDefaultResultField};
		std::uint64_t markovTweetSleep{0};
		std::string markovTweetSourcesField{markovTweetDefaultSourcesField};
		bool markovTweetSpellcheck{true};
		bool markovTweetTiming{true};
	};

}  /* namespace crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_MARKOVTWEET_HPP_ */
