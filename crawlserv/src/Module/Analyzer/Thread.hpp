/*
 *
 * ---
 *
 *  Copyright (C) 2022 Anselm Schmidt (ans[ät]ohai.su)
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
#include "../../Data/Data.hpp"
#include "../../Helper/DateTime.hpp"
#include "../../Helper/Json.hpp"
#include "../../Helper/Memory.hpp"
#include "../../Main/Exception.hpp"
#include "../../Network/FTPUpload.hpp"
#include "../../Query/Container.hpp"
#include "../../Struct/CorpusProperties.hpp"
#include "../../Struct/QueryProperties.hpp"
#include "../../Struct/QueryStruct.hpp"
#include "../../Struct/StatusSetter.hpp"
#include "../../Struct/ThreadOptions.hpp"
#include "../../Struct/ThreadStatus.hpp"
#include "../../Timer/Simple.hpp"

#include "../../_extern/rapidjson/include/rapidjson/document.h"

#include <algorithm>	// std::all_of, std::any_of, std::remove_if
#include <chrono>		// std::chrono
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint64_t
#include <locale>		// std::locale
#include <map>			// std::map
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <stdexcept>	// std::logic_error
#include <string>		// std::string
#include <string_view>	// std::string_view
#include <utility>		// std::swap
#include <vector>		// std::vector

namespace crawlservpp::Module::Analyzer {

	///@name Constants
	///@{

	//! The number of tokens after which the status will be updated when combining corpora.
	constexpr auto combineUpdateStatusEvery{100000};

	///@}

	//! Abstract class providing thread functionality to algorithm (child) classes.
	class Thread : public Module::Thread, public Query::Container, protected Config {
		// for convenience
		using Corpus = Data::Corpus;

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
		std::vector<Corpus> corpora;

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
		///@name Query Functions
		///@{

		void initQueries() override;
		void deleteQueries() override;
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
		bool addCorpora(bool isCombine, StatusSetter& statusSetter);
		void checkCorpusSources(StatusSetter& statusSetter);

		///@}
		///@name Helper Functions for Clean-up
		///@{

		void uploadResult();
		void cleanUpCorpora();
		void cleanUpQueries();

		///@}

		//! Class for analyzer exceptions to be used by algorithms.
		MAIN_EXCEPTION_CLASS();

	private:
		// queries
		/*
		 * make sure to initialize AND delete them!
		 *  -> setUpQueries(), cleanUpQueries()
		 */
		std::vector<QueryStruct> queryFilterQueries;

		// restart timer
		std::chrono::time_point<std::chrono::steady_clock> idleStart{};

		// initialization functions
		void setUpConfig(std::queue<std::string>& warningsTo);
		void setUpLogging();
		void setUpDatabase();
		void setUpTarget();
		void setUpSqlStatements();
		void setUpQueries();
		void setUpAlgorithm();
		void logWarnings(std::queue<std::string>& warnings);

		// internal helper functions
		void addCorpus(std::size_t index, StatusSetter& statusSetter);
		void combineCorpora(StatusSetter& statusSetter);
		void filterCorpusByQuery(std::size_t index, StatusSetter& statusSetter);

		// internal static helper template
		template<typename Allocator>
		[[nodiscard]] static rapidjson::Value createJSONValue(
				Data::Type type,
				Data::Value value,
				const std::string& originalType,
				Allocator& allocator
		) {
			rapidjson::Value result;

			switch(type) {
			case Data::Type::_bool:
				result.SetBool(value._b);

				break;

			case Data::Type::_int32:
				result.SetInt(value._i32);

				break;

			case Data::Type::_uint32:
				result.SetUint(value._ui32);

				break;

			case Data::Type::_int64:
				result.SetUint(value._i64);

				break;

			case Data::Type::_uint64:
				result.SetUint(value._ui64);

				break;

			case Data::Type::_double:
				result.SetDouble(value._d);

				break;

			case Data::Type::_string:
				result.SetString(value._s, allocator);

				break;

			default:
				throw Thread::Exception("Cannot write unknown data type '" + originalType + "' to JSON");
			}

			return {};
		}

		// hide other functions not to be used by the thread
		void start();
		void unpause();
		void stop();
		void interrupt();
	};

} /* namespace crawlservpp::Module::Analyzer */

#endif /* MODULE_ANALYZER_THREAD_HPP_ */
