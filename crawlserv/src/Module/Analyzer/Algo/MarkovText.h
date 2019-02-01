/*
 * MarkovText.h
 *
 *   Markov Chain Text Generator algorithm implemented as child of the abstract Analyzer thread class.
 *
 *   This is a semi-serious proof-of-concept class for the crawlserv++ Analyzer module.
 *
 *   It uses the markov chain algorithm to generate random texts from a large text corpus previously parsed.
 *
 *   The implementation of the algorithm itself is taken from Rosetta Code at https://rosettacode.org/wiki/Markov_chain_text_generator
 *
 *
 *  Created on: Jan 13, 2019
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_MARKOVTEXT_H_
#define MODULE_ANALYZER_ALGO_MARKOVTEXT_H_

#include "../Thread.h"

#include "../../../Main/Data.h"
#include "../../../Timer/Simple.h"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace crawlservpp::Module::Analyzer::Algo {
	typedef crawlservpp::Main::Data::Type DataType;
	typedef crawlservpp::Main::Data::Value DataValue;

	class MarkovText: public crawlservpp::Module::Analyzer::Thread {
	public:
		MarkovText(crawlservpp::Main::Database& dbBase, unsigned long analyzerId, const std::string& analyzerStatus,
						bool analyzerPaused, const crawlservpp::Struct::ThreadOptions& threadOptions, unsigned long analyzerLast);
		MarkovText(crawlservpp::Main::Database& dbBase, const crawlservpp::Struct::ThreadOptions& threadOptions);
		virtual ~MarkovText();

		// implemented algorithm functions
		bool onAlgoInit(bool resumed);
		bool onAlgoTick();
		void onAlgoPause();
		void onAlgoUnpause();
		void onAlgoClear(bool interrupted);

	private:
		std::string source;
		std::map<std::string, std::vector<std::string>> dictionary;
		unsigned long sources;

		// algorithm functions
		std::string createText();
		void createDictionary();
	};
}

#endif /* MODULE_ANALYZER_ALGO_MARKOVTEXT_H_ */
