/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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
 * TopicModelInfo.hpp
 *
 * Structure with information about
 *  Hierarchical Dirichlet Process (HDP) models.
 *
 * Using tomoto, the underlying C++ API of tomotopy, see:
 *  https://bab2min.github.io/tomotopy/
 *
 * If you use the HDP topic modelling algorithm, please cite:
 *
 *  Teh, Y. W., Jordan, M. I., Beal, M. J., & Blei, D. M. (2005). Sharing
 *   clusters among related groups: Hierarchical Dirichlet processes.
 *   In Advances in neural information processing systems, 1385–1392.
 *
 *  Newman, D., Asuncion, A., Smyth, P., & Welling, M. (2009). Distributed
 *   algorithms for topic models. Journal of Machine Learning Research,
 *   10 (Aug), 1801–1828.
 *
 * If you use the LDA topic modelling algorithm, please cite:
 *
 *  Blei, D. M., Ng, A. Y., & Jordan, M. I. (2003). Latent dirichlet
 *   allocation. Journal of machine Learning research, 3(Jan), 993–1022.
 *
 *  Newman, D., Asuncion, A., Smyth, P., & Welling, M. (2009). Distributed
 *   algorithms for topic models. Journal of Machine Learning Research,
 *   10 (Aug), 1801–1828.
 *
 * If you use automated topic labeling, please cite:
 *
 *  Mei, Q., Shen, X., & Zhai, C. (2007). Automatic labeling of multinomial
 *   topic models. In Proceedings of the 13th ACM SIGKDD International
 *   Conference on Knowledge Discovery and Data Mining, 490–499.
 *
 *  Created on: Feb 2, 2021
 *      Author: ans
 */

#ifndef STRUCT_HDPMODELINFO_HPP_
#define STRUCT_HDPMODELINFO_HPP_

#include <cstddef>	// std::size_t
#include <queue>	// std::queue
#include <sstream>	// std::ostringstream
#include <string>	// std::string, std::to_string
#include <vector>	// std::vector

namespace crawlservpp::Struct {

	//! Structure containing information about the currently trained Hierarchical Dirichlet Process (HDP) model.
	struct TopicModelInfo {
		///@name Basic Information
		///@{

		//! The name of the model.
		std::string modelName;

		//! The version of the model (as string).
		std::string modelVersion;

		//! The number of documents in the model
		std::size_t numberOfDocuments{};

		//! The number of words in the model.
		std::size_t numberOfWords{};

		// The number of unique words in the model.
		std::size_t sizeOfVocabulary{};

		// The number of unique words used in the model.
		std::size_t sizeOfVocabularyUsed{};

		//! The entropy of words in the model.
		double wordEntropy{};

		//! The (s)top words removed before training.
		std::vector<std::string> removedWords;

		///@}
		///@name Training Information
		///@{

		//! The number of iterations performed.
		std::size_t numberOfIterations{};

		//! The number of initially skipped, i.e. burn-in, steps.
		std::size_t numberOfBurnInSteps{};

		//! The optimization interval.
		std::size_t optimizationInterval{};

		//! The log-likelihood per word.
		double logLikelihoodPerWord{};

		///@}
		///@name Initial Parameters
		///@{

		//! Term weighting mode as string.
		std::string weighting;

		//! Minimum collection frequency of words.
		std::size_t minCollectionFrequency{};

		//! Minimum document frequency of words.
		std::size_t minDocumentFrequency{};

		//! The number of (s)top words to be removed.
		std::size_t numberOfTopWordsToBeRemoved{};

		//! The initial number of topics, which will be adjusted for the data during training.
		std::size_t numberOfInitialTopics{};

		//! The initial concentration coefficient of the Dirichlet Process for document-table.
		float initialAlpha{};

		//! The initial hyperparameter for the Dirichlet distribution for topic-word.
		float initialEta{};

		//! The initial concentration coefficient of the Dirichlet Process for table-topic.
		float initialGamma{};

		//! The initial seed for random number generation.
		std::size_t seed{};

		//! The version of the modeller the model has been trained with.
		std::string trainedWithVersion{};

		///@}
		///@name Parameters
		///@{

		//! The concentration coeficient of the Dirichlet Process for document-table (HDP only).
		float alpha{};

		//! The Dirichlet priors on the per-document topic distributions (LDA only).
		std::vector<float> alphas;

		//! The Dirichlet prior on the per-topic word distribution (HDP only).
		float eta{};

		//! The concentration coefficient of the Dirichlet Process for table-topic.
		/*!
		 * Not used by LDA models, i.e. set to zero
		 *  when a fixed number of topics is set.
		 */
		float gamma{};

		//! The number of topics.
		std::size_t numberOfTopics{};

		//! The number of tables.
		/*!
		 * Not used by LDA models, i.e. set to zero
		 *  when a fixed number of topics is set.
		 */
		std::size_t numberOfTables{};

		///@}
		///@name Helper Function
		///@{

		//! Return queue with strings describing the information contained in the structure.
		std::queue<std::string> toQueueOfStrings() const {
			std::queue<std::string> result;

			result.emplace("<Basic Info>");
			result.emplace(
					"| "
					+ this->modelName
					+ " (current version: "
					+ this->modelVersion
					+ ")"
			);
			result.emplace(
					"| "
					+
					std::to_string(this->numberOfDocuments)
					+ " docs, "
					+ std::to_string(this->numberOfWords)
					+ " words"
			);
			result.emplace(
					"| Total Vocabs: "
					+ std::to_string(this->sizeOfVocabulary)
					+ ", Used Vocabs: "
					+ std::to_string(this->sizeOfVocabularyUsed)
			);
			result.emplace(
					"| Entropy of words: "
					+ std::to_string(this->wordEntropy)
			);

			std::string removed{"| Removed Vocabs:"};

			if(this->removedWords.empty()) {
				removed += " <NA>";
			}
			else {
				for(const auto& word : this->removedWords) {
					removed.push_back(' ');

					removed += word;
				}
			}

			result.emplace(removed);
			result.emplace("|");
			result.emplace("<Training Info>");
			result.emplace(
					"| Iterations: "
					+ std::to_string(this->numberOfIterations)
					+ ", Burn-in steps: "
					+ std::to_string(this->numberOfBurnInSteps)
			);
			result.emplace(
					"| Optimization Interval: "
					+ std::to_string(this->optimizationInterval)
			);
			result.emplace(
					"| Log-likelihood per word: "
					+ std::to_string(this->logLikelihoodPerWord)
			);
			result.emplace("|");
			result.emplace("<Initial Parameters>");
			result.emplace("| tw: " + this->weighting);
			result.emplace(
					"| min_cf: "
					+ std::to_string(this->minCollectionFrequency)
					+ " (minimum collection frequency of words)"
			);
			result.emplace(
					"| min_df: "
					+ std::to_string(this->minDocumentFrequency)
					+ " (minimum document frequency of words)"
			);
			result.emplace(
					"| rm_top: "
					+ std::to_string(this->numberOfTopWordsToBeRemoved)
					+ " (the number of top words to be removed)"
			);
			if(this->numberOfInitialTopics > 0) {
				result.emplace(
						"| initial_k: "
						+ std::to_string(this->numberOfInitialTopics)
						+	" (the initial number of topics between 2 ~ 32767,"
							" which will be adjusted for data during training)"
				);
			}
			else {
				result.emplace(
						"| k: "
						+ std::to_string(this->numberOfTopics)
						+ " (the number of topics between 1 ~ 32767)"
				);
			}
			result.emplace(
					"| alpha: "
					+ std::to_string(this->initialAlpha)
					+ " (concentration coeficient of Dirichlet Process for document-topic)"
			);
			result.emplace(
					"| eta: "
					+ std::to_string(this->initialEta)
					+ " (hyperparameter of Dirichlet distribution for topic-word)"
			);

			if(this->initialGamma > 0.) { /* only used by HDP */
				result.emplace(
						"| gamma: "
						+ std::to_string(this->initialGamma)
						+ " (concentration coeficient of Dirichlet Process for table-topic)"
				);
			}

			result.emplace(
					"| seed: "
					+ std::to_string(this->seed)
					+ " (random seed)"
			);
			if(!(this->trainedWithVersion.empty())) {
				result.emplace(
						"| trained in version " + this->trainedWithVersion
				);
			}
			result.emplace("|");
			result.emplace("<Parameters>");
			if(this->alphas.empty()) {
				result.emplace("| alpha (concentration coeficient of Dirichlet Process for document-table)");
				result.emplace("|  " + std::to_string(this->alpha));
			}
			else { /* only used by LDA */
				result.emplace("| alpha (Dirichlet prior on the per-document topic distributions)");

				constexpr uint8_t lineBreakAfter{6};
				std::string line{"|  ["};
				std::uint8_t lineN{};

				for(const auto a : this->alphas) {
					if(lineN == lineBreakAfter) {
						// remove last space and add line
						line.pop_back();

						result.emplace(line);

						line = "|   ";

						lineN = 0;
					}

					line += std::to_string(a) + " ";

					++lineN;
				}

				line.back() = ']';

				result.emplace(line);
			}
			result.emplace("| eta (Dirichlet prior on the per-topic word distribution)");
			result.emplace("|  " + std::to_string(this->eta));

			if(gamma > 0.) { /* only used by HDP */
				result.emplace("| gamma (concentration coeficient of Dirichlet Process for table-topic)");
				result.emplace("|  " + std::to_string(this->gamma));
			}

			result.emplace("|");
			result.emplace("| Number of Topics: " + std::to_string(this->numberOfTopics));

			if(this->numberOfTables > 0) { /* only used by HDP */
				result.emplace("| Number of Tables: " + std::to_string(this->numberOfTables));
			}

			return result;
		}
	};

} /* namespace crawlservpp:Struct */

#endif /* STRUCT_HDPMODELINFO_HPP_ */
