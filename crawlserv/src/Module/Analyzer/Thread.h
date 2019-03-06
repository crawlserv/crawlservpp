/*
 * Thread.h
 *
 * Abstract implementation of the Thread interface for analyzer threads to be inherited by the algorithm class.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_THREAD_H_
#define MODULE_ANALYZER_THREAD_H_

#include "Config.h"
#include "Database.h"

#include "../Thread.h"

#include "../../Helper/DateTime.h"
#include "../../Helper/Strings.h"
#include "../../Struct/ThreadOptions.h"
#include "../../Timer/StartStop.h"

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
	class Thread: public Module::Thread {
		// for convenience
		typedef Struct::ThreadOptions ThreadOptions;

	public:
		Thread(Main::Database& database, unsigned long analyzerId, const std::string& analyzerStatus,
				bool analyzerPaused, const ThreadOptions& threadOptions, unsigned long analyzerLast);
		Thread(Main::Database& database, const ThreadOptions& threadOptions);
		virtual ~Thread();

	protected:
		// analyzing configuration and database functionality for thread
		Config config;
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
}

#endif /* MODULE_ANALYZER_THREAD_H_ */
