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
 * Bytes.hpp
 *
 * Helper functions for byte operations.
 *
 *  Created on: Feb 3, 2021
 *      Author: ans
 */

#ifndef HELPER_BYTES_HPP_
#define HELPER_BYTES_HPP_

#include <algorithm>	// std::copy, std::reverse
#include <array>		// std::array
#include <cctype>		// std::isprint
#include <cstddef>		// std::size_t
#include <cstdint>		// std::[u]int[8,16,32,54]_t
#include <stdexcept>	// std::invalid_argument
#include <string>		// std::to_string
#include <vector>		// std::vector

//! Namespace for global byte operation helper functions.
namespace crawlservpp::Helper::Bytes {

	// for convenience
	using Bytes = std::vector<std::uint8_t>;

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Index of the first byte.
	constexpr auto first{0};

	//! Index of the second byte.
	constexpr auto second{1};

	//! Index of the third byte.
	constexpr auto third{2};

	//! Index of the fourth byte.
	constexpr auto fourth{3};

	//! Index of the fifth byte.
	constexpr auto fifth{4};

	//! Index of the sixth byte.
	constexpr auto sixth{5};

	//! Index of the seventh byte.
	constexpr auto seventh{6};

	//! Index of the eighth byte.
	constexpr auto eighth{7};

	//! One byte in bits.
	constexpr auto oneByteBits{8};

	//! Two bytes in bits.
	constexpr auto twoBytesBits{16};

	//! Three bytes in bits.
	constexpr auto threeBytesBits{24};

	//! Four bytes in bits.
	constexpr auto fourBytesBits{32};

	//! Five bytes in bits.
	constexpr auto fiveBytesBits{40};

	//! Six bytes in bits.
	constexpr auto sixBytesBits{48};

	//! Seven bytes in bits.
	constexpr auto sevenBytesBits{56};

	//! Size of eight bytes.
	constexpr auto sizeEight{8};

	//! Size of four bytes.
	constexpr auto sizeFour{4};

	//! Size of two bytes.
	constexpr auto sizeTwo{2};

	///@}

	/*
	 * DECLARATION
	 */

	///@name Endianness
	///@{

	bool isBigEndian() noexcept;
	bool isFloatBigEndian() noexcept;

	///@}
	///@name Bytes-to-Number Conversion
	///@{

	std::uint64_t bytesToUInt64(const Bytes& bytes, std::size_t& pos);
	std::uint64_t bytesToUInt64(const Bytes& bytes, std::size_t& pos, std::size_t len);
	std::int64_t bytesToInt64(const Bytes& bytes, std::size_t& pos);
	std::int64_t bytesToInt64(const Bytes& bytes, std::size_t& pos, std::size_t len);
	std::uint32_t bytesToUInt32(const Bytes& bytes, std::size_t& pos);
	std::int32_t bytesToInt32(const Bytes& bytes, std::size_t& pos);
	std::uint16_t bytesToUInt16(const Bytes& bytes, std::size_t& pos);
	std::int16_t bytesToInt16(const Bytes& bytes, std::size_t& pos);
	double bytesToDouble(const Bytes& bytes, std::size_t& pos);

	///@}
	///@name Number-to-Bytes Conversion
	///@{

	std::array<std::uint8_t, sizeEight> uInt64ToBytes(std::uint64_t number);
	std::array<std::uint8_t, sizeEight> int64ToBytes(std::int64_t number);
	std::array<std::uint8_t, sizeFour> uInt32ToBytes(std::uint32_t number);
	std::array<std::uint8_t, sizeFour> int32ToBytes(std::int32_t number);
	std::array<std::uint8_t, sizeTwo> uInt16ToBytes(std::uint16_t number);
	std::array<std::uint8_t, sizeTwo> int16ToBytes(std::int16_t number);
	std::array<std::uint8_t, sizeEight> doubleToBytes(double number);

	///@}
	///@name String Representation
	///@{

	std::string byteToHexString(std::uint8_t byte);
	std::string charToString(char c);

	///@}

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * ENDIANNESS
	 */

	//! Returns whether the machine running this code uses big endianness.
	/*!
	 * \returns True, if the machine running this
	 *   code uses big endianness. False otherwise.
	 */
	inline bool isBigEndian() noexcept {
		const std::uint8_t one{1};

		return *(reinterpret_cast<const char *>(&one)) != 1;
	}

	//! Returns whether the machine running this code uses big endianness for floating-point numbers.
	/*!
	 * \returns True, if the machine running this
	 *   code uses big endianness for floating-point
	 *   numbers. False otherwise.
	 */
	inline bool isFloatBigEndian() noexcept {
		const auto minusOne{-1.F};

		return *(reinterpret_cast<const std::uint8_t *>(&minusOne)) > 0;
	}

	/*
	 * BYTES-TO-NUMBER CONVERSION
	 */

	//! Retrieve an unsigned 64-bit number from a vector of bytes.
	/*!
	 * \param bytes Constant reference to a vector
	 *   containing the bytes.
	 * \param pos Reference to the start position of
	 *   the number contained in the bytes.
	 *   The position will be updated after reading
	 *   the number from the bytes.
	 *
	 * \return The number as read from the bytes.
	 */
	inline std::uint64_t bytesToUInt64(const Bytes& bytes, std::size_t& pos) {
		std::array<std::uint8_t, sizeEight> numberBytes{
			bytes[pos + first], bytes[pos + second], bytes[pos + third], bytes[pos + fourth],
			bytes[pos + fifth], bytes[pos + sixth], bytes[pos + seventh], bytes[pos + eighth]
		};

		pos += sizeEight;

		if(Helper::Bytes::isBigEndian()) {
			std::reverse(numberBytes.begin(), numberBytes.end());
		}

		return static_cast<std::uint64_t>(numberBytes[eighth]) << sevenBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[seventh]) << sixBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[sixth]) << fiveBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[fifth]) << fourBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[fourth]) << threeBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[third]) << twoBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[second]) << oneByteBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[first]);
	}

	//! Retrieve an unsigned 64-bit number from a vector of bytes.
	/*!
	 * \param bytes Constant reference to a vector
	 *   containing the bytes.
	 * \param pos Reference to the start position of
	 *   the number contained in the bytes.
	 *   The position will be updated after reading
	 *   the number from the bytes.
	 * \param len The actual number of bytes used by
	 *   the number.
	 *
	 * \return The number as read from the bytes.
	 */
	inline std::uint64_t bytesToUInt64(const Bytes& bytes, std::size_t& pos, std::size_t len) {
		if(len > sizeEight) {
			throw std::invalid_argument(
					"Bytes::bytesToUInt64():"
					" Only numbers up to a length of eight bytes are supported (len="
					+ std::to_string(len)
					+ ")"
			);
		}

		std::array<std::uint8_t, sizeEight> numberBytes{};

		for(std::size_t n{}; n < len; ++n) {
			numberBytes.at(n) = bytes[pos + n];
		};

		pos += len;

		if(Helper::Bytes::isBigEndian()) {
			std::reverse(numberBytes.begin(), numberBytes.end());
		}

		return static_cast<std::uint64_t>(numberBytes[eighth]) << sevenBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[seventh]) << sixBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[sixth]) << fiveBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[fifth]) << fourBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[fourth]) << threeBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[third]) << twoBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[second]) << oneByteBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint64_t>(numberBytes[first]);
	}

	//! Retrieve a signed 64-bit number from a vector of bytes.
	/*!
	 * \param bytes Constant reference to a vector
	 *   containing the bytes.
	 * \param pos Reference to the start position of
	 *   the number contained in the bytes.
	 *   The position will be updated after reading
	 *   the number from the bytes.
	 * \param len The actual number of bytes used by
	 *   the number.
	 *
	 * \return The number as read from the bytes.
	 */
	inline std::int64_t bytesToInt64(const Bytes& bytes, std::size_t& pos, std::size_t len) {
		const std::uint64_t withoutSign{bytesToUInt64(bytes, pos, len)};

		return *reinterpret_cast<const std::int64_t *>(&withoutSign);
	}

	//! Retrieve a signed 64-bit number from a vector of bytes.
	/*!
	 * \param bytes Constant reference to a vector
	 *   containing the bytes.
	 * \param pos Reference to the start position of
	 *   the number contained in the bytes.
	 *   The position will be updated after reading
	 *   the number from the bytes.
	 *
	 * \return The number as read from the bytes.
	 */
	inline std::int64_t bytesToInt64(const Bytes& bytes, std::size_t& pos) {
		const std::uint64_t withoutSign{bytesToUInt64(bytes, pos)};

		return *reinterpret_cast<const std::int64_t *>(&withoutSign);
	}

	//! Retrieve an unsigned 32-bit number from a vector of bytes.
	/*!
	 * \param bytes Constant reference to a vector
	 *   containing the bytes.
	 * \param pos Reference to the start position of
	 *   the number contained in the bytes.
	 *   The position will be updated after reading
	 *   the number from the bytes.
	 *
	 * \return The number as read from the bytes.
	 */
	inline std::uint32_t bytesToUInt32(const Bytes& bytes, std::size_t& pos) {
		std::array<std::uint8_t, sizeFour> numberBytes{
			bytes[pos + first], bytes[pos + second], bytes[pos + third], bytes[pos + fourth]
		};

		pos += sizeFour;

		if(Helper::Bytes::isBigEndian()) {
			std::reverse(numberBytes.begin(), numberBytes.end());
		}

		return static_cast<std::uint32_t>(numberBytes[fourth]) << threeBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint32_t>(numberBytes[third]) << twoBytesBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint32_t>(numberBytes[second]) << oneByteBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint32_t>(numberBytes[first]);
	}

	//! Retrieve a signed 32-bit number from a vector of bytes.
	/*!
	 * \param bytes Constant reference to a vector
	 *   containing the bytes.
	 * \param pos Reference to the start position of
	 *   the number contained in the bytes.
	 *   The position will be updated after reading
	 *   the number from the bytes.
	 *
	 * \return The number as read from the bytes.
	 */
	inline std::int32_t bytesToInt32(const Bytes& bytes, std::size_t& pos) {
		const std::uint32_t withoutSign{bytesToUInt32(bytes, pos)};

		return *reinterpret_cast<const std::int32_t *>(&withoutSign);
	}

	//! Retrieve an unsigned 16-bit number from a vector of bytes.
	/*!
	 * \param bytes Constant reference to a vector
	 *   containing the bytes.
	 * \param pos Reference to the start position of
	 *   the number contained in the bytes.
	 *   The position will be updated after reading
	 *   the number from the bytes.
	 *
	 * \return The number as read from the bytes.
	 */
	inline std::uint16_t bytesToUInt16(const Bytes& bytes, std::size_t& pos) {
		std::array<std::uint8_t, sizeTwo> numberBytes{
			bytes[pos + first], bytes[pos + second]
		};

		pos += sizeTwo;

		if(Helper::Bytes::isBigEndian()) {
			std::reverse(numberBytes.begin(), numberBytes.end());
		}

		return static_cast<std::uint16_t>(numberBytes[second]) << oneByteBits //NOLINT(hicpp-signed-bitwise)
				| static_cast<std::uint16_t>(numberBytes[first]);
	}

	//! Retrieve a signed 16-bit number from a vector of bytes.
	/*!
	 * \param bytes Constant reference to a vector
	 *   containing the bytes.
	 * \param pos Reference to the start position of
	 *   the number contained in the bytes.
	 *   The position will be updated after reading
	 *   the number from the bytes.
	 *
	 * \return The number as read from the bytes.
	 */
	inline std::int16_t bytesToInt16(const Bytes& bytes, std::size_t& pos) {
		const std::uint16_t withoutSign{bytesToUInt16(bytes, pos)};

		return *reinterpret_cast<const std::int16_t *>(&withoutSign);
	}

	//! Retrieves a IEEE 754 double-precision binary floating-point number from a vector of bytes.
	/*!
	 * \param bytes Constant reference to a vector
	 *   containing the bytes.
	 * \param pos Reference to the start position of
	 *   the floating-point number contained in the
	 *   bytes. The position will be updated after
	 *   reading the floating-point number from the
	 *   bytes.
	 *
	 * \return The floating-point number as read from the bytes.
	 */
	inline double bytesToDouble(const Bytes& bytes, std::size_t& pos) {
		std::array<std::uint8_t, sizeEight> numberBytes{
			bytes[pos + eighth], bytes[pos + seventh], bytes[pos + sixth], bytes[pos + fifth],
			bytes[pos + fourth], bytes[pos + third], bytes[pos + second], bytes[pos + first]
		};

		pos += sizeEight;

		if(Helper::Bytes::isFloatBigEndian()) {
			std::reverse(numberBytes.begin(), numberBytes.end());
		}

		return reinterpret_cast<const double&>(*(numberBytes.data()));
	}

	/*
	 * NUMBER-TO-BYTES CONVERSION
	 */

	//! Converts an unsigned 64-bit number to an array of eight bytes.
	/*!
	 * \param number The number to convert to bytes.
	 *
	 * \returns An array containing the resulting
	 *   eight bytes.
	 */
	inline std::array<std::uint8_t, sizeEight> uInt64ToBytes(std::uint64_t number) {
		std::array<std::uint8_t, sizeEight> result{};

		reinterpret_cast<std::uint64_t&>(*result.data()) = number;

		if(Helper::Bytes::isBigEndian()) {
			std::reverse(result.begin(), result.end());
		}

		return result;
	}

	//! Converts a signed 64-bit number to an array of eight bytes.
	/*!
	 * \param number The number to convert to bytes.
	 *
	 * \returns An array containing the resulting
	 *   eight bytes.
	 */
	inline std::array<std::uint8_t, sizeEight> int64ToBytes(std::int64_t number) {
		std::array<std::uint8_t, sizeEight> result{};

		reinterpret_cast<std::int64_t&>(*result.data()) = number;

		if(Helper::Bytes::isBigEndian()) {
			std::reverse(result.begin(), result.end());
		}

		return result;
	}

	//! Converts an unsigned 32-bit number to an array of four bytes.
	/*!
	 * \param number The number to convert to bytes.
	 *
	 * \returns An array containing the resulting
	 *   four bytes.
	 */
	inline std::array<std::uint8_t, sizeFour> uInt32ToBytes(std::uint32_t number) {
		std::array<std::uint8_t, sizeFour> result{};

		reinterpret_cast<std::uint32_t&>(*result.data()) = number;

		if(Helper::Bytes::isBigEndian()) {
			std::reverse(result.begin(), result.end());
		}

		return result;
	}

	//! Converts an signed 32-bit number to an array of four bytes.
	/*!
	 * \param number The number to convert to bytes.
	 *
	 * \returns An array containing the resulting
	 *   four bytes.
	 */
	inline std::array<std::uint8_t, sizeFour> int32ToBytes(std::int32_t number) {
		std::array<std::uint8_t, sizeFour> result{};

		reinterpret_cast<std::int32_t&>(*result.data()) = number;

		if(Helper::Bytes::isBigEndian()) {
			std::reverse(result.begin(), result.end());
		}

		return result;
	}

	//! Converts an unsigned 16-bit number to an array of two bytes.
	/*!
	 * \param number The number to convert to bytes.
	 *
	 * \returns An array containing the resulting
	 *   two bytes.
	 */
	inline std::array<std::uint8_t, sizeTwo> uInt16ToBytes(std::uint16_t number) {
		std::array<std::uint8_t, sizeTwo> result{};

		reinterpret_cast<std::uint16_t&>(*result.data()) = number;

		if(Helper::Bytes::isBigEndian()) {
			std::reverse(result.begin(), result.end());
		}

		return result;
	}

	//! Converts an signed 16-bit number to an array of two bytes.
	/*!
	 * \param number The number to convert to bytes.
	 *
	 * \returns An array containing the resulting
	 *   two bytes.
	 */
	inline std::array<std::uint8_t, sizeTwo> int16ToBytes(std::int16_t number) {
		std::array<std::uint8_t, sizeTwo> result{};

		reinterpret_cast<std::int16_t&>(*result.data()) = number;

		if(Helper::Bytes::isBigEndian()) {
			std::reverse(result.begin(), result.end());
		}

		return result;
	}

	//! Converts a floating-point number with double precision to an array of four bytes.
	/*!
	 * \param number The floating-point number to
	 *   convert to bytes.
	 *
	 * \returns An array containing the resulting
	 *   eight bytes.
	 */
	inline std::array<std::uint8_t, sizeEight> doubleToBytes(double number) {
		std::array<std::uint8_t, sizeEight> result{};

		reinterpret_cast<double&>(*result.data()) = number;

		if(!Helper::Bytes::isFloatBigEndian()) {
			std::reverse(result.begin(), result.end());
		}

		return result;
	}

	/*
	 * STRING REPRESENTATION
	 */

	//! Converts a byte to a string containing the byte in hexadecimal format.
	/*!
	 * \param byte The byte to convert into a string.
	 *
	 * \returns The string containing a hexadecimal
	 *   representation of the byte in the format
	 *   \c 0xHH where \HH is the hexadecimal value
	 *   of the byte.
	 */
	inline std::string byteToHexString(std::uint8_t byte) {
		constexpr std::array hexChars{
			'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
		};

		constexpr auto resultSize{4};
		constexpr auto hex240{0xF0};
		constexpr auto hex15{0xF};
		constexpr auto hexShift{4};

		std::string result{"0x"};

		result.reserve(resultSize);

		result.push_back(hexChars.at(((byte & hex240) >> hexShift)));	//NOLINT(hicpp-signed-bitwise)
		result.push_back(hexChars.at(byte & hex15));					//NOLINT(hicpp-signed-bitwise)

		return result;
	}

	//! Converts a character to a string.
	/*!
	 * If printable, the character will be simply
	 *  converted into a string. If escapable,
	 *  the string representation of its C escape
	 *  sequence will be returned. Otherwise, its
	 *  hexadecimal representation will be returned.
	 *
	 * The resulting string will be between one and
	 *  four characters long.
	 *
	 * \param c The character to convert.
	 *
	 * \returns The resulting string.
	 *
	 * \sa byteToHexString
	 */
	inline std::string charToString(char c) {
		switch(c) {
		case '\0':
			return "\\0";

		case '\a':
			return "\\a";

		case '\f':
			return "\\f";

		case '\n':
			return "\\n";

		case '\r':
			return "\\r";

		case '\v':
			return "\\v";

		case '\t':
			return "\\t";

		default:
			if(std::isprint(c) != 0) {
				return std::string(c, 1);
			}

			break;
		}

		return byteToHexString(static_cast<std::uint8_t>(c));
	}

} /* namespace crawlservpp::Helper::Bytes */

#endif /* HELPER_BYTES_HPP_ */
