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
 * German.hpp
 *
 * Simple German stemmer based on CISTEM by Leonie Weißweiler and Alexander Fraser.
 *
 *  Original: https://github.com/LeonieWeissweiler/CISTEM
 *
 *  See:
 *
 *  Weißweiler, Leonie / Fraser, Alexander: Developing a Stemmer for
 *   German Based on a Comparative Analysis of Publicly Available Stemmers,
 *   in: Proceedings of the German Society for Computational Linguistics and
 *   Language Technology (GSCL), 27th International Conference. Berlin,
 *   September 13–14, 2017.
 *
 *
 *  Created on: Aug 2, 2020
 *      Author: ans
 */

#ifndef DATA_STEMMER_GERMAN_HPP_
#define DATA_STEMMER_GERMAN_HPP_

#include <cstddef>	// std::size_t
#include <string>	// std::string

namespace crawlservpp::Data::Stemmer {

	/*
	 * CONSTANTS
	 */

	///@{

	//! Minimum length of a word to strip two letters from the end or the beginning.
	inline constexpr auto minLengthStrip2{6};

	//! Minimum length of a word to strip one letter from the end.
	inline constexpr auto minLengthStrip1{4};

	//! Literal for binary inversion.
	inline constexpr auto binInv{0xff};

	//! Number to add to make uppercase ASCII letters lowercase.
	inline constexpr auto toLowerCase{32};

	//! First byte of 2-byte UTF-8 characters for umlauts and sharp s.
	inline constexpr auto utf8mb2{0xC3};

	//! First byte of 3-byte UTF-8 character for capital sharp s.
	inline constexpr auto utf8mb3{0xE1};

	//! Second byte of UTF-8 umlaut ä.
	inline constexpr auto umlautA2sm{0xA4};

	//! Second byte of UTF-8 umlaut Ä.
	inline constexpr auto umlautA2l{0x84};

	//! Second byte of UTF-8 umlaut ö.
	inline constexpr auto umlautO2sm{0xB6};

	//! Second byte of UTF-8 umlaut Ö.
	inline constexpr auto umlautO2l{0x96};

	//! Second byte of UTF-8 umlaut ü.
	inline constexpr auto umlautU2sm{0xBC};

	//! Second byte of UTF-8 umlaut Ü.
	inline constexpr auto umlautU2l{0x9C};

	//! Second byte of UTF-8 sharp s.
	inline constexpr auto sharpS2sm{0x9F};

	//! Second byte of UTF-8 capital sharp s.
	inline constexpr auto sharpS2l{0xBA};

	//! Third byte of UTF-8 capital sharp s.
	inline constexpr auto sharpS3l{0x9E};

	///@}

	/*
	 * DECLARATION
	 */

	void stemGerman(std::string& token);

	/*
	 * IMPLEMENTATION
	 */

	//! Stems a token in German.
	/*!
	 * \param token The token to be stemmed
	 *   in situ.
	 */
	inline void stemGerman(std::string& token) {
		if(token.empty()) {
			return;
		}

		if(token.size() == 1) {
			switch(token[0]) {
			// remove punctuation
			case '"':
			case '!':
			case '#':
			case '$':
			case '%':
			case '&':
			case '\'':
			case '(':
			case ')':
			case '*':
			case '+':
			case ',':
			case '-':
			case '.':
			case '/':
			case ':':
			case ';':
			case '<':
			case '=':
			case '>':
			case '?':
			case '@':
			case '[':
			case '\\':
			case ']':
			case '^':
			case '_':
			case '`':
			case '{':
			case '|':
			case '}':
			case '~':
				token.clear();

				break;

			// transform to lower-case
			case 'A':
			case 'B':
			case 'C':
			case 'D':
			case 'E':
			case 'F':
			case 'G':
			case 'H':
			case 'I':
			case 'J':
			case 'K':
			case 'L':
			case 'M':
			case 'N':
			case 'O':
			case 'P':
			case 'Q':
			case 'R':
			case 'S':
			case 'T':
			case 'U':
			case 'V':
			case 'W':
			case 'X':
			case 'Y':
			case 'Z':
				token[0] += toLowerCase;

				break;

			default:
				break;
			}

			return;
		}

		// replace umlauts and sharp s
		for(std::size_t n{1}; n < token.size(); ++n) {
			if((binInv & token[n - 1]) == utf8mb2) { //NOLINT(hicpp-signed-bitwise)
				switch(binInv & token[n]) { //NOLINT(hicpp-signed-bitwise)
				case umlautA2sm:
				case umlautA2l:
					//ä
					token.erase(n, 1);

					token[n - 1] = 'a';

					break;

				case umlautO2sm:
				case umlautO2l:
					//ö
					token.erase(n, 1);

					token[n - 1] = 'o';

					break;

				case umlautU2sm:
				case umlautU2l:
					//ü
					token.erase(n, 1);

					token[n - 1] = 'u';

					break;

				case sharpS2sm:
					//ß
					token[n - 1] = 's';
					token[n] = 's';

					++n;

					break;

				default:
					continue;
				}
			}
			else if(
					(binInv & token[n - 1]) == utf8mb3 //NOLINT(hicpp-signed-bitwise)
					&& (binInv & token[n]) == sharpS2l //NOLINT(hicpp-signed-bitwise)
					&& (n + 1) < token.size()
					&& (binInv & token[n + 1]) == sharpS3l //NOLINT(hicpp-signed-bitwise)
			) {
				//ẞ
				token.erase(n, 1);

				token[n - 1] = 's';
				token[n] = 's';

				++n;
			}
			else {
				switch(token[n - 1]) {
				// remove punctuation
				case '"':
				case '!':
				case '#':
				case '$':
				case '%':
				case '&':
				case '\'':
				case '(':
				case ')':
				case '*':
				case '+':
				case ',':
				case '-':
				case '.':
				case '/':
				case ':':
				case ';':
				case '<':
				case '=':
				case '>':
				case '?':
				case '@':
				case '[':
				case '\\':
				case ']':
				case '^':
				case '_':
				case '`':
				case '{':
				case '|':
				case '}':
				case '~':
					token.erase(n - 1, 1);

					--n;

					break;

				// transform to lower-case
				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
				case 'F':
				case 'G':
				case 'H':
				case 'I':
				case 'J':
				case 'K':
				case 'L':
				case 'M':
				case 'N':
				case 'O':
				case 'P':
				case 'Q':
				case 'R':
				case 'S':
				case 'T':
				case 'U':
				case 'V':
				case 'W':
				case 'X':
				case 'Y':
				case 'Z':
					token[n - 1] += toLowerCase;

					break;

				default:
					continue;
				}
			}
		}

		// check last character
		switch(token.back()) {
		// remove punctuation
		case '"':
		case '!':
		case '#':
		case '$':
		case '%':
		case '&':
		case '\'':
		case '(':
		case ')':
		case '*':
		case '+':
		case ',':
		case '-':
		case '.':
		case '/':
		case ':':
		case ';':
		case '<':
		case '=':
		case '>':
		case '?':
		case '@':
		case '[':
		case '\\':
		case ']':
		case '^':
		case '_':
		case '`':
		case '{':
		case '|':
		case '}':
		case '~':
			token.pop_back();

			break;

		// transform to lower-case
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
		case 'G':
		case 'H':
		case 'I':
		case 'J':
		case 'K':
		case 'L':
		case 'M':
		case 'N':
		case 'O':
		case 'P':
		case 'Q':
		case 'R':
		case 'S':
		case 'T':
		case 'U':
		case 'V':
		case 'W':
		case 'X':
		case 'Y':
		case 'Z':
			token.back() += toLowerCase;

			break;

		default:
			break;
		}

		// do not process short tokens any further
		if(token.length() < minLengthStrip1) {
			return;
		}

		// strip 'ge-' if word is long enough
		if(
				token.length() >= minLengthStrip2
				&& token[0] == 'g'
				&& token[1] == 'e'
		) {
			token.erase(0, 2);
		}

		// keep important characters sequences
		std::size_t ignore{};

		for(std::size_t n{1}; n < token.size(); ++n) {
			if(token[n - 1] == 'e' && token[n] == 'i') {
				token[n - 1] = '%';
				token[n] = '%';

				++n;
				++ignore;
			}
			else if(token[n - 1] == 'i' && token[n] == 'e') {
				token[n - 1] = '&';
				token[n] = '&';

				++n;
				++ignore;
			}
			else if(
					n + 1 < token.size()
					&& token[n - 1] == 's'
					&& token[n] == 'c'
					&& token[n + 1] == 'h'
			) {
				token[n - 1] = '$';
				token[n] = '$';
				token[n + 1] = '$';

				n += 2;
				ignore += 2;
			}
		}

		// mark double characters
		char last{};

		for(auto& c : token) {
			if(
					c == last
					&& c != '%'
					&& c != '&'
					&& c != '$'
			) {
				c = '*';
				last = 0;
			}
			else {
				last = c;
			}
		}

		while(token.size() - ignore >= minLengthStrip1) {
			const auto indexLast{token.size() - 1};

			if(token.size() - ignore >= minLengthStrip2) {
				switch(token[indexLast - 1]) {
				case 'e':
					switch(token[indexLast]) {
					case 'm':
					case 'r':
						token.pop_back();
						token.pop_back();

						continue;

					default:
						break;
					}

					break;

				case 'n':
					if(token[indexLast] == 'd') {
						// strip '-nd'
						token.pop_back();
						token.pop_back();

						continue;
					}

					break;

				default:
					break;
				}
			}

			// strip '-t', '-e', '-s', or '-n'
			switch(token[token.size() - 1]) {
			case 't':
			case 'e':
			case 's':
			case 'n':
				token.pop_back();

				continue;

			default:
				break;
			}

			break;
		}

		// undo substitutions
		last = 0;

		for(auto& c : token) {
			if(c == '*') {
				c = last;
			}
			else {
				last = c;
			}
		}

		for(std::size_t n{1}; n < token.size(); ++n) {
			switch(token[n - 1]) {
			case '%':
				token[n - 1] = 'e';
				token[n] = 'i';

				++n;

				continue;

			case '&':
				token[n - 1] = 'i';
				token[n] = 'e';

				++n;

				continue;

			case '$':
				token[n - 1] = 's';
				token[n] = 'c';
				token[n + 1] = 'h';

				n += 2;

				continue;

			default:
				continue;
			}
		}
	}

} /* namespace crawlservpp::Data::Stemmer */

#endif /* DATA_STEMMER_GERMAN_HPP_ */
