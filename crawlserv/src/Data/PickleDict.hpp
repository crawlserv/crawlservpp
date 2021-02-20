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
 * PickleDict.hpp
 *
 * Simple dictionary that supports extracting data
 *  from and writing data to Python pickles.
 *
 * Only pickles with protocol version 4 or higher
 *  are supported.
 *
 * NOTE: Does not actually run Python pickle op-codes,
 *   only extracts data from, or writes its data to a
 *   simple Python pickle.
 *
 *  Created on: Feb 3, 2021
 *      Author: ans
 */

#ifndef HELPER_PICKLEDICT_HPP_
#define HELPER_PICKLEDICT_HPP_

#include "../Helper/Bytes.hpp"
#include "../Main/Exception.hpp"

#include <array>			// std::array
#include <cstddef>			// std::size_t
#include <cstdint>			// std::int8_t, std::int64_t, std::uint[8|16|32|62]_t
#include <cstdlib>			// std::strtof, std::strtoll
#include <iterator>			// std::make_move_iterator
#include <limits>			// std::numeric_limits
#include <optional>			// std::optional
#include <string>			// std::to_string
#include <unordered_map>	// std::unordered_map
#include <vector>			// std::vector

#include <iostream>

namespace crawlservpp::Data {

	// for convenience
	using Bytes = std::vector<std::uint8_t>;

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! One byte.
	inline constexpr auto pickleOneByte{1};

	//! Two bytes.
	inline constexpr auto pickleTwoBytes{2};

	//! Four bytes.
	inline constexpr auto pickleFourBytes{4};

	//! Eight bytes.
	inline constexpr auto pickleEightBytes{8};

	//! Nine bytes (eight bytes and an op-code).
	inline constexpr auto pickleNineBytes{9};

	//! The minimum size of a Python pickle to extract a frame.
	inline constexpr auto pickleMinSize{11};

	//! The protocol version of Python pickles used.
	inline constexpr auto pickleProtocolVersion{4};

	//! The position of the protocol byte in a Python pickle.
	inline constexpr auto pickleProtoByte{0};

	//! The position of the version byte in a Python pickle.
	inline constexpr auto pickleVersionByte{1};

	//! The size of the Python pickle header, in bytes.
	inline constexpr auto pickleHeadSize{2};

	//! The minimum size of a Python pickle frame.
	inline constexpr auto pickleMinFrameSize{9};

	//! Maximum number in unsigned one-byte number.
	inline constexpr std::uint8_t pickleMaxUOneByteNumber{255};

	//! Maximum number in unsigned two-byte number.
	inline constexpr std::uint16_t pickleMaxUTwoByteNumber{65535};

	//! Maximum number in unsigned four-byte number.
	inline constexpr std::uint32_t pickleMaxUFourByteNumber{4294967295};

	//! The base used for converting strings to numbers.
	inline constexpr auto pickleBase{10};

	///@}
	/*
	 * CLASS DECLARATION
	 */

	//! Simple Python pickle dictionary.
	/*!
	 * Only pickles with protocol version 4 or higher
	 *  are supported.
	 *
	 * \note Does not actually run Python pickle op-codes,
	 *   only extracts data from, or writes its data to a
	 *   simple Python pickle.
	 *
	 * \warning Only \c SHORT_BINSTRING and
	 *   \c SHORT_BINUNICODE, i.e. strings up to a length
	 *   of 255 are supported as key names. They need to
	 *   be separated by \c MEMOIZE from their respective
	 *   values in the Python pickle.
	 */
	class PickleDict {
	public:
		///@name Construction
		///@{

		//! Default constructor.
		PickleDict() = default;

		explicit PickleDict(const Bytes& data);

		///@}
		///@name Getters
		///@{

		[[nodiscard]] std::optional<std::int64_t> getNumber(const std::string& key) const;
		[[nodiscard]] std::optional<double> getFloat(const std::string& key) const;
		[[nodiscard]] std::optional<std::string> getString(const std::string& key) const;

		///@}
		///@name Setters
		///@{

		void setNumber(
				const std::string& key,
				std::int64_t value
		);
		void setFloat(
				const std::string& key,
				double value
		);
		void setString(
				const std::string& key,
				const std::string& value
		);

		///@}
		///@name Reading and Writing
		///@{

		void readFrom(const Bytes& data);
		void writeTo(Bytes& dataTo) const;

		///@}

		//! Class for Python pickle exceptions.
		MAIN_EXCEPTION_CLASS();

	private:
		// dictionary
		std::unordered_map<std::string, std::string> strings;
		std::unordered_map<std::string, std::int64_t> numbers;
		std::unordered_map<std::string, double> floats;

		/*
		 * INTERNAL DATA STRUCTURES
		 */

		// Python pickle op-codes
		//  Source: https://github.com/python/cpython/blob/master/Modules/_pickle.c
		enum OpCode {
			MARK            = '(',
			STOP            = '.',
			POP             = '0',
			POP_MARK        = '1',
			DUP             = '2',
			FLOAT           = 'F',
			INT             = 'I',
			BININT          = 'J',
			BININT1         = 'K',
			LONG            = 'L',
			BININT2         = 'M',
			NONE            = 'N',
			PERSID          = 'P',
			BINPERSID       = 'Q',
			REDUCE          = 'R',
			STRING          = 'S',
			BINSTRING       = 'T',
			SHORT_BINSTRING = 'U',
			UNICODE         = 'V',
			BINUNICODE      = 'X',
			APPEND          = 'a',
			BUILD           = 'b',
			GLOBAL          = 'c',
			DICT            = 'd',
			EMPTY_DICT      = '}',
			APPENDS         = 'e',
			GET             = 'g',
			BINGET          = 'h',
			INST            = 'i',
			LONG_BINGET     = 'j',
			LIST            = 'l',
			EMPTY_LIST      = ']',
			OBJ             = 'o',
			PUT             = 'p',
			BINPUT          = 'q',
			LONG_BINPUT     = 'r',
			SETITEM         = 's',
			TUPLE           = 't',
			EMPTY_TUPLE     = ')',
			SETITEMS        = 'u',
			BINFLOAT        = 'G',

			/* Protocol 2. */
			PROTO       = '\x80',
			NEWOBJ      = '\x81',
			EXT1        = '\x82',
			EXT2        = '\x83',
			EXT4        = '\x84',
			TUPLE1      = '\x85',
			TUPLE2      = '\x86',
			TUPLE3      = '\x87',
			NEWTRUE     = '\x88',
			NEWFALSE    = '\x89',
			LONG1       = '\x8a',
			LONG4       = '\x8b',

			/* Protocol 3 (Python 3.x) */
			BINBYTES       = 'B',
			SHORT_BINBYTES = 'C',

			/* Protocol 4 */
			SHORT_BINUNICODE = '\x8c',
			BINUNICODE8      = '\x8d',
			BINBYTES8        = '\x8e',
			EMPTY_SET        = '\x8f',
			ADDITEMS         = '\x90',
			FROZENSET        = '\x91',
			NEWOBJ_EX        = '\x92',
			STACK_GLOBAL     = '\x93',
			MEMOIZE          = '\x94',
			FRAME            = '\x95',

			/* Protocol 5 */
			BYTEARRAY8       = '\x96',
			NEXT_BUFFER      = '\x97',
			READONLY_BUFFER  = '\x98',

			/* force signedness */
			SIGNED = -1
		};

		// Python pickle frame
		struct Frame {
			std::uint8_t opCode{};

			Bytes data;
		};

		/*
		 * PRIVATE MEMBER FUNCTIONS
		 */

		// internal helper functions
		void readValue(
				const Bytes& data,
				std::size_t& pos,
				const std::string& key
		);

		// internal static helper functions
		static bool readKey(
				const Bytes& data,
				std::size_t& pos,
				std::string& keyTo
		);
		static Bytes unpack(const Bytes& data);
		static bool extractNextFrame(
				const Bytes& bytes,
				std::size_t& pos,
				Frame& frameTo
		);
		static Bytes unpackFrame(const Frame& frame);

		static bool skipMemoize(
				const Bytes& data,
				std::size_t& pos
		);

		static void checkLength(
				std::size_t dataLength,
				std::size_t currentEnd
		);
		static std::size_t readValueLength(
				const Bytes& data,
				std::size_t& pos,
				std::size_t numBytes
		);
		static std::size_t getLengthByTermination(
				const Bytes& data,
				std::size_t pos,
				char terminatingCharacter
		);
		static std::string getString(
				const Bytes& data,
				std::size_t& pos,
				std::size_t length
		);

		static void writeHead(Bytes& to);
		static void writeFrame(const Bytes& frameBytes, Bytes& to, bool isLast);
		static void writeDictHead(Bytes& to);
		static void writeDictTail(Bytes& to);

		static void writeNumberEntry(
				const std::pair<std::string, std::int64_t>& entry,
				Bytes& to
		);
		static void writeFloatEntry(
				const std::pair<std::string, double>& entry,
				Bytes& to
		);
		static void writeStringEntry(
				const std::pair<std::string, std::string>& entry,
				Bytes& to
		);

		static void writeKey(const std::string& key, Bytes& to);

		static void writeBinInt1(std::uint8_t value, Bytes& to);
		static void writeBinInt2(std::uint16_t value, Bytes& to);
		static void writeLong1(std::int64_t value, Bytes& to);
		static void writeBinFloat(double value, Bytes& to);
		static void writeShortBinUnicode(const std::string& value, Bytes& to);
		static void writeBinUnicode(const std::string& value, Bytes& to);
		static void writeBinUnicode8(const std::string& value, Bytes& to);

		// add bytes from any (iterable) container
		template<typename T> static void writeBytes(const T& bytes, Bytes& to) {
			to.reserve(to.size() + bytes.size());

			for(const auto byte : bytes) {
				to.emplace_back(byte);
			}
		}

		// check whether number is in range
		template<typename T> [[nodiscard]] static bool inRange(std::int64_t number) {
			return number >= std::numeric_limits<T>::min()
					&& number <= std::numeric_limits<T>::max();
		}
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * CONSTRUCTION
	 */

	//! Constructor reading the bytes of a Python pickle.
	/*!
	 * \param data The bytes contained in a pickle.
	 */
	inline PickleDict::PickleDict(const Bytes& data) {
		this->readFrom(data);
	}

	/*
	 * GETTERS
	 */

	//! Gets a number from the dictionary, if avaible.
	/*!
	 * \param key The key of the dictionary entry
	 *   containing the number.
	 *
	 * \returns An optional containing the number, if it
	 *   has been found in the dictionary.
	 */
	inline std::optional<std::int64_t> PickleDict::getNumber(const std::string& key) const {
		std::optional<std::int64_t> result;

		const auto it{this->numbers.find(key)};

		if(it != this->numbers.end()) {
			result = it->second;
		}

		return result;
	}

	//! Gets a floating-point number from the dictionary, if avaible.
	/*!
	 * \param key The key of the dictionary entry
	 *   containing the floating-point number.
	 *
	 * \returns An optional containing the floating-point
	 *   number, if it has been found in the dictionary.
	 */
	inline std::optional<double> PickleDict::getFloat(const std::string& key) const {
		std::optional<double> result;

		const auto it{this->floats.find(key)};

		if(it != this->floats.end()) {
			result = it->second;
		}

		return result;
	}

	//! Gets a string from the dictionary, if avaible.
	/*!
	 * \param key The key of the dictionary entry
	 *   containing the string.
	 *
	 * \returns An optional containing the string, if it
	 *   has been found in the dictionary.
	 */
	inline std::optional<std::string> PickleDict::getString(const std::string& key) const {
		std::optional<std::string> result;

		const auto it{this->strings.find(key)};

		if(it != this->strings.end()) {
			result = it->second;
		}

		return result;
	}

	/*
	 * SETTERS
	 */

	//! Adds or overwrite a number in the dictionary.
	/*!
	 * \param key The key of the value.
	 * \param value The corresponding value.
	 */
	inline void PickleDict::setNumber(const std::string& key, std::int64_t value) {
		if(key.empty()) {
			return;
		}

		this->numbers[key] = value;
	}

	//! Adds or overwrites a floating-point number in the dictionary
	/*!
	 * \param key The key of the value.
	 * \param value The corresponding value.
	 */
	inline void PickleDict::setFloat(const std::string& key, double value) {
		if(key.empty()) {
			return;
		}

		this->floats[key] = value;
	}

	//! Add or overwrites a string in the dictionary.
	/*!
	 * \param key The key of the value.
	 * \param value The corresponding value.
	 */
	inline void PickleDict::setString(const std::string& key, const std::string& value) {
		if(key.empty()) {
			return;
		}

		this->strings[key] = value;
	}

	/*
	 * READER
	 */

	//! Creates a simple dictionary from Python pickle data.
	/*!
	 * Only Python pickles with protocol version 4 or
	 *  higher are supported.
	 *
	 * \note Does not actually run Python pickle op-codes,
	 *   only extracts data from, or writes its data to a
	 *   simple Python pickle.
	 *
	 * \param data Constant reference to a vector
	 *   containing the bytes of the Python pickle.
	 *
	 * \throws PickleDict::Exception if the data could
	 *   not be read correctly.
	 *
	 * \warning Only \c SHORT_BINSTRING and
	 *   \c SHORT_BINUNICODE, i.e. strings up to a length
	 *   of 255 are supported as key names. They need to
	 *   be separated by \c MEMOIZE from their respective
	 *   values in the Python pickle.
	 */
	inline void PickleDict::readFrom(const Bytes& data) {
		// unpack frames
		const auto unpackedData{
			PickleDict::unpack(data)
		};

		std::size_t pos{};

		while(pos < unpackedData.size()) {
			// extract keys and values, skip everything else
			std::string key;

			if(
					PickleDict::readKey(unpackedData, pos, key)
					&& PickleDict::skipMemoize(unpackedData, pos)
			) {
				this->readValue(unpackedData, pos, key);
			}
			else {
				/*
				 * skip other values so that they are
				 *  not mistaken for op-codes
				 *
				 */
				this->readValue(unpackedData, pos, "");
			}
		}
	}

	//! Writes dictionary to Python pickle data.
	/*!
	 * Python pickle protocol version 4 will be used and
	 *  the data will be written to one single frame.
	 *
	 * \param dataTo Reference to a vector which will
	 *   contain the bytes of the data after writing.
	 *   The vector will be cleared before writing begins
	 *   (although no memory will be freed).
	 */
	inline void PickleDict::writeTo(Bytes& dataTo) const {
		dataTo.clear();

		// write frame
		std::vector<std::uint8_t> frame;

		PickleDict::writeDictHead(frame);

		for(const auto& entry : this->numbers) {
			PickleDict::writeNumberEntry(entry, frame);
		}

		for(const auto& entry : this->floats) {
			PickleDict::writeFloatEntry(entry, frame);
		}

		for(const auto& entry : this->strings) {
			PickleDict::writeStringEntry(entry, frame);
		}

		PickleDict::writeDictTail(frame);

		// write whole Python pickle
		PickleDict::writeHead(dataTo);
		PickleDict::writeFrame(frame, dataTo, true);
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// read a value from the current position in the data, or none at all
	inline void PickleDict::readValue(
			const Bytes& data,
			std::size_t& pos,
			const std::string& key
	) {
		std::size_t valueLength{};
		std::string s1;
		std::string s2;

		// check for end of data
		if(pos >= data.size()) {
			throw Exception(
					"SimpleDict::readValue():"
					" Unexpected end of data"
					" (invalid position)"
			);
		}
		if(pos == data.size() - 1) {
			if(data[pos] == static_cast<std::uint8_t>(OpCode::STOP)) {
				/* reached valid end of pickle */
				++pos;

				return;
			}

			throw Exception(
					"SimpleDict::readValue():"
					" Unexpected end of data"
					" (no STOP at the end)"
			);
		}

		// seek ahead
		++pos;

		switch(static_cast<int8_t>(data[pos - 1])) {
		/*
		 * SKIP
		 */
		case OpCode::ADDITEMS:
		case OpCode::APPEND:
		case OpCode::APPENDS:
		case OpCode::BINPERSID:
		case OpCode::BUILD:
		case OpCode::DICT:
		case OpCode::DUP:
		case OpCode::EMPTY_DICT:
		case OpCode::EMPTY_LIST:
		case OpCode::EMPTY_SET:
		case OpCode::EMPTY_TUPLE:
		case OpCode::FROZENSET:
		case OpCode::LIST:
		case OpCode::MARK:
		case OpCode::MEMOIZE:
		case OpCode::NEWOBJ:
		case OpCode::NEWOBJ_EX:
		case OpCode::NEXT_BUFFER:
		case OpCode::OBJ:
		case OpCode::POP:
		case OpCode::POP_MARK:
		case OpCode::READONLY_BUFFER:
		case OpCode::REDUCE:
		case OpCode::SETITEM:
		case OpCode::SETITEMS:
		case OpCode::STACK_GLOBAL:
		case OpCode::TUPLE:
		case OpCode::TUPLE1:
		case OpCode::TUPLE2:
		case OpCode::TUPLE3:
			// skip without argument
			break;

		case OpCode::EXT1:
			// skip one-byte argument
			pos += pickleOneByte;

			break;

		case OpCode::EXT2:
			// skip two-byte argument
			pos += pickleTwoBytes;

			break;

		case OpCode::EXT4:
			// skip four-byte argument
			pos += pickleFourBytes;

			break;

		/*
		 * GET NUMBER
		 */
		case OpCode::NEWFALSE:
		case OpCode::NONE:
			// add zero
			this->setNumber(key, 0);

			++pos;

			break;

		case OpCode::NEWTRUE:
			// add one
			this->setNumber(key, 1);

			++pos;

			break;

		case OpCode::BINGET:
		case OpCode::BININT1:
		case OpCode::BINPUT:
			// get one-byte unsigned integer
			this->setNumber(key, static_cast<std::uint8_t>(data[pos]));

			++pos;

			break;

		case OpCode::BININT2:
			// get two-byte unsigned integer
			PickleDict::checkLength(data.size(), pos + pickleTwoBytes);

			this->setNumber(key, Helper::Bytes::bytesToUInt16(data, pos));

			break;

		case OpCode::LONG_BINGET:
		case OpCode::LONG_BINPUT:
			// get four-byte unsigned integer
			PickleDict::checkLength(data.size(), pos + pickleFourBytes);

			this->setNumber(key, Helper::Bytes::bytesToUInt32(data, pos));

			break;

		case OpCode::LONG1:
			// read one-byte length and corresponding integer
			valueLength = PickleDict::readValueLength(data, pos, pickleOneByte);

			switch(valueLength) {
			case pickleOneByte:
				this->setNumber(key, static_cast<std::int8_t>(data[pos]));

				++pos;

				break;

			case pickleTwoBytes:
				this->setNumber(key, Helper::Bytes::bytesToInt16(data, pos));

				break;

			case pickleFourBytes:
				this->setNumber(key, Helper::Bytes::bytesToInt32(data, pos));

				break;

			case pickleEightBytes:
				this->setNumber(key, Helper::Bytes::bytesToInt64(data, pos));

				break;

			default:
				if(valueLength > pickleEightBytes) {
					throw Exception(
							"Pickle::readValue(): Value lengths consisting of "
							+ std::to_string(valueLength)
							+ " bytes are not supported"
					);
				}

				this->setNumber(key, Helper::Bytes::bytesToInt64(data, pos, valueLength));
			}

			break;

		case OpCode::BININT:
		case OpCode::LONG4:
			// get four-byte signed integer
			PickleDict::checkLength(data.size(), pos + pickleFourBytes);

			this->setNumber(key, Helper::Bytes::bytesToInt32(data, pos));

			break;

		case OpCode::INT:
		case OpCode::LONG:
			// get number from newline-terminated string
			valueLength = PickleDict::getLengthByTermination(data, pos, '\n');

			s1 = PickleDict::getString(data, pos, valueLength);

			++pos; // jump past newline

			this->setNumber(key, std::strtoll(s1.c_str(), nullptr, pickleBase));

			break;

		/*
		 * GET FLOATING-POINT NUMBER
		 */
		case OpCode::BINFLOAT:
			// get eight-byte floating-point number
			PickleDict::checkLength(data.size(), pos + pickleEightBytes);

			this->setFloat(key, Helper::Bytes::bytesToDouble(data, pos));

			break;

		case OpCode::FLOAT:
			// get floating-point number from from newline-terminated string
			valueLength = PickleDict::getLengthByTermination(data, pos, '\n');

			s1 = PickleDict::getString(data, pos, valueLength);

			++pos; // jump past newline

			this->setFloat(key, std::strtod(s1.c_str(), nullptr));

			break;

		/*
		 * GET STRING (OR BYTES)
		 */
		case OpCode::SHORT_BINBYTES:
		case OpCode::SHORT_BINSTRING:
		case OpCode::SHORT_BINUNICODE:
			// read one-byte length and corresponding string
			valueLength = PickleDict::readValueLength(data, pos, pickleOneByte);

			this->setString(key, PickleDict::getString(data, pos, valueLength));

			break;

		case OpCode::BINBYTES:
		case OpCode::BINSTRING:
		case OpCode::BINUNICODE:
			// read four-bytes length and corresponding string
			valueLength = PickleDict::readValueLength(data, pos, pickleFourBytes);

			this->setString(key, PickleDict::getString(data, pos, valueLength));

			break;

		case OpCode::BINBYTES8:
		case OpCode::BINUNICODE8:
		case OpCode::BYTEARRAY8:
			// read eight-bytes length and corresponding string
			valueLength = PickleDict::readValueLength(data, pos, pickleEightBytes);

			this->setString(key, PickleDict::getString(data, pos, valueLength));

			break;

		case OpCode::GET:
		case OpCode::PERSID:
		case OpCode::PUT:
		case OpCode::STRING:
		case OpCode::UNICODE:
			// read string terminated by newline
			valueLength = PickleDict::getLengthByTermination(data, pos, '\n');

			this->setString(key, PickleDict::getString(data, pos, valueLength));

			++pos; // jump past newline

			break;

		case OpCode::GLOBAL:
		case OpCode::INST:
			// read two strings terminated by newlines
			valueLength = PickleDict::getLengthByTermination(data, pos, '\n');

			s1 = PickleDict::getString(data, pos, valueLength);

			++pos; // jump past first newline

			valueLength = PickleDict::getLengthByTermination(data, pos, '\n');

			s2 = PickleDict::getString(data, pos, valueLength);

			++pos; // jump past second newline

			// combine strings
			this->setString(key, s1 + "." + s2);

			break;

		/*
		 * ERRORS
		 */
		case OpCode::FRAME:
			throw Exception(
					"SimpleDict::ReadValue():"
					" Unexpected frame still found after unpacking"
			);

		case OpCode::STOP:
			throw Exception(
					"SimpleDict::ReadValue():"
					" Unexpected 'STOP' before the end of the data"
			);

		case OpCode::PROTO:
			throw Exception(
					"SimpleDict::ReadValue():"
					" Unexpected 'PROTO' after the beginning of the data"
			);

		default:
			throw Exception(
					"SimpleDict::ReadValue():"
					" Unknown Python pickle op-code encountered"
			);
		}
	}

	/*
	 * INTERNAL STATIC HELPER FUNCTIONS (private)
	 */

	// read a key at the current position in the data, return whether a key was read
	inline bool PickleDict::readKey(
			const Bytes& data,
			std::size_t& pos,
			std::string& keyTo
	) {
		// check current op-code for pushing a short string
		if(
				data[pos] == static_cast<std::uint8_t>(OpCode::SHORT_BINSTRING)
				|| data[pos] == static_cast<std::uint8_t>(OpCode::SHORT_BINUNICODE)
		) {
			// (= 1 byte)
			++pos;

			// read key length
			const auto keyLength{data[pos]};

			// (= 1 byte)
			++pos;

			// check key length
			const auto keyEnd{pos + keyLength};

			if(keyEnd > data.size()) {
				throw Exception(
						"SimpleDict::readKey():"
						" Unexpected end of data (expected >"
						+ std::to_string(keyEnd - data.size())
						+ " bytes more)"
				);
			}

			// clear target and reserve memory
			keyTo.clear();
			keyTo.reserve(keyLength);

			// read key
			for(; pos < keyEnd; ++pos) {
				keyTo.push_back(static_cast<char>(data[pos]));
			}

			return true;
		}

		return false;
	}

	// unpack all frames from a Python pickle  with protocol version 4
	inline Bytes PickleDict::unpack(const Bytes& data) {
		Bytes unpacked;
		std::size_t pos{};
		Frame frame;

		while(PickleDict::extractNextFrame(data, pos, frame)) {
			auto frameData{PickleDict::unpackFrame(frame)};

			unpacked.insert(
					unpacked.end(),
					std::make_move_iterator(frameData.begin()),
					std::make_move_iterator(frameData.end())
			);
		}

		return unpacked;
	}

	// extract the next frame from a Python pickle with protocol version 4
	inline bool PickleDict::extractNextFrame(
			const Bytes& bytes,
			std::size_t& pos,
			Frame& frameTo
	) {
		if(pos == 0) {
			// check format and version of the Python pickle
			if(bytes.size() < pickleMinSize) {
				throw Exception(
						"Pickle::extractFirstFrame():"
						" No Python pickle found (only "
						+ std::to_string(bytes.size())
						+ " bytes)"
				);
			}

			if(bytes[pickleProtoByte] != static_cast<std::uint8_t>(OpCode::PROTO)) {
				throw Exception(
						"Pickle::extractFirstFrame():"
						" No Python pickle found (invalid first byte: "
						+ Helper::Bytes::byteToHexString(bytes[pickleProtoByte])
						+ " != "
						+ Helper::Bytes::byteToHexString(OpCode::PROTO)
						+ ")"
				);
			}

			if(bytes[pickleVersionByte] < pickleProtocolVersion) {
				throw Exception(
						"Pickle::extractFirstFrame():"
						" Python pickle of unsupported version ("
						+ std::to_string(bytes[pickleVersionByte])
						+ " < "
						+ std::to_string(pickleProtocolVersion)
						+ ")"
				);
			}

			pos += pickleHeadSize;
		}

		if(pos == bytes.size()) {
			return false;
		}

		// check number of remaining bytes
		const auto remaining{bytes.size() - pos};

		if(remaining < pickleMinFrameSize) {
			throw Exception(
					"Pickle::extractFirstFrame():"
					" No frame found in Python pickle (only "
					+ std::to_string(remaining)
					+ " bytes left)"
			);
		}

		// get opcode and size
		frameTo.opCode = bytes[pos];

		++pos;

		auto size{Helper::Bytes::bytesToUInt64(bytes, pos)};
		const auto it{bytes.cbegin() + pos};

		pos += size;

		frameTo.data = Bytes(it, it + size);

		return true;
	}

	// unpack a frame
	inline Bytes PickleDict::unpackFrame(const Frame& frame) {
		if(frame.opCode == static_cast<std::uint8_t>(OpCode::FRAME)) {
			return frame.data;
		}

		Bytes complete;

		complete.reserve(frame.data.size() + 1);
		complete.push_back(frame.opCode);

		complete.insert(
				complete.end(),
				frame.data.begin(),
				frame.data.end()
		);

		return complete;
	}

	// optionally skip MEMOIZE command and return whether
	//  such a command was found at the given position
	inline bool PickleDict::skipMemoize(
			const Bytes& data,
			std::size_t& pos
	) {
		if(
				pos < data.size()
				&& data[pos] == static_cast<std::uint8_t>(OpCode::MEMOIZE)
		) {
			++pos;

			return true;
		}

		return false;
	}

	// check data length
	inline void PickleDict::checkLength(
			std::size_t dataLength,
			std::size_t currentEnd
	) {
		if(currentEnd > dataLength) {
			throw Exception(
					"Pickle::readValue(): Unexpected end of data (expected >"
					+ std::to_string(currentEnd - dataLength)
					+ " bytes more)"
			);
		}
	}

	// read length of succeeding value
	inline std::size_t PickleDict::readValueLength(
			const Bytes& data,
			std::size_t& pos,
			std::size_t numBytes
	) {
		PickleDict::checkLength(data.size(), pos + numBytes);

		std::size_t result{};

		switch(numBytes) {
		case pickleOneByte:
			result = data[pos];

			break;

		case pickleTwoBytes:
			result = Helper::Bytes::bytesToUInt16(data, pos);

			break;

		case pickleFourBytes:
			result = Helper::Bytes::bytesToUInt32(data, pos);

			break;

		case pickleEightBytes:
			result = Helper::Bytes::bytesToUInt64(data, pos);

			break;

		default:
			if(numBytes > pickleEightBytes) {
				throw Exception(
						"Pickle::readValue(): Value lengths consisting of "
						+ std::to_string(numBytes)
						+ " bytes are not supported"
				);
			}

			result = Helper::Bytes::bytesToUInt64(data, pos, numBytes);

			break;
		}

		pos += numBytes;

		PickleDict::checkLength(data.size(), pos + result);

		return result;
	}

	// determine the length of a string by its terminating character (does NOT change the current position)
	inline std::size_t PickleDict::getLengthByTermination(
			const Bytes& data,
			std::size_t pos,
			char terminatingCharacter
	) {
		for(std::size_t end{pos}; end < data.size(); ++end) {
			if(data[end] == static_cast<std::uint8_t>(terminatingCharacter)) {
				return end - pos;
			}
		}

		// no terminating character found
		throw Exception(
				"SimpleDict::getLengthByTermination():"
				" Could not find terminating character '"
				+ Helper::Bytes::charToString(terminatingCharacter)
				+ "' after position #"
				+ std::to_string(pos)
		);
	}

	// extract a string from the data
	inline std::string PickleDict::getString(
			const Bytes& data,
			std::size_t& pos,
			std::size_t length
	) {
		std::string result;
		const auto end{pos + length};

		result.reserve(length);

		for(; pos < end; ++pos) {
			result.push_back(static_cast<char>(data[pos]));
		}

		return result;
	}

	// write Python pickle data head
	inline void PickleDict::writeHead(Bytes& to) {
		to.push_back(static_cast<std::uint8_t>(OpCode::PROTO));
		to.push_back(pickleProtocolVersion);
	}

	// write Python pickle frame
	inline void PickleDict::writeFrame(const Bytes& frameBytes, Bytes& to, bool isLast) {
		// calculate frame size
		std::uint64_t frameSize{frameBytes.size()};

		if(isLast) {
			++frameSize;
		}

		// reserve memory
		to.reserve(to.size() + frameSize + pickleNineBytes);

		// write frame head (including its size)
		to.push_back(static_cast<std::uint8_t>(OpCode::FRAME));

		PickleDict::writeBytes(Helper::Bytes::uInt64ToBytes(frameSize), to);

		// write frame data
		to.insert(to.end(), frameBytes.begin(), frameBytes.end());

		// finish frame
		if(isLast) {
			to.push_back(static_cast<std::uint8_t>(OpCode::STOP));
		}
	}

	// write dictionary head
	inline void PickleDict::writeDictHead(Bytes& to) {
		to.push_back(static_cast<std::uint8_t>(OpCode::EMPTY_DICT));
		to.push_back(static_cast<std::uint8_t>(OpCode::MEMOIZE));
		to.push_back(static_cast<std::uint8_t>(OpCode::MARK));
	}

	// write dictionary tail
	inline void PickleDict::writeDictTail(Bytes& to) {
		to.push_back(static_cast<std::uint8_t>(OpCode::MEMOIZE));
		to.push_back(static_cast<std::uint8_t>(OpCode::SETITEMS));
	}

	// write dictionary entry containing a number
	inline void PickleDict::writeNumberEntry(
			const std::pair<std::string, std::int64_t>& entry,
			Bytes& to
	) {
		PickleDict::writeKey(entry.first, to);

		if(entry.second >= 0) {
			if(entry.second <= pickleMaxUOneByteNumber) {
				PickleDict::writeBinInt1(static_cast<std::uint8_t>(entry.second), to);

				return;
			}

			if(entry.second <= pickleMaxUTwoByteNumber) {
				PickleDict::writeBinInt2(static_cast<std::uint16_t>(entry.second), to);

				return;
			}
		}

		PickleDict::writeLong1(entry.second, to);
	}

	// write dictionary entry containing a floating-point number
	inline void PickleDict::writeFloatEntry(
			const std::pair<std::string, double>& entry,
			Bytes& to
	) {
		PickleDict::writeKey(entry.first, to);
		PickleDict::writeBinFloat(entry.second, to);
	}

	// write dictionary entry containing a string
	inline void PickleDict::writeStringEntry(
			const std::pair<std::string, std::string>& entry,
			Bytes& to
	) {
		PickleDict::writeKey(entry.first, to);

		if(entry.second.size() <= pickleMaxUOneByteNumber) {
			PickleDict::writeShortBinUnicode(entry.second, to);
		}
		else if(entry.second.size() <= pickleMaxUFourByteNumber) {
			PickleDict::writeBinUnicode(entry.second, to);
		}
		else {
			PickleDict::writeBinUnicode8(entry.second, to);
		}
	}

	// write dictionary key
	inline void PickleDict::writeKey(const std::string& key, Bytes& to) {
		PickleDict::writeShortBinUnicode(key, to);

		to.push_back(static_cast<std::uint8_t>(OpCode::MEMOIZE));
	}

	// write one-byte unsigned number
	inline void PickleDict::writeBinInt1(std::uint8_t value, Bytes& to) {
		to.push_back(static_cast<std::uint8_t>(OpCode::BININT1));
		to.push_back(value);
	}

	// write two-bytes unsigned number
	inline void PickleDict::writeBinInt2(std::uint16_t value, Bytes& to) {
		to.push_back(static_cast<std::uint8_t>(OpCode::BININT2));

		PickleDict::writeBytes(Helper::Bytes::uInt16ToBytes(value), to);
	}

	// write number of bytes and signed number
	inline void PickleDict::writeLong1(std::int64_t value, Bytes& to) {
		to.push_back(static_cast<std::uint8_t>(OpCode::LONG1));

		if(PickleDict::inRange<std::int8_t>(value)) {
			to.push_back(pickleOneByte);
			to.push_back(static_cast<std::int8_t>(value));
		}
		else if(PickleDict::inRange<std::int16_t>(value)) {
			to.push_back(pickleTwoBytes);

			PickleDict::writeBytes(
					Helper::Bytes::int16ToBytes(
							static_cast<std::int16_t>(value)
					),
					to
			);
		}
		else if(PickleDict::inRange<std::int32_t>(value)) {
			to.push_back(pickleFourBytes);

			PickleDict::writeBytes(
					Helper::Bytes::int32ToBytes(
							static_cast<std::int32_t>(value)
					),
					to
			);
		}
		else {
			to.push_back(pickleEightBytes);

			PickleDict::writeBytes(Helper::Bytes::int64ToBytes(value), to);
		}
	}

	// write floating-point number of eight bytes, i.e. with double precision
	inline void PickleDict::writeBinFloat(double value, Bytes& to) {
		to.push_back(static_cast<std::uint8_t>(OpCode::BINFLOAT));

		PickleDict::writeBytes(Helper::Bytes::doubleToBytes(value), to);
	}

	// write one-byte length and string
	inline void PickleDict::writeShortBinUnicode(const std::string& value, Bytes& to) {
		to.push_back(static_cast<std::uint8_t>(OpCode::SHORT_BINUNICODE));

		// use max. 255 bytes
		std::uint8_t length{};

		if(value.size() > pickleMaxUOneByteNumber) {
			length = pickleMaxUOneByteNumber;
		}
		else {
			length = static_cast<std::uint8_t>(value.size());
		}

		// reserve memory
		to.reserve(length + pickleOneByte);

		// write length
		to.push_back(length);

		// write string
		for(std::size_t index{}; index < length; ++index) {
			to.push_back(static_cast<std::uint8_t>(value[index]));
		}
	}

	// write four-byte length and string
	inline void PickleDict::writeBinUnicode(const std::string& value, Bytes& to) {
		to.push_back(static_cast<std::uint8_t>(OpCode::BINUNICODE));

		// use max. 4,294,967,295 bytes
		std::uint32_t length{};

		if(value.size() > pickleMaxUOneByteNumber) {
			length = pickleMaxUOneByteNumber;
		}
		else {
			length = static_cast<std::uint32_t>(value.size());
		}

		// reserve memory
		to.reserve(length + pickleFourBytes);

		// write length
		PickleDict::writeBytes(Helper::Bytes::uInt32ToBytes(length), to);

		// write string
		for(std::size_t index{}; index < length; ++index) {
			to.push_back(static_cast<std::uint8_t>(value[index]));
		}
	}

	// write eight-byte length and string
	inline void PickleDict::writeBinUnicode8(const std::string& value, Bytes& to) {
		to.push_back(static_cast<std::uint8_t>(OpCode::BINUNICODE8));

		// reserve memory
		to.reserve(value.size() + pickleEightBytes);

		// write length
		PickleDict::writeBytes(Helper::Bytes::uInt64ToBytes(value.size()), to);

		// write string
		for(std::size_t index{}; index < value.size(); ++index) {
			to.push_back(static_cast<std::uint8_t>(value[index]));
		}
	}

} /* namespace crawlservpp::Data */

#endif /* DATA_PICKLEDICT_HPP_ */
