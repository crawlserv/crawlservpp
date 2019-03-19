/*
 * Thread.hpp
 *
 * Interface for a thread which implements all module-independent thread functionality like connecting to the database,
 *  managing the thread status (including pausing the thread), run the thread ticks and catching exceptions thrown by the thread.
 *
 *  Created on: Oct 10, 2018
 *      Author: ans
 */

#ifndef MODULE_THREAD_HPP_
#define MODULE_THREAD_HPP_

// do not catch thread exceptions: use only for debugging!
//#define MODULE_THREAD_DEBUG_NOCATCH

// hard-coded constant
#define MODULE_THREAD_SLEEP_ON_CONNECTION_ERROR_SEC 10

#include "Database.hpp"

#include "../Main/Database.hpp"
#include "../Helper/DateTime.hpp"
#include "../Struct/ThreadOptions.hpp"
#include "../Wrapper/DatabaseLock.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace crawlservpp::Module {

	class Thread {
		// for convenience
		typedef Main::Database::ConnectionException ConnectionException;
		typedef Struct::ThreadOptions ThreadOptions;
		typedef Wrapper::DatabaseLock<Database> DatabaseLock;

	public:
		// constructors
		Thread(Main::Database& dbBase, unsigned long threadId, const std::string& threadModule,
				const std::string& threadStatus, bool threadPaused, const ThreadOptions& threadOptions,
				unsigned long threadLast);
		Thread(Main::Database& dbBase, const std::string& threadModule, const ThreadOptions& threadOptions);

		// destructor
		virtual ~Thread();

		// getters
		unsigned long getId() const;
		unsigned long getWebsite() const;
		unsigned long getUrlList() const;
		unsigned long getConfig() const;
		bool isShutdown() const;
		bool isRunning() const;
		bool isFinished() const;

		// control functions
		void start();
		bool pause();
		void unpause();
		void stop();
		void interrupt();
		void end();

		// not moveable, not copyable
		Thread(Thread&) = delete;
		Thread(Thread&&) = delete;
		Thread& operator=(Thread&) = delete;
		Thread& operator=(Thread&&) = delete;

	protected:
		Database database; // access to the database for the thread

		std::string websiteNamespace; // namespace of website
		std::string urlListNamespace; // namespace of URL list
		std::string configuration; // configuration

		// thread helper functions
		void pauseByThread();
		void setStatusMessage(const std::string& statusMessage);
		void setProgress(float progress);
		void log(const std::string& entry);
		void allowPausing();
		void disallowPausing();

		unsigned long getLast() const;
		void setLast(unsigned long last);
		void incrementLast();
		std::string getStatusMessage();

		virtual void onInit(bool resumed) = 0;
		virtual void onTick() = 0;
		virtual void onPause() = 0;
		virtual void onUnpause() = 0;
		virtual void onClear(bool interrupted) = 0;

	private:
		Main::Database& databaseClass;	// access to the database for the class

		std::atomic<bool> pausable;		// thread is pausable
 		std::atomic<bool> running;		// thread is running (or paused)
		std::atomic<bool> paused;		// thread is paused
		std::atomic<bool> interrupted;	// thread has been interrupted by shutdown
		std::atomic<bool> resumed;		// thread has been resumed after interruption by shutdown
		std::atomic<bool> terminated;	// thread has been terminated due to an exception
		std::atomic<bool> shutdown;		// shutdown in progress
		std::atomic<bool> finished;		// shutdown is finished

		const std::string module; // the module of the thread (used for logging)
		std::atomic<unsigned long> id; //  of the thread in the database
		const ThreadOptions options; // options for the thread
		unsigned long last; // last  for the thread
		std::string idString; //  of the thread as string (used for logging, ONLY for threads!)

		std::condition_variable pauseCondition; // condition variable to wait for unpause
		std::mutex pauseLock;

		std::string status; // status message of the thread (without pause state)
		std::mutex statusLock;

		std::thread thread; // pointer to the thread

		// timing statistics (in seconds)
		std::chrono::steady_clock::time_point startTimePoint;
		std::chrono::steady_clock::time_point pauseTimePoint;
		std::chrono::duration<unsigned long> runTime;
		std::chrono::duration<unsigned long> pauseTime;
		void updateRunTime();
		void updatePauseTime();

		// internal thread functions
		void init();
		void tick();
		void wait();
		void clear();

		// pause checker
		bool isUnpaused() const;

		// private helper functions
		void onEnd();
		void clearException(const std::exception& e, const std::string& inFunction);
		void clearException(const std::string& inFunction);

		// main function
		void main();
	};

} /* crawlservpp::Module */

#endif /* MODULE_THREAD_HPP_ */
