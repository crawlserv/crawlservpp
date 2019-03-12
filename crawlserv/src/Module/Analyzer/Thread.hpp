/*
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
#include "../../Struct/ThreadOptions.hpp"
#include "../../Timer/StartStop.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <locale>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace crawlservpp::Module::Analyzer {

	class Thread: public Module::Thread, public Config {
		// for convenience
		typedef Struct::ThreadOptions ThreadOptions;

	public:
		Thread(Main::Database& database, unsigned long analyzerId, const std::string& analyzerStatus,
				bool analyzerPaused, const ThreadOptions& threadOptions, unsigned long analyzerLast);
		Thread(Main::Database& database, const ThreadOptions& threadOptions);
		virtual ~Thread();

	protected:
		// analyzing configuration and database functionality for thread
		Database database;

		// target table
		std::string targetTable;

		// implemented thread functions
		void onInit(bool resumed) override;
		void onTick() override;
		void onPause() override;
		void onUnpause() override;
		void onClear(bool interrupted) override;

		// functions to be implemented by algorithm class
		virtual void onAlgoInit(bool resumed) = 0;
		virtual void onAlgoTick() = 0;
		virtual void onAlgoPause() = 0;
		virtual void onAlgoUnpause() = 0;
		virtual void onAlgoClear(bool interrupted) = 0;

		// algorithm is finished
		void finished();

		// shadow pause function not to be used by thread
		void pause();

	private:
		// hide other functions not to be used by thread
		void start();
		void unpause();
		void stop();
		void interrupt();
	};

} /* crawlservpp::Module::Analyzer */

#endif /* MODULE_ANALYZER_THREAD_HPP_ */
