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
 * Sentiment.hpp
 *
 * Port of VADER sentiment analysis from Python to C++.
 *
 * Original: https://github.com/cjhutto/vaderSentiment/
 *
 * If you use the VADER sentiment analysis tools, please cite:
 *
 * Hutto, C.J. & Gilbert, E.E. (2014). VADER: A Parsimonious Rule-based Model for
 * Sentiment Analysis of Social Media Text. Eighth International Conference on
 * Weblogs and Social Media (ICWSM-14). Ann Arbor, MI, June 2014.
 *
 * !!! FOR ENGLISH LANGUAGE ONLY !!!
 *
 *  Created on: Dec 29, 2020
 *      Author: ans
 */

#ifndef DATA_SENTIMENT_HPP_
#define DATA_SENTIMENT_HPP_

#include <algorithm>		// std::all_of, std::find, std::find_if, std::transform
#include <array>			// std::array
#include <cctype>			// std::ispunct, std::isupper, std::tolower
#include <cmath>			// std::fabs, std::sqrt
#include <cstddef>			// std::size_t
#include <cstdint>			// std::uint8_t, std::uint64_t
#include <fstream>			// std::ifstream
#include <iterator>			// std::back_inserter
#include <limits>			// std::numeric_limits
#include <numeric>			// std::accumulate
#include <stdexcept>		// std::runtime_error
#include <string>			// std::getline, std::stof, std::string
#include <string_view>		// std::string_view
#include <unordered_map>	// std::unordered_set
#include <unordered_set>	// std::unordered_map
#include <utility>			// std::pair
#include <vector>			// std::vector

namespace crawlservpp::Data {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Zero.
	inline constexpr auto VaderZero{0};

	//! One.
	inline constexpr auto VaderOne{1};

	//! Two.
	inline constexpr auto VaderTwo{2};

	//! Three.
	inline constexpr auto VaderThree{3};

	//! Four.
	inline constexpr auto VaderFour{4};

	//! Factor of One.
	inline constexpr auto VaderFOne{1.F};

	//! Factor by which the scalar modifier of immediately preceding words is dampened.
	inline constexpr auto VaderDampOne{0.95F};

	//! Factor by which the scalar modifier of previously preceding words is dampened.
	inline constexpr auto VaderDampTwo{0.9F};

	//! Factor by which the modifier is dampened before a "but".
	inline constexpr auto VaderButFactorBefore{0.5F};

	//! Factor by which the modifier is heightened after a "but".
	inline constexpr auto VaderButFactorAfter{1.5F};

	//! Factor by which the modifier is heightened after a "never".
	inline constexpr auto VaderNeverFactor{1.25F};

	/*
	//! Empirically derived mean sentiment intensity rating increase for exclamation marks.
	inline constexpr auto VaderEPFactor{0.292F};

	//! Empirically derived mean sentiment intensity rating increase for each question mark.
	inline constexpr auto VaderQMFactor{0.18F};

	//! Maximum sentiment intensity rating increase for question marks.
	inline constexpr auto VaderQMFactorMax{0.96F};
	*/

	//! Empirically derived mean sentiment intensity rating increase for booster words.
	inline constexpr auto VaderB_INCR{0.293F};

	//! Empirically derived mean sentiment intensity rating decrease for negative booster words.
	inline constexpr auto VaderB_DECR{-0.293F};

	//! Empirically derived mean sentiment intensity rating increase for using ALLCAPs to emphasize a word.
	inline constexpr auto VaderC_INCR{0.733F};

	//! Negation factor.
	inline constexpr auto VaderN_SCALAR{-0.74F};

	///@}

	/*
	 * DECLARATION
	 */

	//! Structure for VADER sentiment scores.
	struct SentimentScores {
		//! Positive sentiment.
		/*!
		 * The positive, neutral, and negative
		 *  scores are ratios for proportions of
		 *  text that fall in each category (so
		 *  these should all add up to be 1...
		 *  or close to it with float operation).
		 *
		 *  These are the most useful metrics if
		 *  you want multidimensional measures of
		 *  sentiment for a given sentence.
		 *
		 *  \sa neutral, negative
		 */
		float positive{};

		//! Neutral sentiment.
		/*!
		 * The positive, neutral, and negative
		 *  scores are ratios for proportions of
		 *  text that fall in each category (so
		 *  these should all add up to be 1...
		 *  or close to it with float operation).
		 *
		 *  These are the most useful metrics if
		 *  you want multidimensional measures of
		 *  sentiment for a given sentence.
		 *
		 *  \sa positive, negative
		 */
		float neutral{};

		//! Negative sentiment.
		/*!
		 * The positive, neutral, and negative
		 *  scores are ratios for proportions of
		 *  text that fall in each category (so
		 *  these should all add up to be 1...
		 *  or close to it with float operation).
		 *
		 *  These are the most useful metrics if
		 *  you want multidimensional measures of
		 *  sentiment for a given sentence.
		 *
		 *  \sa positive, neutral
		 */
		float negative{};

		//! Compound score.
		/*!
		 * This score is computed by summing the
		 *  valence scores of each word in the
		 *  lexicon, adjusted according to the rules,
		 *  and then normalized to be between -1
		 *  (most extreme negative) and +1 (most
		 *  extreme positive).
		 *
		 * This is the most useful metric if you want
		 *  a single unidimensional measure of
		 *  sentiment for a given sentence.
		 *
		 * Calling it a 'normalized, weighted composite
		 *  score' is accurate.
		 */
		float compound{};
	};

	//! Implementation of the VADER sentiment analysis algorithm.
	/*!
	 * See:
	 *
	 * Hutto, C.J. & Gilbert, E.E. (2014). VADER: A Parsimonious Rule-based Model for
	 * Sentiment Analysis of Social Media Text. Eighth International Conference on
	 * Weblogs and Social Media (ICWSM-14). Ann Arbor, MI, June 2014.
	 *
	 * \warning For English language only!
	 */
	class Sentiment {
		// for convinience
		using WORDS = std::vector<std::string>;

		/*
		 * CONSTANTS
		 */

		inline static const std::unordered_set<std::string_view> NEGATE{
			"aint",
			"arent",
			"cannot",
			"cant",
			"couldnt",
			"darent",
			"didnt",
			"doesnt",
			"ain't",
			"aren't",
			"can't",
			"couldn't",
			"daren't",
			"didn't",
			"doesn't",
			"dont",
			"hadnt",
			"hasnt",
			"havent",
			"isnt",
			"mightnt",
			"mustnt",
			"neither",
			"don't",
			"hadn't",
			"hasn't",
			"haven't",
			"isn't",
			"mightn't",
			"mustn't",
			"neednt",
			"needn't",
			"never",
			"none",
			"nope",
			"nor",
			"not",
			"nothing",
			"nowhere",
			"oughtnt",
			"shant",
			"shouldnt",
			"uhuh",
			"wasnt",
			"werent",
			"oughtn't",
			"shan't",
			"shouldn't",
			"uh-uh",
			"wasn't",
			"weren't",
			"without",
			"wont",
			"wouldnt",
			"won't",
			"wouldn't",
			"rarely",
			"seldom",
			"despite"
		};

		// booster/dampener 'intensifiers' or 'degree adverbs'
		// http://en.wiktionary.org/wiki/Category:English_degree_adverbs
		inline static const std::unordered_map<std::string_view, float> BOOSTER_DICT{
			{ "absolutely", VaderB_INCR },
			{ "amazingly", VaderB_INCR },
			{ "awfully", VaderB_INCR },
			{ "completely", VaderB_INCR },
			{ "considerable", VaderB_INCR },
			{ "considerably", VaderB_INCR },
			{ "decidedly", VaderB_INCR },
			{ "deeply", VaderB_INCR },
			{ "effing", VaderB_INCR },
			{ "enormous", VaderB_INCR },
			{ "enormously", VaderB_INCR },
			{ "entirely", VaderB_INCR },
			{ "especially", VaderB_INCR },
			{ "exceptional", VaderB_INCR },
			{ "exceptionally", VaderB_INCR },
			{ "extreme", VaderB_INCR },
			{ "extremely", VaderB_INCR },
			{ "fabulously", VaderB_INCR },
			{ "flipping", VaderB_INCR },
			{ "flippin", VaderB_INCR },
			{ "frackin", VaderB_INCR },
			{ "fracking", VaderB_INCR },
			{ "fricking", VaderB_INCR },
			{ "frickin", VaderB_INCR },
			{ "frigging", VaderB_INCR },
			{ "friggin", VaderB_INCR },
			{ "fully", VaderB_INCR },
			{ "fuckin", VaderB_INCR },
			{ "fucking", VaderB_INCR },
			{ "fuggin", VaderB_INCR },
			{ "fugging", VaderB_INCR },
			{ "greatly", VaderB_INCR },
			{ "hella", VaderB_INCR },
			{ "highly", VaderB_INCR },
			{ "hugely", VaderB_INCR },
			{ "incredible", VaderB_INCR },
			{ "incredibly", VaderB_INCR },
			{ "intensely", VaderB_INCR },
			{ "major", VaderB_INCR },
			{ "majorly", VaderB_INCR },
			{ "more", VaderB_INCR },
			{ "most", VaderB_INCR },
			{ "particularly", VaderB_INCR },
			{ "purely", VaderB_INCR },
			{ "quite", VaderB_INCR },
			{ "really", VaderB_INCR },
			{ "remarkably", VaderB_INCR },
			{ "so", VaderB_INCR },
			{ "substantially", VaderB_INCR },
			{ "thoroughly", VaderB_INCR },
			{ "total", VaderB_INCR },
			{ "totally", VaderB_INCR },
			{ "tremendous", VaderB_INCR },
			{ "tremendously", VaderB_INCR },
			{ "uber", VaderB_INCR },
			{ "unbelievably", VaderB_INCR },
			{ "unusually", VaderB_INCR },
			{ "utter", VaderB_INCR },
			{ "utterly", VaderB_INCR },
			{ "very", VaderB_INCR },
			{ "almost", VaderB_DECR },
			{ "barely", VaderB_DECR },
			{ "hardly", VaderB_DECR },
			{ "just enough", VaderB_DECR },
			{ "kind of", VaderB_DECR },
			{ "kinda", VaderB_DECR },
			{ "kindof", VaderB_DECR },
			{ "kind-of", VaderB_DECR },
			{ "less", VaderB_DECR },
			{ "little", VaderB_DECR },
			{ "marginal", VaderB_DECR },
			{ "marginally", VaderB_DECR },
			{ "occasional", VaderB_DECR },
			{ "occasionally", VaderB_DECR },
			{ "partly", VaderB_DECR },
			{ "scarce", VaderB_DECR },
			{ "scarcely", VaderB_DECR },
			{ "slight", VaderB_DECR },
			{ "slightly", VaderB_DECR },
			{ "somewhat", VaderB_DECR },
			{ "sort of", VaderB_DECR },
			{ "sorta", VaderB_DECR },
			{ "sortof", VaderB_DECR },
			{ "sort-of", VaderB_DECR}
		};

		// check for special case idioms and phrases containing lexicon words
		inline static const std::unordered_map<std::string_view, float> SPECIAL_CASES{
			{ "the shit", 3.F },
			{ "the bomb", 3.F },
			{ "bad ass", 1.5F },
			{ "badass", 1.5F },
			{ "bus stop", 0.F },
			{ "yeah right", -2.F },
			{ "kiss of death", -1.5F },
			{ "to die for", 3.F },
			{ "beating heart", 3.1F },
			{ "broken heart", -2.9F }
		};

	public:
		///@name Construction
		///@{

		Sentiment(const std::string& dictionaryFile, const std::string& emojiFile);

		///@}
		///@name Getters
		///@{

		[[nodiscard]] std::size_t getDictSize() const;
		[[nodiscard]] std::size_t getEmojiNum() const;

		///@}
		///@name Sentiment Analysis
		///@{

		[[nodiscard]] SentimentScores analyze(const WORDS& words);

		///@}

	private:
		// dictionaries
		std::unordered_map<std::string, float> dictMap;
		std::unordered_map<std::string, std::string> emojiMap;

		// internal helper functions
		void sentimentValence(
				float& valence,
				const WORDS& words,
				const WORDS& wordsLower,
				std::size_t index,
				std::vector<float>& sentiments,
				bool isCapDifference
		);
		SentimentScores scoreValence(const std::vector<float>& sentiments, const WORDS& words);
		void leastCheck(float& valence, const WORDS& wordsLower, std::size_t index) const;

		// static internal helper functions
		[[nodiscard]] static std::vector<std::string> toLower(const WORDS& words);
		[[nodiscard]] static bool isNegated(const std::string& wordLower);
		[[nodiscard]] static bool isNegated(const WORDS& wordsLower);
		[[nodiscard]] static float normalize(float score);
		[[nodiscard]] static bool isAllCaps(const std::string& word);
		[[nodiscard]] static bool isAllCapDifferential(const WORDS& words);
		[[nodiscard]] static float scalarIncDec(
				const std::string& word,
				const std::string& wordLower,
				float valence,
				bool isCapDiff
		);
		/*
		 * (the following functions are not used, because punctuation is removed by the tokenizer)
		 *
		[[nodiscard]] static float punctuationEmphasis(const WORDS& words);
		[[nodiscard]] static float amplifyEP(const WORDS& words);
		[[nodiscard]] static float amplifyQM(const WORDS& words);
		*/

		static void butCheck(const WORDS& wordsLower, std::vector<float>& sentiments);
		static void negationCheck(float& valence, const WORDS& wordsLower, std::uint8_t startIndex, std::size_t index);
		static void specialIdiomsCheck(float& valence, const WORDS& wordsLower, std::size_t index);
		static void siftSentimentScores(
				const std::vector<float>& sentiments,
				float& positiveSumTo,
				float& negativeSumTo,
				std::size_t& neutralCountTo
		);
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * CONSTRUCTION
	 */

	//! Constructor.
	/*!
	 * Creates the dictionaries from the given files.
	 *
	 * \param dictionaryFile Constant reference
	 *   to a string containing the file name
	 *   of the dictionary to be used.
	 * \param emojiFile Constant reference to a
	 *   string containing the file name of the
	 *   emoji dictionary to be used.
	 */
	inline Sentiment::Sentiment(const std::string& dictionaryFile, const std::string& emojiFile) {
		std::ifstream dictIn(dictionaryFile.c_str());
		std::string line;

		if(!dictIn.is_open()) {
			throw std::runtime_error("Could not open dictionary file: '" + dictionaryFile + "'");
		}

		while(std::getline(dictIn, line)) {
			const auto firstTab{line.find('\t')};

			if(firstTab != std::string::npos) {
				const auto term{line.substr(0, firstTab)};
				const auto secondTab{line.find('\t', firstTab + 1)};
				const auto value{std::stof(line.substr(firstTab + 1, secondTab - firstTab))};

				this->dictMap.emplace_hint(this->dictMap.end(), term, value);
			}
		}

		dictIn.close();

		std::ifstream emojiIn(emojiFile.c_str());

		if(!emojiIn.is_open()) {
			throw std::runtime_error("Could not open emoji file: '" + emojiFile + "'");
		}

		while(std::getline(emojiIn, line)) {
			const auto tab{line.find('\t')};

			if(tab != std::string::npos) {
				const auto emoji{line.substr(0, tab)};
				const auto value{line.substr(tab + 1)};

				this->emojiMap.emplace_hint(this->emojiMap.end(), emoji, value);
			}
		}

		emojiIn.close();
	}

	/*
	 * GETTERS
	 */

	//! Gets the number of dictionary entries.
	/*!
	 * \returns Number of entries in the dictionary.
	 */
	std::size_t Sentiment::getDictSize() const {
		return this->dictMap.size();
	}

	//! Gets the number of entries in the emoji dictionary.
	/*!
	 * \returns Number of emojis in the dictionary.
	 */
	std::size_t Sentiment::getEmojiNum() const {
		return this->emojiMap.size();
	}

	/*
	 * SENTIMENT ANALYSIS
	 */

	//! Get the sentiment strength in the given sentence.
	/*!
	 * \param words Constant reference to a vector
	 *   containing the words of the sentence.
	 *
	 * \returns Floating point number representing
	 *   sentiment strength based on input.
	 *   Positive values are positive valence,
	 *   negative values are negative valence.
	 */
	SentimentScores Sentiment::analyze(const WORDS& words) {
		const bool isCapDifference{
			Sentiment::isAllCapDifferential(words)
		};

		// replace emojis
		std::vector<std::string> newWords;

		// copy trimmed string
		newWords.reserve(words.size());

		for(auto& word : words) {
			std::string wordCopy;

			std::size_t beg{};

			for(; beg < word.length(); ++beg) {
				const auto c{word[beg]};

				if(!std::ispunct(c) && !std::iscntrl(c) && c != ' ') {
					break;
				}
			}

			std::size_t len{word.length() - beg};

			do {
				const auto c{word[beg + len - VaderOne]};

				if(!std::ispunct(c) && !std::iscntrl(c) && c != ' ') {
					break;
				}

				--len;
			} while(len > 0);

			wordCopy = word.substr(beg, len);

			const auto it = this->emojiMap.find(wordCopy);

			if(it != this->emojiMap.end()) {
				std::string emojiWord;

				emojiWord.reserve(it->second.length());

				for(auto c : it->second) {
					switch(c) {
					case ' ':
						if(!emojiWord.empty()) {
							newWords.emplace_back(emojiWord);

							emojiWord.clear();
						}

						break;

					default:
						emojiWord.push_back(c);
					}
				}

				if(!emojiWord.empty()) {
					newWords.emplace_back(emojiWord);
				}
			}
			else {
				newWords.emplace_back(wordCopy);
			}
		}

		// create copy with lower-case words
		std::vector<std::string> wordsLower{Sentiment::toLower(newWords)};

		// calculate sentiments
		std::vector<float> sentiments;

		sentiments.reserve(newWords.size());

		for(std::size_t index{}; index < newWords.size(); ++index) {
			float valence{};

			if(std::find_if(BOOSTER_DICT.begin(), BOOSTER_DICT.end(), [&wordsLower, &index](const auto& booster) {
				return booster.first == wordsLower[index];
			}) != BOOSTER_DICT.end()) {
				sentiments.push_back(valence);

				continue;
			}

			if(index < newWords.size() - 1 && wordsLower[index] == "kind" && wordsLower[index + 1] == "of") {
				sentiments.push_back(valence);

				continue;
			}

			this->sentimentValence(valence, newWords, wordsLower, index, sentiments, isCapDifference);
		}

		Sentiment::butCheck(wordsLower, sentiments);

		return this->scoreValence(sentiments, newWords);
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// calculate sentiment valence
	void Sentiment::sentimentValence(
				float& valence,
				const WORDS& words,
				const WORDS& wordsLower,
				std::size_t index,
				std::vector<float>& sentiments,
				bool isCapDifference
	) {
		// get the sentiment valence
		const auto it{this->dictMap.find(wordsLower[index])};

		if(it != this->dictMap.end()) {
			valence = it->second;

			// check for "no" as negation for an adjacent lexicon item vs "no" as its own stand-alone lexicon item
			if(
					wordsLower[index] == "no"
					&& index < words.size() - VaderOne
					&& this->dictMap.find(wordsLower[index + VaderOne]) != this->dictMap.end()
			) {
				// don't use valence of "no" as a lexicon item. Instead set it's valence to 0.0 and negate the next item
				valence = 0.F;
			}

			if(
					(index > 0 && wordsLower[index - VaderOne] == "no")
					|| (index > 1 && wordsLower[index - VaderTwo] == "no")
					|| (
							index > 2
							&& wordsLower[index - VaderThree] == "no"
							&& (
									wordsLower[index - VaderOne] == "or"
									|| wordsLower[index - VaderOne] == "nor"
							)
					)
			) {
				valence = it->second;
			}

			// check if sentiment laden word is in ALL CAPS (while others aren't)
			if(Sentiment::isAllCaps(words[index]) && isCapDifference) {
				if(valence > 0.F) {
					valence += VaderC_INCR;
				}
				else {
					valence -= VaderC_INCR;
				}
			}

			for(std::uint8_t startIndex{0}; startIndex < VaderThree; ++startIndex) {
				// dampen the scalar modifier of preceding words and emoticons
				// (excluding the ones that immediately preceed the item) based
				// on their distance from the current item.
				if(index > startIndex) {
					const auto& precWord{words[index - (startIndex + VaderOne)]};
					const auto& precWordLower{wordsLower[index - (startIndex + VaderOne)]};

					if(this->dictMap.find(precWordLower) == this->dictMap.end()) {
						float s{Sentiment::scalarIncDec(precWord, precWordLower, valence, isCapDifference)};

						if(std::fabs(s) <= std::numeric_limits<float>::epsilon()) {
							if(startIndex == VaderOne) {
								s *= VaderDampOne;
							}
							else if(startIndex == VaderTwo) {
								s *= VaderDampTwo;
							}
						}

						valence += s;

						Sentiment::negationCheck(valence, wordsLower, startIndex, index);

						if(startIndex == VaderTwo) {
							Sentiment::specialIdiomsCheck(valence, wordsLower, index);
						}
					}
				}
			}

			this->leastCheck(valence, wordsLower, index);
		}

		sentiments.push_back(valence);
	}

	// calculate valence score
	SentimentScores Sentiment::scoreValence(const std::vector<float>& sentiments, const WORDS& words) {
		if(sentiments.empty()) {
			return SentimentScores{};
		}

		auto sum{std::accumulate(sentiments.begin(), sentiments.end(), 0.F)};

		/*
		 * (the following code is not used, because punctuation is removed by the tokenizer)
		 *
		const auto punctEmphAmp{Sentiment::punctuationEmphasis(words)};

		// compute and add emphasis from punctuation in text
		if(sum > 0.F) {
			sum += punctEmphAmp;
		}
		else if(sum < 0.F) {
			sum -= punctEmphAmp;
		}
		*/

		SentimentScores result;
		std::size_t neuCount{};

		result.compound = Sentiment::normalize(sum);

		Sentiment::siftSentimentScores(sentiments, result.positive, result.negative, neuCount);

		/*
		 * (the following code is not used, because punctuation is removed by the tokenizer)
		 *
		if(result.positive > std::fabs(result.negative)) {
			result.positive += punctEmphAmp;
		}
		else if(result.positive  < std::fabs(result.negative)) {
			result.negative -= punctEmphAmp;
		}
		*/

		const auto total{result.positive + std::fabs(result.negative) + neuCount};

		result.positive = std::fabs(result.positive / total);
		result.negative = std::fabs(result.negative / total);
		result.neutral = std::fabs(static_cast<float>(neuCount) / total);

		return result;
	}

	// check for negation case using "least"
	void Sentiment::leastCheck(float& valence, const WORDS& wordsLower, std::size_t index) const {
		if(
				index > VaderOne
				&& this->dictMap.find(wordsLower[index - VaderOne]) == this->dictMap.end()
				&& wordsLower[index - VaderOne] == "least"
		) {
			if(
					wordsLower[index - VaderTwo] != "at"
					&& wordsLower[index - VaderTwo] != "very"
			) {
				valence *= VaderN_SCALAR;
			}
		}
		else if(
			index > VaderZero
			&& this->dictMap.find(wordsLower[index - VaderOne]) == this->dictMap.end()
			&& wordsLower[index - VaderOne] == "least"
		) {
			valence *= VaderN_SCALAR;
		}
	}

	/*
	 * STATIC INTERNAL HELPER FUNCTIONS (private)
	 */

	// Create lower-case copies of given words
	std::vector<std::string> Sentiment::toLower(const WORDS& words) {
		std::vector<std::string> wordsLower;

		wordsLower.reserve(words.size());

		std::transform(words.cbegin(), words.cend(), std::back_inserter(wordsLower), [](const auto& word) {
			std::string wordLower;

			wordLower.reserve(word.size());

			std::transform(word.cbegin(), word.cend(), std::back_inserter(wordLower), [](const auto c) {
				return std::tolower(c);
			});

			return wordLower;
		});

		return wordsLower;
	}

	bool Sentiment::isNegated(const std::string& wordLower) {
		if(Sentiment::NEGATE.find(wordLower) != Sentiment::NEGATE.end()) {
			return true;
		}

		if(wordLower.find("n't") != std::string::npos) {
			return true;
		}

		return false;
	}

	// Determine if input contains negation words (NOTE: strings in vector need to be lowercase!)
	bool Sentiment::isNegated(const WORDS& wordsLower) {
		for(const auto& wordLower : wordsLower) {
			if(Sentiment::isNegated(wordLower)) {
				return true;
			}
		}

		return false;
	}

	// Normalize the score to be between -1 and 1 using an alpha that approximates the max expected value
	float Sentiment::normalize(float score) {
		constexpr auto alpha{15};

		const float normScore{score / std::sqrt((score * score) + alpha)};

		if(normScore < -1.F) {
			return -1.F;
		}
		else if(normScore > 1.F) {
			return 1.F;
		}

		return normScore;
	}

	// Check whether word is ALL CAPS
	bool Sentiment::isAllCaps(const std::string& word) {
		return std::all_of(word.begin(), word.end(), [](const char c) {
			return std::isupper(c);
		});
	}

	// Check whether just some words in the input are ALL CAPS (returns false if ALL or NONE of the words are ALL CAPS)
	bool Sentiment::isAllCapDifferential(const WORDS& words) {
		std::size_t allCapWords{0};

		for(const auto& word : words) {
			if(Sentiment::isAllCaps(word)) {
				++allCapWords;
			}
		}

		return allCapWords > 0 && allCapWords < words.size();
	}

	// Check if the preceding words increase, decrease, or negate/nullify the valence
	float Sentiment::scalarIncDec(
				const std::string& word,
				const std::string& wordLower,
				float valence,
				bool isCapDiff
	) {
		float scalar{};

		const auto it{Sentiment::BOOSTER_DICT.find(wordLower)};

		if(it != Sentiment::BOOSTER_DICT.end()) {
			scalar = it->second;

			if(valence < 0.F) {
				scalar *= -1.F;
			}

			if(isAllCaps(word) && isCapDiff) {
				if(valence > 0.F) {
					scalar += VaderC_INCR;
				}
				else {
					scalar -= VaderC_INCR;
				}
			}
		}

		return scalar;
	}

	/*
	 * (the following functions are not used, because punctuation is removed by the tokenizer)
	 *
	// add emphasis from exclamation points and question marks
	float Sentiment::punctuationEmphasis(const WORDS& words) {
		return Sentiment::amplifyEP(words) + amplifyQM(words);
	}

	float Sentiment::amplifyEP(const WORDS& words) {
		auto epCount{std::accumulate(words.begin(), words.end(), std::uint64_t{}, [](auto count, const auto& word) {
			return count + std::count(word.begin(), word.end(), '!');
		})};

		if(epCount > Four) {
			epCount = Four;
		}

		return VaderEPFactor * epCount;
	}

	float Sentiment::amplifyQM(const WORDS& words) {
		auto qmCount{std::accumulate(words.begin(), words.end(), std::uint64_t{}, [](auto count, const auto& word) {
			return count + std::count(word.begin(), word.end(), '?');
		})};

		if(qmCount > One) {
			if(qmCount <= Three) {
				return VaderQMFactor * qmCount;
			}

			return VaderQMFactorMax;
		}

		return 0.F;
	}
	*/

	// check for modification in sentiment due to contrastive conjunction 'but'
	void Sentiment::butCheck(const WORDS& wordsLower, std::vector<float>& sentiments) {
		const auto it{std::find(wordsLower.cbegin(), wordsLower.cend(), "but")};

		if(it != wordsLower.cend()) {
			const auto butIndex{static_cast<std::size_t>(it - wordsLower.begin())};

			for(std::size_t index{}; index < sentiments.size(); ++index) {
				if(index < butIndex) {
					sentiments[index] *= VaderButFactorBefore;
				}
				else if(index > butIndex) {
					sentiments[index] *= VaderButFactorAfter;
				}
			}
		}
	}

	// check for negation (either by "never so/this" or by "without doubt")
	void Sentiment::negationCheck(float& valence, const WORDS& wordsLower, std::uint8_t startIndex, std::size_t index) {
		switch(startIndex) {
		case VaderZero:
			if(Sentiment::isNegated(wordsLower[index - (startIndex + VaderOne)])) {
				// 1 word preceding lexicon word (w/o stopwords)
				valence *= VaderN_SCALAR;
			}
			break;

		case VaderOne:
			if(
					wordsLower[index - VaderTwo] == "never"
					&& (
							wordsLower[index - VaderOne] == "so"
							|| wordsLower[index - VaderOne] == "this"
					)
			) {
				valence *= VaderNeverFactor;
			}
			else if(
					wordsLower[index - VaderTwo] == "without"
					&& wordsLower[index - VaderOne] == "doubt"
			) {
				// (ignore)
			}
			else if(
					Sentiment::isNegated(wordsLower[index - (startIndex + VaderOne)])
			) {
				// 2 words preceding the lexicon word position
				valence *= VaderN_SCALAR;
			}

			break;

		case VaderTwo:
			if(
					wordsLower[index - VaderThree] == "never"
					&& (
							wordsLower[index - VaderTwo] == "so"
							|| wordsLower[index - VaderTwo] == "this"
							|| (wordsLower[index - VaderOne] == "so"
							|| wordsLower[index - VaderOne] == "this")
					)
			) {
				valence *= VaderNeverFactor;
			}
			else if(
					wordsLower[index - VaderThree] == "without"
					&& (
							wordsLower[index - VaderTwo] == "doubt"
							|| wordsLower[index - VaderOne] == "doubt"
					)
			) {
				// (ignore)
			}
			else if(
					Sentiment::isNegated(wordsLower[index - (startIndex + VaderOne)])
			) {
				// 3 words preceding the lexicon word position
				valence *= VaderN_SCALAR;
			}

			break;

		default:
			break;
		}
	}

	// check for special idioms
	void Sentiment::specialIdiomsCheck(float& valence, const WORDS& wordsLower, std::size_t index) {
		const auto oneZero{
			wordsLower[index - VaderOne]
			+ " "
			+ wordsLower[index]
		};

		const auto twoOneZero{
			wordsLower[index - VaderTwo]
			+ " "
			+ wordsLower[index - VaderOne]
			+ " "
			+ wordsLower[index]
		};

		const auto twoOne{
			wordsLower[index - VaderTwo]
			+ " "
			+ wordsLower[index - VaderOne]
		};

		const auto threeTwoOne{
			wordsLower[index - VaderThree]
			+ " "
			+ wordsLower[index - VaderTwo]
			+ " "
			+ wordsLower[index - VaderOne]
		};

		const auto threeTwo{
			wordsLower[index - VaderThree]
			+ " "
			+ wordsLower[index - VaderTwo]
		};

		const std::array sequences{oneZero, twoOneZero, twoOne, threeTwoOne, threeTwo};

		for(const auto& sequence : sequences) {
			const auto it{Sentiment::SPECIAL_CASES.find(sequence)};

			if(it != Sentiment::SPECIAL_CASES.end()) {
				valence = it->second;

				break;
			}
		}

		if(wordsLower.size() - VaderOne > index) {
			const auto zeroOne{
				wordsLower[index]
				+ " "
				+ wordsLower[index + VaderOne]
			};

			const auto it{Sentiment::SPECIAL_CASES.find(zeroOne)};

			if(it != Sentiment::SPECIAL_CASES.end()) {
				valence = it->second;
			}
		}

		if(wordsLower.size() - VaderOne > index + VaderOne) {
			const auto zeroOneTwo{
				wordsLower[index]
				+ " "
				+ wordsLower[index + VaderOne]
				+ " "
				+ wordsLower[index + VaderTwo]
			};

			const auto it{Sentiment::SPECIAL_CASES.find(zeroOneTwo)};

			if(it != Sentiment::SPECIAL_CASES.end()) {
				valence = it->second;
			}
		}

		// check for booster/dampener bi-grams such as 'sort of' or 'kind of'
		const std::array nGrams{threeTwoOne, threeTwo, twoOne};

		for(const auto& nGram : nGrams) {
			const auto it{Sentiment::BOOSTER_DICT.find(nGram)};

			if(it != Sentiment::BOOSTER_DICT.end()) {
				valence += it->second;
			}
		}
	}

	// calculate final sentiment scores
	void Sentiment::siftSentimentScores(
				const std::vector<float>& sentiments,
				float& positiveSumTo,
				float& negativeSumTo,
				std::size_t& neutralCountTo
	) {
		for(const auto sentiment : sentiments) {
			if(sentiment > std::numeric_limits<float>::epsilon()) {
				positiveSumTo += sentiment + VaderFOne; // compensates for neutral words that are counted as 1
			}
			else if(sentiment < -std::numeric_limits<float>::epsilon()) {
				negativeSumTo += sentiment - VaderFOne; // when used with fabs(), compensates for neutrals
			}
			else {
				++neutralCountTo;
			}
		}
	}

} /* namespace crawlservpp::Data */

#endif /* DATA_SENTIMENT_HPP_ */
