/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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
 * Thread.hpp
 *
 * Abstract implementation of the Thread interface for analyzer threads to be inherited by the algorithm class.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_THREAD_HPP_
#define MODULE_ANALYZER_THREAD_HPP_

#include "Config.hpp"
#include "Database.hpp"

#include "../Config.hpp"
#include "../Thread.hpp"

#include "../../Helper/DateTime.hpp"
#include "../../Helper/Strings.hpp"
#include "../../Main/Exception.hpp"
#include "../../Struct/ThreadOptions.hpp"
#include "../../Struct/ThreadStatus.hpp"
#include "../../Timer/StartStop.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <locale>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace crawlservpp::Module::Analyzer {

	class Thread: public Module::Thread, protected Config {
		// for convenience
		typedef Struct::ThreadOptions ThreadOptions;
		typedef Struct::ThreadStatus ThreadStatus;

	public:
		Thread(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		Thread(Main::Database& dbBase, const ThreadOptions& threadOptions);
		virtual ~Thread();

	protected:
		// analyzing configuration and database functionality for thread
		Database database;

		// target table
		std::string targetTable;

		// implemented thread functions
		void onInit() override;
		void onTick() override;
		void onPause() override;
		void onUnpause() override;
		void onClear() override;

		// functions to be implemented by algorithm class
		virtual void onAlgoInit() = 0;
		virtual void onAlgoTick() = 0;
		virtual void onAlgoPause() = 0;
		virtual void onAlgoUnpause() = 0;
		virtual void onAlgoClear() = 0;

		// algorithm is finished
		void finished();

		// shadow pause function not to be used by thread
		void pause();

		// class for Analyzer exceptions (to be used by algorithms)
		MAIN_EXCEPTION_CLASS();

	private:
		// hide other functions not to be used by thread
		void start();
		void unpause();
		void stop();
		void interrupt();
	};

} /* crawlservpp::Module::Analyzer */

#endif /* MODULE_ANALYZER_THREAD_HPP_ */
