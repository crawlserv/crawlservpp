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

#include <string>

namespace crawlservpp::Struct {
	struct AlgoThreadProperties {
		bool recreate;
		unsigned long algoId;
		unsigned long threadId;
		Main::Database& dbBase;
		const std::string& status;
		bool paused;
		const ThreadOptions& options;
		unsigned long last;

		// properties for newly created thread
		AlgoThreadProperties(
				unsigned long setAlgoId,
				Main::Database& setDatabase,
				const ThreadOptions& setOptions
		)
					: recreate(false),
					  algoId(setAlgoId),
					  threadId(0),
					  dbBase(setDatabase),
					  status(""),
					  paused(false),
					  options(setOptions),
					  last(0) {}

		// properties for previously interrupted thread
		AlgoThreadProperties(
				unsigned long setAlgoId,
				unsigned long setThreadId,
				Main::Database& setDatabase,
				const std::string& setStatus,
				bool setPaused,
				const ThreadOptions& setOptions,
				unsigned long setLast
		)
					: recreate(true),
					  algoId(setAlgoId),
					  threadId(setThreadId),
					  dbBase(setDatabase),
					  status(setStatus),
					  paused(setPaused),
					  options(setOptions),
					  last(setLast) {}
	};
}

#endif /* SRC_STRUCT_ALGOTHREADPROPERTIES_HPP_ */
