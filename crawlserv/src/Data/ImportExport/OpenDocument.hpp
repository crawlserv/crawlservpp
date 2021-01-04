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

#include <string>	// std::string
#include <utility>	// std::pair
#include <vector>	// std::vector

//! Namespace for importing and exporting OpenDocument spreadsheets.
namespace crawlservpp::Data::ImportExport::OpenDocument {

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

	std::string exportSpreadsheet(const std::vector<NamedTable>& tables);

	///@}

	/*
	 * IMPLEMENTATION
	 */

	//! Exports tables as a OpenDocument spreadsheet.
	/*!
	 * \param tables Constant reference to
	 *  a vector containing the tables,
	 *  each consisting of a pair of its
	 *  name and a vector of rows, each
	 *  consisting of a vector of strings
	 *  representing the values in the row.
	 *
	 * \returns Copy of a string containing
	 *   the content of the new OpenDocument
	 *   spreadsheet to be written to disk.
	 *
	 * \note The result is ZIP-compressed
	 *   and needs therefore be handled as
	 *   binary data.
	 */
	std::string exportSpreadsheet(const std::vector<NamedTable>& tables) {
		std::vector<StringString> fileContents;

		// count number of columns

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
			" xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\">\n"
			" <office:automatic-styles></office:automatic-styles>\n"
			" <office:body>\n"
			"  <office:spreadsheet>\n"
		};

		for(const auto& table : tables) {
			content += "   <table:table table:name=\"" + table.first + "\">\n";

			for(const auto& row : table.second) {

				content += "    <table:table-row>\n";

				for(const auto& cell : row) {
					content +=	"     <table:table-cell>\n"
								"      <text:p>" + cell + "</text:p>\n"
								"     </table:table-cell>\n";
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

} /* rawlservpp::Data::ImportExport::OpenDocument */

#endif /* DATA_IMPORTEXPORT_OPENDOCUMENT_HPP_ */
