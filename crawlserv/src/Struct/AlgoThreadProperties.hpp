/*
 *
 * ---
 *
 *  Copyright (C) 2023 Anselm Schmidt (ans[ät]ohai.su)
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
 * AlgoThreadProperties.hpp
 *
 * Properties of an algorithm thread.
 *
 *  Created on: Mar 12, 2019
 *      Author: ans
 */

#ifndef STRUCT_ALGOTHREADPROPERTIES_HPP_
#define STRUCT_ALGOTHREADPROPERTIES_HPP_

#include "../Main/Database.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/ThreadOptions.hpp"
#include "../Struct/ThreadStatus.hpp"

#include <cstdint>		// std::uint32_t
#include <optional>		// std::optional

//! Namespace for data structures.
namespace crawlservpp::Struct {

	//! Properties of an algorithm thread.
	struct AlgoThreadProperties {
		///@name Properties
		///@{

		//! The ID of the algorithm run by the thread.
		std::uint32_t algoId;

		//! Options used for the algorithm thread.
		ThreadOptions options;

		//! Status of the algorithm thread.
		/*!
		 * Will be empty, if the thread has not been previously interrupted.
		 */
		ThreadStatus status;

		///@}
		///@name Database Reference
		///@{

		//! Reference to the database instance used by the thread.
		Main::Database& dbBase;

		///@}
		///@name Construction and Destruction
		///@{

		//! Constructor setting properties for a newly created thread.
		/*!
		 * \param setAlgoId The ID of the algorithm used
		 *   by the newly created thread.
		 * \param setOptions Constant reference to the options
		 *   used by the newly created thread.
		 * \param setDatabase Reference to the database instance
		 *   used by the newly created thread.
		 */
		AlgoThreadProperties(
				std::uint32_t setAlgoId,
				const ThreadOptions& setOptions,
				Main::Database& setDatabase
		)
					: algoId(setAlgoId),
					  options(setOptions),
					  dbBase(setDatabase)
					  {}

		//! Constructor setting properties for a previously interrupted thread.
		/*!
		 * \param setAlgoId The ID of the algorithm used
		 *   by the previously interrupted thread.
		 * \param setOptions Constant reference to the
		 *   options used by the previously interrupted
		 *   thread.
		 * \param setStatus Constant reference to the
		 *   status of the algorithm thread.
		 * \param setDatabase Reference to the database instance
		 *   used by the previously interrupted thread.
		 *
		 * \throws Main::Exception if the specified status
		 *   contains an invalid thread ID, i.e. zero.
		 */
		AlgoThreadProperties(
				std::uint32_t setAlgoId,
				const ThreadOptions& setOptions,
				const ThreadStatus& setStatus,
				Main::Database& setDatabase
		)
					: algoId(setAlgoId),
					  options(setOptions),
					  status(setStatus),
					  dbBase(setDatabase) {
			if(status.id == 0) {
				throw Main::Exception(
						"AlgoThreadProperties::AlgoThreadProperties():"
						" Invalid thread ID for previously interrupted algorithm (is zero)"
				);
			}
		}

		//! Default destructor.
		virtual ~AlgoThreadProperties() = default;

		///@}
		/**@name Copy and Move
		 * The class is neither copyable, nor moveable.
		 */
		///@{

		//! Deleted copy constructor.
		AlgoThreadProperties(AlgoThreadProperties&) = delete;

		//! Deleted copy assignment operator.
		AlgoThreadProperties& operator=(AlgoThreadProperties&) = delete;

		//! Deleted move constructor.
		AlgoThreadProperties(AlgoThreadProperties&&) = delete;

		//! Deleted move assignment operator.
		AlgoThreadProperties& operator=(AlgoThreadProperties&&) = delete;

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_ALGOTHREADPROPERTIES_HPP_ */
