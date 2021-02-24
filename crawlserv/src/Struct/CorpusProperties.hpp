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
 * CorpusProperties.hpp
 *
 * Corpus properties (source type, table and field).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_CORPUSPROPERTIES_HPP_
#define STRUCT_CORPUSPROPERTIES_HPP_

#include <cstdint>	// std::uint16_t
#include <string>	// std::string
#include <vector>	// std::vector

namespace crawlservpp::Struct {

	//! Corpus properties containing the type, table, and column name of its source.
	struct CorpusProperties {
		///@name Properties
		///@{

		//! The type of the source from which the corpus is created (see below).
		/*!
		 * \sa Module::Analyzer::generalInputSourcesParsing,
		 *   Module::Analyzer::generalInputSourcesExtracting,
		 *   Module::Analyzer::generalInputSourcesAnalyzing,
		 *   Module::Analyzer::generalInputSourcesCrawling
		 */
		std::uint16_t sourceType{};

		//! The name of the table from which the corpus is created.
		std::string sourceTable;

		//! The name of the table column from which the corpus is created.
		std::string sourceColumn;

		//! The IDs of manipulators for preprocessing the corpus.
		std::vector<std::uint16_t> manipulators;

		//! The models used by the manipulators with the same array index.
		std::vector<std::string> models;

		//! The dictionaries used by the manipulators with the same array index.
		std::vector<std::string> dictionaries;

		//! The languages used by the manipulators with the same array index.
		std::vector<std::string> languages;

		//! List of savepoints.
		/*!
		 * Manipulation steps after which the result will
		 *  be stored in the database. If zero, the
		 *  unmanipulated corpus will be stored. Starting
		 *  with one, the save points correspond to the
		 *  manipulators used on the corpus.
		 *
		 * Only the unmanipulated corpus will be stored
		 *  by default.
		 */
		std::vector<std::uint16_t> savePoints{{}};

		//! Number of processed bytes in a continuous corpus after which memory will be freed.
		/*!
		 * If zero, memory will only be freed after
		 *  processing is complete.
		 */
		std::uint64_t freeMemoryEvery{};

		//! Tokenization.
		/*!
		 * True, of the corpus will be tokenized.
		 *  False otherwise.
		 */
		bool tokenize{false};

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		CorpusProperties() = default;

		//! Constructor setting properties for a tokenized corpus.
		/*!
		 * \param setSourceType The type of the source
		 *   from which the corpus is created (see below).e
		 * \param setSourceTable Constant reference to a
		 *   string containing the name of the table from
		 *   which the corpus is created.
		 * \param setSourceColumn Constant reference to a
		 *   string containing the name of the table column
		 *   from which the corpus is created.
		 * \param setManipulators Constant reference to a
		 *   vector containing the manipulators to be
		 *   applied when preprocessing the corpus.
		 * \param setModels Constant reference to a vector
		 *   of strings, containing a model for each
		 *   manipulator, or an empty string if no
		 *   model is required by the manipulator.
		 * \param setDictionaries Constant reference to a
		 *   vector of strings, containing a dictionary for
		 *   each manipulator, or an empty string if no
		 *   dictionary is required by the manipulator.
		 * \param setLanguages Constant reference to a
		 *   vector of strings, containing a language for
		 *   each manipulator, or an empty string if no
		 *   language is required by the manipulator, or
		 *   its default language should be used.
		 * \param setSavePoints Constant reference to a
		 *   vector containing the save points to be
		 *   generated. A value of zero indicates that the
		 *   unmanipulated corpus will be saved. Starting
		 *   from one, the number corresponds to the
		 *   manipulator used on the corpus.
		 * \param setFreeMemoryEvery Number of processed bytes
		 *   in a continuous corpus after which memory will
		 *   be freed. If zero, memory will only be freed
		 *   after processing is complete.
		 *
		 * \sa Module::Analyzer::generalInputSourcesParsing,
		 *   Module::Analyzer::generalInputSourcesExtracting,
		 *   Module::Analyzer::generalInputSourcesAnalyzing,
		 *   Module::Analyzer::generalInputSourcesCrawling,
		 *	 Data::Corpus::corpusManipNone,
		 *	 Data::Corpus::corpusManipTagger,
		 *	 Data::Corpus::corpusManipTaggerPosterior,
		 *	 Data::Corpus::corpusManipEnglishStemmer,
		 *	 Data::Corpus::corpusManipGermanStemmer,
		 *	 Data::Corpus::corpusManipLemmatizer,
		 *	 Data::Corpus::corpusManipRemove,
		 *	 Data::Corpus::corpusManipCorrect
		 */
		CorpusProperties(
				std::uint16_t setSourceType,
				const std::string& setSourceTable,
				const std::string& setSourceColumn,
				const std::vector<std::uint16_t>& setManipulators,
				const std::vector<std::string>& setModels,
				const std::vector<std::string>& setDictionaries,
				const std::vector<std::string>& setLanguages,
				const std::vector<std::uint16_t>& setSavePoints,
				std::uint64_t setFreeMemoryEvery
		) : sourceType{setSourceType},
			sourceTable{setSourceTable},
			sourceColumn{setSourceColumn},
			manipulators{setManipulators},
			models{setModels},
			dictionaries{setDictionaries},
			languages{setLanguages},
			savePoints{setSavePoints},
			freeMemoryEvery{setFreeMemoryEvery},
			tokenize{true} {}

		//! Constructor setting properties for a continuous corpus.
		/*!
		 * \param setSourceType The type of the source
		 *   from which the corpus is created (see below).e
		 * \param setSourceTable Constant reference to a
		 *   string containing the name of the table from
		 *   which the corpus is created.
		 * \param setSourceColumn Constant reference to a
		 *   string containing the name of the table column
		 *   from which the corpus is created.
		 * \param setFreeMemoryEvery Number of processed bytes
		 *   in a continuous corpus after which memory will
		 *   be freed. If zero, memory will only be freed
		 *   after processing is complete.
		 *
		 * \sa Module::Analyzer::generalInputSourcesParsing,
		 *   Module::Analyzer::generalInputSourcesExtracting,
		 *   Module::Analyzer::generalInputSourcesAnalyzing,
		 *   Module::Analyzer::generalInputSourcesCrawling
		 */
		CorpusProperties(
				std::uint16_t setSourceType,
				const std::string& setSourceTable,
				const std::string& setSourceColumn,
				std::uint64_t setFreeMemoryEvery
		) : sourceType{setSourceType},
			sourceTable{setSourceTable},
			sourceColumn{setSourceColumn},
			freeMemoryEvery{setFreeMemoryEvery} {}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_CORPUSPROPERTIES_HPP_ */
