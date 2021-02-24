/*
 * Tagger.hpp
 *
 * Multilingual POS tagger using Wapiti by Thomas Lavergne.
 *
 *  Use the original wapiti program to train models for the tagger.
 *
 *  Source: https://github.com/Jekub/Wapiti
 *
 *  Paper: Lavergne, Thomas / Cappe, Olivier / Yvon, François:
 *   Practical Very Large Scale CRFs, in: Proceedings of the
 *   48th Annual Meeting of the Association for Computational
 *   Linguistics, Uppsala, 11–16 July 2010, pp. 504–513.
 *
 *  Created on: Aug 4, 2020
 *      Author: ans
 */

#ifndef DATA_TAGGER_H_
#define DATA_TAGGER_H_

#include "../Main/Exception.hpp"

#include "../_extern/wapiti/wapiti.h"

#include <cstdio>		// std::fclose, std::FILE, std::fopen
#include <cstdlib>		// std::free
#include <iterator>		// std::distance
#include <limits>		// std::numeric_limits
#include <memory>		// std::unique_ptr
#include <stdexcept>	// std::runtime_error
#include <string>		// std::string
#include <string_view>	// std::string_view
#include <vector>		// std::vector

namespace crawlservpp::Data {
	/*
	 * DECLARATION
	 */

	//! Multilingual POS (part of speech) tagger using @c Wapiti by Thomas Lavergne.
	/*!
	 * Based on a minimized version of @c Wapiti.
	 *
	 *  Source: https://github.com/Jekub/Wapiti
	 *
	 *  Paper:
	 *   Lavergne, Thomas / Cappe, Olivier / Yvon, François:
	 *   Practical Very Large Scale CRFs, in: Proceedings of the
	 *   48th Annual Meeting of the Association for Computational
	 *   Linguistics, Uppsala, 11–16 July 2010, pp. 504–513.
	 *
	 *  Use the original wapiti program to train
	 *  models for the tagger.
	 *
	 *  See <a href="https://wapiti.limsi.fr/">
	 *  its homepage</a> for more information.
	 */
	class Tagger {
	public:
		///@name Construction and Destruction
		///@{

		//! Default constructor.
		Tagger() = default;

		virtual ~Tagger();

		///@}
		///@name Getter
		///@{

		[[nodiscard]] static constexpr std::string_view getVersion();

		///@}
		///@name Setters
		///@{

		void setPureMaxEntMode(bool isPureMaxEntMode);
		void setPosteriorDecoding(bool isPosteriorDecoding);
		void setPartlyLabeledInput(bool isPartlyLabeledInput);

		///@}
		///@name Model and Tagging
		///@{

		void loadModel(const std::string& modelFile);
		void label(
				std::vector<std::string>::iterator sentenceBegin,
				std::vector<std::string>::iterator sentenceEnd
		);

		///@}

		//! POS (part of speech)-tagging exception.
		/*!
		 * This exception is thrown when:
		 * - a @c Wapiti model file cannot be opened
		 * - a @c Wapiti model cannot be loaded
		 * - an error occured while POS (part of
		 *    speech)-tagging a sentence.
		 */
		MAIN_EXCEPTION_CLASS();

		/**@name Copy and Move
		 * The class is not copyable, only (default) moveable.
		 */
		///@{

		//! Deleted copy constructor.
		Tagger(Tagger&) = delete;

		//! Deleted copy assignment operator.
		Tagger& operator=(Tagger&) = delete;

		//! Default move constructor.
		Tagger(Tagger&&) = default;

		//! Default move assignment operator.
		Tagger& operator=(Tagger&&) = default;

		///@}

	private:
		wapiti::opt_t options{};
		wapiti::mdl_t * model{nullptr};

		bool maxEnt{false};
		bool posterior{false};
		bool partlyLabeled{false};

		// general deleter for wapiti C types
		struct wapitiDelete {
			void operator()(void * ptr) {
				//NOLINTNEXTLINE(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory, hicpp-no-malloc)
				std::free(ptr);
			}
		};

		// deleter for wapiti sequences
		struct wapitiDeleteSeq {
			void operator()(wapiti::seq_t * seq) {
				//NOLINTNEXTLINE(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory, hicpp-no-malloc)
				std::free(seq->raw);

				//NOLINTNEXTLINE(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory, hicpp-no-malloc)
				std::free(seq);
			}
		};
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Destructor freeing the POS-tagging model, if one has been loaded.
	inline Tagger::~Tagger() {
		if(this->model != nullptr) {
			wapiti::mdl_free(this->model);

			this->model = nullptr;
		}
	}

	//! Gets the underlying version of wapiti.
	/*!
	 * \returns The version of wapiti, on
	 *   which the POS (part of speech) tagger
	 *   is built.
	 */
	inline constexpr std::string_view Tagger::getVersion() {
		return wapiti::version;
	}

	//! Sets whether the pure maxent mode of @c Wapiti is enabled.
	/*!
	 * See
	 *  <a href="https://wapiti.limsi.fr/manual.html#puremaxent">
	 *  the manual</a> of @c Wapiti for more information.
	 *
	 *  \param isPureMaxEntMode Set to true to enable
	 *    the pure maxent mode of @c Wapiti.
	 */
	inline void Tagger::setPureMaxEntMode(bool isPureMaxEntMode) {
		this->maxEnt = isPureMaxEntMode;
	}

	//! Sets whether posterior decoding is used instead of the classical Viterbi encoding .
	/*!
	 * See <a href="https://wapiti.limsi.fr/manual.html">
	 *  the manual</a> of @c Wapiti for more information.
	 *
	 * \note Posterior decoding is slower, but more accurate.
	 */
	inline void Tagger::setPosteriorDecoding(bool isPosteriorDecoding) {
		this->posterior = isPosteriorDecoding;
	}

	//! Sets whether the input is already partly labelled.
	/*!
	 * Already existing labels will be kept used to
	 *  improve the POS tagging of the remaining
	 *  tokens.
	 *
	 * The labels need to be separated from the
	 *  tokens by either a space or a tabulator.
	 *
	 * See
	 *  <a href="https://wapiti.limsi.fr/manual.html#forced">
	 *  the manual</a> of @c Wapiti for more information.
	 */
	inline void Tagger::setPartlyLabeledInput(bool isPartlyLabeledInput) {
		this->partlyLabeled = isPartlyLabeledInput;
	}

	//! Loads a POS-tagging model trained by using @c Wapiti.
	/*!
	 * See <a href="https://wapiti.limsi.fr/manual.html">
	 *  the manual</a> of @c Wapiti for more information.
	 *
	 * \param modelFile Name (including path) of the
	 *   model file to be used.
	 *
	 * \throws Tagger::Exception if the model file
	 *   cannot be opened, or if the model cannot be
	 *   loaded.
	 */
	inline void Tagger::loadModel(const std::string& modelFile) {
		// check argument
		if(modelFile.empty()) {
			throw Exception(
					"Tagger::loadModel():"
					" No POS-tagging model has been specified"
			);
		}

		// create model
		this->model = wapiti::mdl_new(wapiti::rdr_new());

		// set wapiti options
		//NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
		std::vector<char> modelFileCopy(modelFile.c_str(), modelFile.c_str() + modelFile.size() + 1);

		this->options.maxent = this->maxEnt;		// maxent mode
		this->options.model = modelFileCopy.data();	// model file
		this->options.lblpost = this->posterior;	// posterior (forward-backward) or Viterbi decoding
		this->options.force = this->partlyLabeled;	// set whether input is partly (and correctly) labeled

		this->model->opt = &(this->options);

		//NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
		auto * file{std::fopen(this->model->opt->model, "re")};

		if(file == nullptr) {
			throw Exception(
					"Tagger::loadModel():"
					" Cannot open POS-tagging model: "
					+ modelFile
			);
		}

		try {
			wapiti::mdl_load(this->model, file);
		}
		catch(const std::runtime_error& e) {
			throw Exception(
					"Tagger::loadModel():"
					" Error while loading the POS-tagging model – "
					+ std::string(e.what())
			);
		}

		//NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
		std::fclose(file);
	}

	//! POS (part of speech)-tags a sentence.
	/*!
	 * The tags will be added to each token of the
	 *  specified sentence, separated by a space.
	 *
	 * See <a href="https://wapiti.limsi.fr/manual.html">
	 *  the manual</a> of @c Wapiti for more information.
	 *
	 * \param sentenceBegin Iterator pointing to the
	 *   beginning of the sentence to be tagged.
	 * \param sentenceEnd Iterator pointing to the end
	 *   of the sentence to be tagged.
	 *
	 * \throws Tagger::Exception if an error occurs while
	 *   POS-tagging the sentence.
	 */
	inline void Tagger::label(
			std::vector<std::string>::iterator sentenceBegin,
			std::vector<std::string>::iterator sentenceEnd
	) {
		// check length of sentence and ignore final tokens in REALLY long sentences
		const auto sentenceLength{std::distance(sentenceBegin, sentenceEnd)};

		if(sentenceLength == 0) {
			return;
		}

		if(sentenceLength > std::numeric_limits<std::uint32_t>::max()) {
			sentenceEnd = sentenceBegin + std::numeric_limits<std::uint32_t>::max();
		};

		// copy tokens into continous memory
		std::vector<std::vector<char>> tokenCopies;
		std::vector<char *> tokenPtrs;

		tokenCopies.reserve(sentenceLength);
		tokenPtrs.reserve(sentenceLength);

		for(auto tokenIt{sentenceBegin}; tokenIt != sentenceEnd; ++tokenIt) {
			tokenCopies.emplace_back(tokenIt->begin(), tokenIt->end());
			tokenCopies.back().push_back('\0');
		}

		for(auto& tokenCopy : tokenCopies) {
			tokenPtrs.push_back(tokenCopy.data());
		}

		// convert tokens into raw data
		std::unique_ptr<wapiti::raw_t, wapitiDelete> rawData{
			static_cast<wapiti::raw_t *>(
					wapiti::xmalloc(
							sizeof(wapiti::raw_t) + sizeof(char *) * tokenPtrs.size()
					)
			)
		};

		rawData->len = tokenPtrs.size();

		std::uint32_t index{};

		for(auto * tokenPtr : tokenPtrs) {
			rawData->lines[index] = tokenPtr;

			++index;
		}

		// create sequence
		auto * lbls = this->model->reader->lbl;

		try {
			std::unique_ptr<wapiti::seq_t, wapitiDeleteSeq> seq{
				wapiti::rdr_raw2seq(
						this->model->reader,
						rawData.get(),
						this->model->opt->force
				)
			};

			// label tokens
			const auto T{seq->len};

			std::unique_ptr<uint32_t, wapitiDelete> out{
					static_cast<uint32_t*>(wapiti::xmalloc(sizeof(uint32_t) * T))
			};
			std::unique_ptr<double, wapitiDelete> psc{
					static_cast<double*>(wapiti::xmalloc(sizeof(double  ) * T))
			};
			std::unique_ptr<double, wapitiDelete> scs{
				static_cast<double*>(wapiti::xmalloc(sizeof(double  )))
			};

			wapiti::tag_viterbi(this->model, seq.get(), out.get(), scs.get(), psc.get());

			// append labels to strings
			index = 0;

			for(auto tokenIt{sentenceBegin}; tokenIt != sentenceEnd; ++tokenIt) {
				const std::string label{wapiti::qrk_id2str(lbls, out.get()[index])};

				tokenIt->reserve(tokenIt->size() + label.size() + 1);
				tokenIt->push_back(' ');

				(*tokenIt) += label;

				++index;
			}
		}
		catch(const std::runtime_error& e) {
			throw Exception(
					"Tagger::label():"
					" Error while loading POS-tagging a sentence – "
					+ std::string(e.what())
			);
		}
	}

} /* namespace crawlservpp::Data */

#endif /* DATA_TAGGER_H_ */
