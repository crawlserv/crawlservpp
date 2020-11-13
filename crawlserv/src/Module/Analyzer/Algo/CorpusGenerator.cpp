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
 * CorpusGenerator.cpp
 *
 * This algorithm uses the built-in functionality for building text corpora from its input data.
 *
 * Additionally, it writes some basic statistics (number and length of words and sentences)
 *  to the target table.
 *
 *
 *  Created on: Mar 5, 2020
 *      Author: ans
 */

#include "CorpusGenerator.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	CorpusGenerator::CorpusGenerator(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions,
			const ThreadStatus& threadStatus
	) : Module::Analyzer::Thread(
				dbBase,
				threadOptions,
				threadStatus
			) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	//! Starts a new algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&)
	 */
	CorpusGenerator::CorpusGenerator(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions
	) : Module::Analyzer::Thread(dbBase, threadOptions) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	/*
	 * IMPLEMENTED ALGORITHM FUNCTIONS
	 */

	//! Generates the corpus.
	void CorpusGenerator::onAlgoInit() {
		// reset progress
		this->setProgress(0.F);

		// check your sources
		this->setStatusMessage("Checking sources...");

		this->log(generalLoggingVerbose, "checks sources...");

		this->database.checkSources(
				this->config.generalInputSources,
				this->config.generalInputTables,
				this->config.generalInputFields
		);

		// set target fields
		std::vector<std::string> fields;
		std::vector<std::string> types;

		fields.reserve(numFields);
		types.reserve(numFields);

		fields.emplace_back("source");
		fields.emplace_back("wordcount");
		fields.emplace_back("avgwordlen");
		fields.emplace_back("medwordlen");
		fields.emplace_back("sentencecount");
		fields.emplace_back("avgsentencelen");
		fields.emplace_back("medsentencelen");

		types.emplace_back("TEXT");
		types.emplace_back("BIGINT UNSIGNED");
		types.emplace_back("FLOAT");
		types.emplace_back("FLOAT");
		types.emplace_back("BIGINT UNSIGNED");
		types.emplace_back("FLOAT");
		types.emplace_back("FLOAT");

		this->database.setTargetFields(fields, types);

		// initialize target table
		this->setStatusMessage("Creating target table...");

		this->log(generalLoggingVerbose, "creates target table...");

		this->database.initTargetTable(true, true);

		// request text corpus
		this->log(generalLoggingVerbose, "gets text corpus...");

		std::size_t corpora{0};
		std::size_t bytes{0};
		std::size_t tokens{0};
		std::size_t sources{0};

		const auto resultTable{
			this->getTargetTableName()
		};

		for(std::size_t n{0}; n < this->config.generalInputSources.size(); ++n) {
			std::string dateFrom;
			std::string dateTo;
			std::size_t corpusSources{0};

			if(this->config.filterDateEnable) {
				dateFrom = this->config.filterDateFrom;
				dateTo = this->config.filterDateTo;
			}

			Data::Corpus corpus(this->config.generalCorpusChecks);

			std::string statusStr;

			if(this->config.generalInputSources.size() > 1) {
				std::ostringstream corpusStatusStrStr;

				corpusStatusStrStr.imbue(std::locale(""));

				corpusStatusStrStr << "Getting text corpus #";
				corpusStatusStrStr << n + 1;
				corpusStatusStrStr << "/";
				corpusStatusStrStr << this->config.generalInputSources.size();
				corpusStatusStrStr << "...";

				statusStr = corpusStatusStrStr.str();
			}
			else {
				statusStr = "Getting text corpus...";
			}

			StatusSetter statusSetter(
					statusStr,
					1.F,
					[this](const auto& status) {
						this->setStatusMessage(status);
					},
					[this](const auto progress) {
						this->setProgress(progress);
					},
					[this]() {
						return this->isRunning();
					}
			);

			if(!(this->database.getCorpus(
					CorpusProperties(
							this->config.generalInputSources.at(n),
							this->config.generalInputTables.at(n),
							this->config.generalInputFields.at(n),
							this->config.tokenizerSentenceManipulators,
							this->config.tokenizerSentenceModels,
							this->config.tokenizerWordManipulators,
							this->config.tokenizerWordModels,
							this->config.tokenizerSavePoints,
							this->config.tokenizerFreeMemoryEvery
					),
					dateFrom,
					dateTo,
					corpus,
					corpusSources,
					statusSetter
			))) {
				return;
			}

			if(!corpus.empty()) {
				++corpora;

				bytes += corpus.size();

				if(corpus.isTokenized()) {
					tokens += corpus.getNumTokens();
				}

				sources += corpusSources;
			}

			if(statusSetter.change("Creating corpus statistics...")) {
				// calculate token (word) lengths
				std::vector<std::size_t> tokenLengths;

				tokenLengths.reserve(corpus.getNumTokens());

				for(const auto& token : corpus.getcTokens()) {
					tokenLengths.push_back(Helper::Utf8::length(token));
				}

				const auto avgTokenLength{
					Helper::Math::avg<float>(tokenLengths)
				};
				const auto medTokenLength{
					Helper::Math::median<float>(tokenLengths)
				};

				std::vector<std::size_t>{}.swap(tokenLengths);

				// calculate sentence lengths
				std::vector<std::size_t> sentenceLengths;

				sentenceLengths.reserve(corpus.getcSentenceMap().size());

				for(const auto& sentence : corpus.getcSentenceMap()) {
					sentenceLengths.push_back(sentence.second);
				}

				const auto avgSentenceLength{
					Helper::Math::avg<float>(sentenceLengths)
				};
				const auto medSentenceLength{
					Helper::Math::median<float>(sentenceLengths)
				};

				std::vector<std::size_t>{}.swap(sentenceLengths);

				// write data to target table
				Data::InsertFieldsMixed data;

				data.columns_types_values.reserve(numFields);

				data.table = resultTable;

				data.columns_types_values.emplace_back(
						"analyzed__source",
						Data::Type::_string,
						Data::Value(
								this->config.generalInputSources.at(n)
								+ "."
								+ this->config.generalInputTables.at(n)
								+ "."
								+ this->config.generalInputFields.at(n)
						)
				);

				data.columns_types_values.emplace_back(
						"analyzed__wordcount",
						Data::Type::_uint64,
						Data::Value(corpus.getNumTokens())
				);

				data.columns_types_values.emplace_back(
						"analyzed__avgwordlen",
						Data::Type::_double,
						Data::Value(avgTokenLength)
				);

				data.columns_types_values.emplace_back(
						"analyzed__medwordlen",
						Data::Type::_double,
						Data::Value(medTokenLength)
				);

				data.columns_types_values.emplace_back(
						"analyzed__sentencecount",
						Data::Type::_uint64,
						Data::Value(corpus.getcSentenceMap().size())
				);

				data.columns_types_values.emplace_back(
						"analyzed__avgsentencelen",
						Data::Type::_double,
						Data::Value(avgSentenceLength)
				);

				data.columns_types_values.emplace_back(
						"analyzed__medsentencelen",
						Data::Type::_double,
						Data::Value(medSentenceLength)
				);

				this->database.insertCustomData(data);

				this->database.updateTargetTable();
			}
		}

		// algorithm has finished
		this->log(generalLoggingExtended, "has finished.");

		/*
		 * NOTE: The status will be saved in-class and not set here, because
		 * 	the parent class will revert to the original status after initialization
		 */

		std::ostringstream statusStrStr;

		statusStrStr.imbue(std::locale(""));

		if(corpora > 0) {
			statusStrStr << "IDLE ";

			if(corpora == 1) {
				statusStrStr << "Corpus of ";
			}
			else {
				statusStrStr << corpora << " corpora of ";
			}

			if(bytes == 1) {
				statusStrStr << "one byte";
			}
			else {
				statusStrStr << bytes << " bytes";
			}

			if(tokens > 0) {
				statusStrStr << ", ";

				if(tokens == 1) {
					statusStrStr << "one word";
				}
				else {
					statusStrStr << tokens << " words";
				}
			}

			if(sources == 1) {
				statusStrStr << " from one source";
			}
			else if(sources > 1) {
				statusStrStr << " from " << sources << " sources";
			}

			this->status = statusStrStr.str();
		}
		else {
			this->status = "IDLE No corpus created.";
		}

	}

	//! Sleeps until the thread is terminated.
	/*!
	 * The corpus has already been genererated on
	 *  initialization.
	 *
	 * \sa onAlgoInit
	 */
	void CorpusGenerator::onAlgoTick() {
		this->setStatusMessage(this->status);

		// sleep forever (i.e. until the thread is terminated)
		this->sleep(std::numeric_limits<std::uint64_t>::max());
	}

	//! Does nothing.
	void CorpusGenerator::onAlgoPause() {}

	//! Does nothing.
	void CorpusGenerator::onAlgoUnpause() {}

	//! Does nothing.
	void CorpusGenerator::onAlgoClear() {}

	/*
	 * IMPLEMENTED CONFIGURATION FUNCTIONS
	 */

	//! Does nothing.
	void CorpusGenerator::parseAlgoOption() {}

	//! Does nothing.
	void CorpusGenerator::checkAlgoOptions() {
		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

} /* namespace crawlservpp::Module::Analyzer::Algo */
