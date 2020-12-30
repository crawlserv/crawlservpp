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
 * Thread.hpp
 *
 * Abstract implementation of the Thread interface
 *  for analyzer threads to be inherited by the algorithm class.
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

#include "../../Data/Corpus.hpp"
#include "../../Main/Exception.hpp"
#include "../../Query/Container.hpp"
#include "../../Struct/CorpusProperties.hpp"
#include "../../Struct/QueryProperties.hpp"
#include "../../Struct/QueryStruct.hpp"
#include "../../Struct/StatusSetter.hpp"
#include "../../Struct/ThreadOptions.hpp"
#include "../../Struct/ThreadStatus.hpp"
#include "../../Timer/Simple.hpp"

#include <cstddef>		// std::size_t
#include <functional>	// std::bind
#include <locale>		// std::locale
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <stdexcept>	// std::logic_error
#include <string>		// std::string
#include <string_view>	// std::string_view
#include <tuple>		// std::tuple
#include <vector>		// std::vector

namespace crawlservpp::Module::Analyzer {

	//! Abstract class providing thread functionality to algorithm (child) classes.
	class Thread : public Module::Thread, public Query::Container, protected Config {
		// for convenience
		using CorpusProperties = Struct::CorpusProperties;
		using QueryProperties = Struct::QueryProperties;
		using QueryStruct = Struct::QueryStruct;
		using StatusSetter = Struct::StatusSetter;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

	public:
		///@name Construction
		///@{

		Thread(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		Thread(Main::Database& dbBase, const ThreadOptions& threadOptions);

		///@}

	protected:
		///@name Database Connection
		///@{

		//! %Database connection for the analyzer thread.
		Database database;

		///@}
		///@name Corpora
		///@{

		//! Vector of corpora for the analyzer thread.
		std::vector<Data::Corpus> corpora;

		///@}
		///@name Getter
		///@{

		//! Returns the name of the algorithm.
		/*!
		 * Needs to be implemented by the (child)
		 *  class for the specific algorithm.
		 *
		 * \returns A string view containing the
		 *   name of the implemented algorithm.
		 */
		virtual std::string_view getName() const = 0;

		///@}
		///@name Implemented Thread Functions
		///@{

		void onInit() override;
		void onTick() override;
		void onPause() override;
		void onUnpause() override;
		void onClear() override;
		void onReset() override;

		///@}
		///@name Query Initialization
		///@{

		void initQueries() override;
		void addOptionalQuery(std::uint64_t queryId, QueryStruct& propertiesTo);
		void addQueries(
				const std::vector<std::uint64_t>& queryIds,
				std::vector<QueryStruct>& propertiesTo
		);

		///@}
		///@name Algorithm Events
		///@{

		//! Initializes the target table for the algorithm.
		/*!
		 * Needs to be implemented by the
		 *  (child) class for the specific
		 *  algorithm.
		 *
		 * \note The SQL statements for the
		 *   analyzer will not be yet prepared
		 *   when this function is called.
		 */
		virtual void onAlgoInitTarget() = 0;

		//! Initializes the algorithm.
		/*!
		 * Needs to be implemented by the
		 *  (child) class for the specific
		 *  algorithm.
		 */
		virtual void onAlgoInit() = 0;

		//! Performs an algorithm tick.
		/*!
		 * Needs to be implemented by the
		 *  (child) class for the specific
		 *  algorithm.
		 */
		virtual void onAlgoTick() = 0;

		//! Pauses the algorithm.
		/*!
		 * Needs to be implemented by the
		 *  (child) class for the specific
		 *  algorithm.
		 */
		virtual void onAlgoPause() = 0;

		//! Unpauses the algorithm.
		/*!
		 * Needs to be implemented by the
		 *  (child) class for the specific
		 *  algorithm.
		 */
		virtual void onAlgoUnpause() = 0;

		//! Clears the algorithm.
		/*!
		 * Needs to be implemented by the
		 *  (child) class for the specific
		 *  algorithm.
		 */
		virtual void onAlgoClear() = 0;

		///@}
		///@name Thread Control for Algorithms
		///@{

		void finished();
		void pause();

		///@}
		///@name Helper Functions for Algorithms
		///@{

		[[nodiscard]] std::string getTargetTableName() const;
		bool addCorpus(std::size_t index, StatusSetter& statusSetter);
		void checkCorpusSources(StatusSetter& statusSetter);

		///@}

		//! Class for analyzer exceptions to be used by algorithms.
		MAIN_EXCEPTION_CLASS();

	private:
		// queries for filtering a text corpus
		std::vector<QueryStruct> queryFilterQueries;

		// initialization functions
		void setUpConfig(std::queue<std::string>& warningsTo);
		void setUpLogging();
		void setUpDatabase();
		void setUpTarget();
		void setUpSqlStatements();
		void setUpQueries();
		void setUpAlgorithm();
		void logWarnings(std::queue<std::string>& warnings);

		// internal helper function
		std::size_t filterCorpusByQuery(StatusSetter& statusSetter);

		// hide other functions not to be used by the thread
		void start();
		void unpause();
		void stop();
		void interrupt();
	};

} /* namespace crawlservpp::Module::Analyzer */

#endif /* MODULE_ANALYZER_THREAD_HPP_ */
