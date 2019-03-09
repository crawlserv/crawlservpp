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
#include "../../../Struct/CorpusProperties.h"
#include "../../../Timer/Simple.h"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace crawlservpp::Module::Analyzer::Algo {
	class MarkovText: public Module::Analyzer::Thread {
		// for convenience
		typedef Main::Data::Type DataType;
		typedef Main::Data::Value DataValue;
		typedef Struct::CorpusProperties CorpusProperties;
		typedef Struct::ThreadOptions ThreadOptions;

	public:
		MarkovText(Main::Database& dbBase, unsigned long analyzerId, const std::string& analyzerStatus,
						bool analyzerPaused, const ThreadOptions& threadOptions, unsigned long analyzerLast);
		MarkovText(Main::Database& dbBase, const ThreadOptions& threadOptions);
		virtual ~MarkovText();

		// implemented algorithm functions
		void onAlgoInit(bool resumed);
		void onAlgoTick();
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
