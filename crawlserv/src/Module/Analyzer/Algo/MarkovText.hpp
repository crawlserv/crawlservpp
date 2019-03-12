/*
 * MarkovText.hpp
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

#ifndef MODULE_ANALYZER_ALGO_MARKOVTEXT_HPP_
#define MODULE_ANALYZER_ALGO_MARKOVTEXT_HPP_

#include "../Thread.hpp"

#include "../../../Main/Data.hpp"
#include "../../../Struct/CorpusProperties.hpp"
#include "../../../Timer/Simple.hpp"

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
		void onAlgoInit(bool resumed) override;
		void onAlgoTick() override;
		void onAlgoPause() override;
		void onAlgoUnpause() override;
		void onAlgoClear(bool interrupted) override;

		// overwritten configuration functions
		void parseOption() override;
		void checkOptions() override;

	private:
		std::string source;
		std::map<std::string, std::vector<std::string>> dictionary;
		unsigned long sources;

		// algorithm options
		unsigned char markovTextDimension;
		unsigned long markovTextLength;
		unsigned long markovTextMax;
		std::string markovTextResultField;
		unsigned long markovTextSleep;
		std::string markovTextSourcesField;
		bool markovTextTiming;

		// algorithm functions
		std::string createText();
		void createDictionary();
	};

} /* crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_MARKOVTEXT_HPP_ */
