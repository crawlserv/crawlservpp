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
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace crawlservpp::Module::Analyzer {
	class Thread: public crawlservpp::Module::Thread {
	public:
		Thread(crawlservpp::Main::Database& database, unsigned long analyzerId, const std::string& analyzerStatus,
				bool analyzerPaused, const crawlservpp::Struct::ThreadOptions& threadOptions, unsigned long analyzerLast);
		Thread(crawlservpp::Main::Database& database, const crawlservpp::Struct::ThreadOptions& threadOptions);
		virtual ~Thread();

	protected:
		// analyzing configuration and database functionality for thread
		Config config;
		Database database;

		// target table
		std::string targetTable;

		// implemented thread functions
		bool onInit(bool resumed) override;
		bool onTick() override;
		void onPause() override;
		void onUnpause() override;
		void onClear(bool interrupted) override;

		// functions to be implemented by algorithm class
		virtual bool onAlgoInit(bool resumed) = 0;
		virtual bool onAlgoTick() = 0;
		virtual void onAlgoPause() = 0;
		virtual void onAlgoUnpause() = 0;
		virtual void onAlgoClear(bool interrupted) = 0;

		// algorithm is finished
		void finished();

	private:
		// hide functions not to be used by thread
		void start();
		void pause();
		void unpause();
		void stop();
		void interrupt();
	};
}

#endif /* MODULE_ANALYZER_THREAD_H_ */
