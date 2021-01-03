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
 * Empty.hpp
 *
 * Empty template class for adding new algorithms to the application.
 *  Duplicate this (and the source) file to implement a new algorithm,
 *  then add it to 'All.cpp' in the same directory.
 *
 * TODO: change name, date and description of the file
 *
 *  Created on: Jul 23, 2020
 *      Author: ans
 */

/*
 * TODO: change the name of the include guard
 */
#ifndef MODULE_ANALYZER_ALGO_EMPTY_HPP_
#define MODULE_ANALYZER_ALGO_EMPTY_HPP_

#include "../Thread.hpp"

/*
 * TODO: add additional crawlserv++ headers
 */
//#include "../../../Data/Data.hpp"
#include "../../../Main/Database.hpp"
//#include "../../../Struct/QueryStruct.hpp"
#include "../../../Struct/StatusSetter.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"

/*
 * TODO: add standard library includes here
 */
//#include <cstddef>	// std::size_t
//#include <string>		// std::string
#include <string_view>	// std::string_view
//#include <utility>	// std::pair
//#include <vector>		// std::vector

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	/*
	 * TODO: add constants for the algorithm here
	 *
	 * NOTE: choose a distinct prefix to avoid name conflicts with other algorithms
	 * 			(or use a namespace)
	 */
	//inline constexpr auto ...{...};

	//@}

	/*
	 * DECLARATION
	 */

	/*
	 * TODO: change name and description of the class
	 */

	//! %Empty algorithm template.
	/*!
	 * This is an empty template class for adding
	 *  new algorithms to the application.
	 */
	class Empty final : public Module::Analyzer::Thread {
		// for convenience

		/*
		 * TODO: add aliases of used data structures
		 */
		using Exception = Module::Analyzer::Thread::Exception;

		//using QueryStruct = Struct::QueryStruct;
		using StatusSetter = Struct::StatusSetter;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		//using StringString = std::pair<std::string, std::string>;

	public:
		///@name Construction
		///@{

		/*
		 * TODO: change the names of the constructors
		 */

		Empty(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		Empty(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions
		);

		///@}
		///@name Implemented Getter
		///@{

		std::string_view getName() const override;

		///@}
		///@name Implemented Algorithm Functions
		///@{

		void onAlgoInitTarget() override;
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
		void resetAlgo() override;

		///@}

	private:
		// algorithm options
		struct Entries {
		/*
		 * TODO: add members for algorithm options here
		 */
		} algoConfig;

		// algorithm queries
		/*
		 * TODO: decide whether queries are needed
		 *
		 * use QueryStruct (or vectors of it) to store queries
		 */

		// algorithm state
		/*
		 * TODO: add additional private member variables here
		 */

		// algorithm functions
		/*
		 * TODO: add internal algorithm functions here
		 */

		// query functions
		/*
		 * TODO: decide whether queries are needed
		void initQueries() override;
		void deleteQueries() override;
		*/

		// internal helper functions
		/*
		 * TODO: add additional helper functions here
		 */
	};

} /* namespace crawlservpp::Module::Analyzer::Algo */

/*
 * TODO: change the name of the include guard
 */
#endif /* MODULE_ANALYZER_ALGO_EMPTY_HPP_ */
