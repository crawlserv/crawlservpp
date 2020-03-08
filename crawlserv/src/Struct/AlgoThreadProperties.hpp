/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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

#ifndef SRC_STRUCT_ALGOTHREADPROPERTIES_HPP_
#define SRC_STRUCT_ALGOTHREADPROPERTIES_HPP_

#include "../Main/Database.hpp"
#include "../Struct/ThreadOptions.hpp"
#include "../Struct/ThreadStatus.hpp"

namespace crawlservpp::Struct {

	struct AlgoThreadProperties {
	private:
		const ThreadStatus _emptyStatus;

	public:
		bool recreate;
		size_t algoId;
		Main::Database& dbBase;
		const ThreadOptions& options;
		const ThreadStatus& status;

		// properties for newly created thread
		AlgoThreadProperties(
				size_t setAlgoId,
				Main::Database& setDatabase,
				const ThreadOptions& setOptions
		)
					: recreate(false),
					  algoId(setAlgoId),
					  dbBase(setDatabase),
					  options(setOptions),
					  status(_emptyStatus)
					  {}

		// properties for previously interrupted thread
		AlgoThreadProperties(
				size_t setAlgoId,
				Main::Database& setDatabase,
				const ThreadOptions& setOptions,
				const ThreadStatus& setStatus
		)
					: recreate(true),
					  algoId(setAlgoId),
					  dbBase(setDatabase),
					  options(setOptions),
					  status(setStatus) {}
	};

} /* crawlservpp::Struct */

#endif /* SRC_STRUCT_ALGOTHREADPROPERTIES_HPP_ */
