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
 * AssocOverTime.hpp
 *
 * Algorithm counting associations between the keyword and
 *  different categories over time.
 *
 * Created on: Oct 10, 2020
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_ASSOCOVERTIME_HPP_
#define MODULE_ANALYZER_ALGO_ASSOCOVERTIME_HPP_

#include "../Thread.hpp"

#include "../../../Data/Corpus.hpp"
#include "../../../Data/Data.hpp"
#include "../../../Helper/DateTime.hpp"
#include "../../../Main/Database.hpp"
#include "../../../Query/Container.hpp"
#include "../../../Struct/CorpusProperties.hpp"
#include "../../../Struct/QueryProperties.hpp"
#include "../../../Struct/QueryStruct.hpp"
#include "../../../Struct/StatusSetter.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"

#include <algorithm>	// std::find_if, std::min
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint16_t, std::uint64_t
#include <limits>		// std::numeric_limits
#include <locale>		// std::locale
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <string>		// std::string
#include <map>			// std::map
#include <vector>		// std::vector

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * DECLARATION
	 */

	//! %Empty algorithm template.
	/*!
	 * This is an empty template class for adding
	 *  new algorithms to the application.
	 */
	class AssocOverTime final : public Module::Analyzer::Thread, private Query::Container {
		// for convenience
		using QueryProperties = Struct::QueryProperties;
		using QueryStruct = Struct::QueryStruct;

		using Exception = Module::Analyzer::Thread::Exception;

		using CorpusProperties = Struct::CorpusProperties;
		using StatusSetter = Struct::StatusSetter;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

	public:
		///@name Construction
		///@{

		AssocOverTime(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		AssocOverTime(
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
		// custom structure
		struct Associations {
			std::vector<std::uint64_t> keywordPositions;
			std::vector<std::vector<std::uint64_t>> categoriesPositions;
			std::uint64_t offset{0};
		};

		// corpora and associations
		std::size_t currentCorpus{0};
		std::vector<Data::Corpus> corpora;
		std::map<std::string, std::map<std::string, Associations>> associations;

		// algorithm options
		std::uint64_t keyWordQuery{0};
		std::vector<std::string> categoryLabels;
		std::vector<std::uint64_t> categoryQueries;
		std::uint16_t windowSize{1};

		// algorithm queries
		QueryStruct queryKeyWord;
		std::vector<QueryStruct> queriesCategories;

		// algorithm functions
		void addCurrent();
		void saveAssociations();

		// query functions
		void initQueries();
		void addOptionalQuery(std::uint64_t queryId, QueryStruct& propertiesTo);
		void addQueries(
				const std::vector<std::uint64_t>& queryIds,
				std::vector<QueryStruct>& propertiesTo
		);
	};

} /* namespace crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_ASSOCOVERTIME_HPP_ */
