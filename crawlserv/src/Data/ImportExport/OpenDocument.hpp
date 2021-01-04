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
 * OpenDocument.hpp
 *
 * Namespace for functions to import/export data from/to files in the OpenDocument format.
 *
 *  Created on: Jan 4, 2021
 *      Author: ans
 */

#ifndef DATA_IMPORTEXPORT_OPENDOCUMENT_HPP_
#define DATA_IMPORTEXPORT_OPENDOCUMENT_HPP_

#include "../Compression/Zip.hpp"

#include "../../Helper/Strings.hpp"

#include <clocale>	// std::setlocale
#include <cstdint>	// std::uint8_t
#include <string>	// std::stod, std::string, std::to_string
#include <utility>	// std::pair
#include <vector>	// std::vector

//! Namespace for importing and exporting OpenDocument spreadsheets.
namespace crawlservpp::Data::ImportExport::OpenDocument {

	/*
	 * CONSTANTS
	 */

	//! The number of spaces before a OpenDocument XML cell element.
	inline constexpr auto cellSpacing{5};

	//! The number of lines used for a OpenDocument XML cell element and its content.
	inline constexpr auto cellLines{3};

	//! The number of additional characters for a OpenDocument XML cell element and its content.
	inline constexpr auto cellConstChars{57};

	/*
	 * DECLARATION
	 */

	//! A pair of strings.
	using StringString = std::pair<std::string, std::string>;

	//! A vector of strings used as rows in a spreadsheet table.
	using TableRow = std::vector<std::string>;

	//! A vector of vectors of strings used as spreadsheet tables.
	using Table = std::vector<TableRow>;

	//! A pair containing the name and the content of a spreadsheet table.
	using NamedTable = std::pair<std::string, Table>;

	///@name Import and Export
	///@{

	std::string exportSpreadsheet(
			const std::vector<NamedTable>& tables,
			bool firstRowBold
	);

	///@}
	///@name Helper
	///@{

	std::string cell(std::uint8_t spacing, const std::string& raw, const std::string& style);

	///@}

	/*
	 * IMPLEMENTATION
	 */

	//! Exports tables as a OpenDocument spreadsheet.
	/*!
	 * \param tables Constant reference to
	 *   a vector containing the tables,
	 *   each consisting of a pair of its
	 *   name and a vector of rows, each
	 *   consisting of a vector of strings
	 *   representing the values in the row.
	 * \param firstRowBold Specifies whether
	 *   the first row in the tables should
	 *   be formatted bold (to indicate
	 *   column headings).
	 *
	 * \returns Copy of a string containing
	 *   the content of the new OpenDocument
	 *   spreadsheet to be written to disk.
	 *
	 * \note The result is ZIP-compressed
	 *   and needs therefore be handled as
	 *   binary data.
	 */
	inline std::string exportSpreadsheet(const std::vector<NamedTable>& tables, bool firstRowBold) {
		std::vector<StringString> fileContents;

		// add MIME type
		fileContents.emplace_back("mimetype", "application/vnd.oasis.opendocument.spreadsheet");

		// add package manifest
		fileContents.emplace_back(
				"META-INF/manifest.xml",
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				"<manifest:manifest manifest-version=\"1.2\""
				" xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\">\n"
				" <manifest:file-entry manifest:full-path=\"/\" manifest:version=\"1.2\""
				" manifest:media-type=\"application/vnd.oasis.opendocument.spreadsheet\"/>"
				" <manifest:file-entry manifest:full-path=\"content.xml\" manifest:media-type=\"text/xml\"/>\n"
				"</manifest:manifest>"
		);

		// create content
		std::string content{
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<office:document-content"
			" xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\""
			" xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\""
			" xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\""
			" xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\""
			" xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0\">"
			" <office:automatic-styles>\n"
			"  <style:style style:name=\"headings\" style:family=\"table-cell\">\n"
			"   <style:text-properties fo:font-weight=\"bold\" style:font-weight-asian=\"bold\""
			" style:font-weight-complex=\"bold\" />\n"
			"  </style:style>\n"
			" </office:automatic-styles>\n"
			" <office:body>\n"
			"  <office:spreadsheet>\n"
		};

		for(const auto& table : tables) {
			content += "   <table:table table:name=\"" + table.first + "\">\n";

			bool boldRow{firstRowBold};

			for(const auto& row : table.second) {
				std::string style;

				if(boldRow) {
					style = "table:style-name=\"headings\"";

					boldRow = false;
				}

				content += "    <table:table-row>\n";

				for(const auto& cell : row) {
					content += OpenDocument::cell(cellSpacing, cell, style);
				}

				content += "    </table:table-row>\n";
			}

			content += "   </table:table>\n";
		}

		content +=	"  </office:spreadsheet>\n"
					" </office:body>\n"
					"</office:document-content>\n";

		fileContents.emplace_back("content.xml", content);

		return Data::Compression::Zip::compress(fileContents);
	}

	//! Creates the XML code for a simple cell containing a value.
	/*!
	 * If the given raw data is numeric, the
	 *  cell will contain a float.
	 *
	 * If the given raw data is a string, the
	 *  special characters @c &'><" will be
	 *  escaped.
	 *
	 * Newlines will be added at the end of
	 *  elements for formatting purposes, and
	 *  Sub-elements will be nested with one
	 *  additional space.
	 *
	 * \param spacing The number of spaces to
	 *   add before the cell.
	 * \param raw The raw data contained in
	 *   the cell.
	 * \param style An optional style argument
	 *   present in the cell, in the format
	 *   @c table:style-name=\"...\".
	 *
	 * \returns A new string containing the
	 *   OpenDocument XML code for the cell.
	 */
	inline std::string cell(std::uint8_t spacing, const std::string& raw, const std::string& style) {
		const std::string spaces(spacing, ' ');

		if(raw.empty()) {
			return spaces + "<table:table-cell />\n";
		}

		std::string content{raw};
		std::string attributes{};
		double numericValue{};
		bool isString{true};

		if(!style.empty()) {
			attributes.reserve(style.size() + 1);

			attributes.push_back(' ');

			attributes += style;
		}

		if(Helper::Strings::isDec(raw)) {
			// try to convert to numeric value
			const auto oldLocale{
				std::setlocale(LC_NUMERIC, "C")
			};

			try {
				numericValue = std::stod(raw);

				isString = false;

				attributes += " office:value-type=\"float\" office:value=\"";
				attributes += std::to_string(numericValue);
				attributes += "\"";
			}
			catch(const std::logic_error& /*unused*/) {}

			// reset C locale
			std::setlocale(LC_NUMERIC, oldLocale);
		}

		if(isString) {
			// replace special characters
			Helper::Strings::replaceAll(content, "&", "&amp;");
			Helper::Strings::replaceAll(content, "'", "&apos;");
			Helper::Strings::replaceAll(content, ">", "&gt;");
			Helper::Strings::replaceAll(content, "<", "&lt;");
			Helper::Strings::replaceAll(content, "\"", "&quot;");
		}

		std::string result{};

		result.reserve(cellLines * spaces.size() + attributes.size() + content.size() + cellConstChars);

		result = spaces;

		result += "<table:table-cell";
		result += attributes;
		result += ">\n";
		result += spaces;
		result += "<text:p>";
		result += content;
		result += "</text:p>\n";
		result += spaces;
		result += "</table:table-cell>\n";

		return result;
	}

} /* rawlservpp::Data::ImportExport::OpenDocument */

#endif /* DATA_IMPORTEXPORT_OPENDOCUMENT_HPP_ */
