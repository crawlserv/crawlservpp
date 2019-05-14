/*
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
		unsigned long algoId;
		Main::Database& dbBase;
		const ThreadOptions& options;
		const ThreadStatus& status;

		// properties for newly created thread
		AlgoThreadProperties(
				unsigned long setAlgoId,
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
				unsigned long setAlgoId,
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
}

#endif /* SRC_STRUCT_ALGOTHREADPROPERTIES_HPP_ */
