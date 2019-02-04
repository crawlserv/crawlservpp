/*
 * Thread.h
 *
 * Interface for a thread which implements all module-independent thread functionality like connecting to the database,
 *  managing the thread status (including pausing the thread), run the thread ticks and catching exceptions thrown by the thread.
 *
 *  Created on: Oct 10, 2018
 *      Author: ans
 */

#ifndef MODULE_THREAD_H_
#define MODULE_THREAD_H_

// do not catch thread errors: use only for debugging!
//#define MODULE_THREAD_DEBUG_NOCATCH

#include "DBThread.h"

#include "../Main/Database.h"
#include "../Helper/DateTime.h"
#include "../Struct/ThreadOptions.h"

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

namespace crawlservpp::Module {
	class Thread {
	public:
		Thread(crawlservpp::Main::Database& dbBase, unsigned long threadId, const std::string& threadModule,
				const std::string& threadStatus, bool threadPaused, const crawlservpp::Struct::ThreadOptions& threadOptions,
				unsigned long threadLast);
		Thread(crawlservpp::Main::Database& dbBase, const std::string& threadModule,
				const crawlservpp::Struct::ThreadOptions& threadOptions);
		virtual ~Thread();

		void start();
		bool pause();
		void unpause();
		void stop();
		void sendInterrupt();
		void finishInterrupt();

		unsigned long getId() const;
		unsigned long getWebsite() const;
		unsigned long getUrlList() const;
		unsigned long getConfig() const;
		bool isTerminated() const;
		bool isRunning() const;

	protected:
		DBThread database; // access to the database for the thread

		std::string websiteNamespace; // namespace of website
		std::string urlListNamespace; // namespace of URL list
		std::string configuration; // configuration

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
		crawlservpp::Main::Database& databaseClass; // access to the database for the class

		std::atomic<bool> pausable; // thread is pausable
 		std::atomic<bool> running; // thread is running (or paused)
		std::atomic<bool> paused; // thread is paused
		std::atomic<bool> interrupted; // thread has been interrupted by shutdown
		std::atomic<bool> resumed; // thread has been resumed after interruption by shutdown
		std::atomic<bool> terminated; // thread has terminated due to exception

		const std::string module; // the module of the thread (used for logging)
		std::atomic<unsigned long> id; //  of the thread in the database
		const crawlservpp::Struct::ThreadOptions options; // options for the thread
		unsigned long last; // last  for the thread
		std::string idString; //  of the thread as string (used for logging, ONLY for threads!)

		std::condition_variable pauseCondition; // condition variable to wait for unpause
		std::mutex pauseLock;

		std::string status; // status message of the thread (without pause state)
		std::mutex statusLock;

		std::unique_ptr<std::thread> threadPointer; // pointer to the thread

		// timing statistics (in seconds)
		std::chrono::steady_clock::time_point startTimePoint;
		std::chrono::steady_clock::time_point pauseTimePoint;
		std::chrono::duration<unsigned long> runTime;
		std::chrono::duration<unsigned long> pauseTime;
		void updateRunTime();
		void updatePauseTime();

		// pause checker
		bool isUnpaused() const;

		// main function
		void main();
	};
}

#endif /* MODULE_THREAD_H_ */
