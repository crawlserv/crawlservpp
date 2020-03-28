
<?php

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
 * corpus.php
 *
 * Corpus helper functions for the PHP/JavaScript frontend for crawlserv++.
 * 
 * @requires helpers.php !!!
 *
 */

// check for valid UTF-8 (by javalc6 at gmail dot com @ https://www.php.net/manual/en/function.mb-check-encoding.php#95289)
function check_utf8($str) {
    $len = strlen($str);
    
    for($i = 0; $i < $len; $i++){
        $c = ord($str[$i]);
        
        if($c > 128) {
            if(($c > 247))
                return false;
            else if($c > 239)
                $bytes = 4;
            else if($c > 223)
                $bytes = 3;
            else if($c > 191)
                $bytes = 2;
            else
                return false;
            
            if(($i + $bytes) > $len)
                return false;
            
            while($bytes > 1) {
                $i++;
                
                $b = ord($str[$i]);
                
                if($b < 128 || $b > 191)
                    return false;
                
                $bytes--;
            }
        }
    }
    
    return true;
}

// cut up to three invalid UTF-8 bytes from the end of a (binary) string
function trim_to_valid_utf8($str) {    
    $l = strlen($str);
    
    if(!$l)
        return "";

    for($cut = 0; $cut < 4; ++$cut) {        
        if($cut + 1 > $l)
            break;
        
        // check one byte before the cut            
        if(check_utf8(substr($str, $l - $cut - 1, 1)))           
            return substr($str, 0, $l - $cut);
        
        if($cut + 2 > $l)
            break;
        
        // check two bytes before the cut
        if(check_utf8(substr($str, $l - $cut - 2, 2)))
            return substr($str, 0, $l - $cut);
        
        if($cut + 3 > $l)
            break;
        
        // check three bytes before the cut
        if(check_utf8(substr($str, $l - $cut - 3, 3)))
            return substr($str, 0, $l - $cut);
        
        if($cut + 4 > $l)
            break;
        
        // check four bytes before the cut
        if(check_utf8(substr($str, $l - $cut - 4, 4)))
            return substr($str, 0, $l - $cut);
    }
    
    if($cut == 4)
        echo "<div><b>WARNING: Invalid UTF-8 in string!</b></div>";
    
    return "";
}

// convert a chunk index in bytes to a chunk index in characters and update the byte index accordingly
function byte_to_character_index(&$bytePosInChunk, $chunk_id) {
    global $dbConnection;
    
    // get four bytes before the current position (= max size of a UTF-8 character)
    $result = $dbConnection->query(
            "SELECT SUBSTR(
                    BINARY corpus,
                    ".($bytePosInChunk > 3 ? $bytePosInChunk - 3 : 1). /* + 1 because MySQL string indices start at 1 */
                    ", ".($bytePosInChunk > 3 ? 4 : $bytePosInChunk)."
            ) AS last
            FROM `crawlserv_corpora`
            WHERE id = $chunk_id
            LIMIT 1"
    );
   
    if(!$result)
        die("ERROR: Could not convert byte index #$bytePosInChunk to character index in corpus chunk #$chunk_id");
    
    $row = $result->fetch_assoc();
    
    if(!$row)
        die("ERROR: Could not convert byte index #$bytePosInChunk to character index in corpus chunk #$chunk_id");
   
    $last = $row["last"];
    
    $toTrim = ($bytePosInChunk > 3 ? 4 : $bytePosInChunk) - strlen(trim_to_valid_utf8($last));
    
    $bytePosInChunk -= $toTrim;
    
    $result->close();
    
    if(!$bytePosInChunk)
        return 0;
    
    $result = $dbConnection->query(
            "SELECT CHAR_LENGTH(
                    CONVERT(
                            SUBSTR(
                                    BINARY corpus,
                                    1,
                                    $bytePosInChunk
                            ) USING utf8mb4
                    )
            ) AS `index`
            FROM `crawlserv_corpora`
            WHERE id = $chunk_id
            LIMIT 1"
    );
    
    if(!$result)
        die("ERROR: Invalid UTF-8 characters in corpus");
    
    $row = $result->fetch_assoc();
    
    if(!$row)
        die("ERROR: Could not convert byte index #$bytePosInChunk to character index in corpus chunk #$chunk_id");
    
    $index = $row["index"];
    
    $result->close();
    
    return $index;
}

// convert a length in bytes to a length of characters at the specified character position in the specified corpus chunk
function byte_to_character_length($bytePosInChunk, $byteLen, $chunk_id) {
    global $dbConnection;
    
    $result = $dbConnection->query(
            "SELECT CHAR_LENGTH(
                    IFNULL(
                            CONVERT(
                                    str USING utf8mb4
                            ),
                            IFNULL(
                                    CONVERT(
                                            SUBSTR(
                                                    str,
                                                    1,
                                                    LENGTH(str) - 1
                                            ) USING utf8mb4
                                    ),
                                    IFNULL(
                                            CONVERT(
                                                    SUBSTR(
                                                            str,
                                                            1,
                                                            LENGTH(str) - 2
                                                    ) USING utf8mb4
                                            ),
                                            CONVERT(
                                                    SUBSTR(
                                                            str,
                                                            1,
                                                            LENGTH(str) - 3
                                                    ) USING utf8mb4
                                            )
                                    )
                            )
                    )
             ) AS l
             FROM (
                    SELECT SUBSTR(
                            BINARY corpus,
                            $bytePosInChunk + 1,
                            $byteLen
                    ) AS str
                    FROM `crawlserv_corpora`
                    WHERE id=$chunk_id
                    LIMIT 1
          ) AS subquery
          LIMIT 1"
    );
    
    if(!$result)
        die("ERROR: Could not convert byte length '$byteLen' to character length in corpus chunk #$chunk_id");
    
    $row = $result->fetch_assoc();
    
    if(!$row)
        die("ERROR: Could not convert byte length '$byteLen' to character length in corpus chunk #$chunk_id");
    
    $l = $row["l"];
    
    $result->close();
    
    return $l;
}

// print the string at the specified byte offset while including the specified markers
//  NOTE: uses $char_pos as character offset and updates it according to the number of printed (multibyte) characters
function print_with_markers($string_offset, $str, $markers, &$char_pos) {    
    // check length of string
    if(!strlen($str))
        return;
    
    // sort markers
    usort($markers, function($a, $b) {
        if($a[0] == $b[0]) {
            if($a[1] == "art_beg" && $b[1] == "dat_beg")
                // move begin of date in front of begin of article
                return true;
                
            return false;
        }
        
        return $a[0] - $b[0];   // sort by position
    });
    
    // check first marker position
    if(count($markers) && $string_offset > $markers[0][0])
        die(
                "ERROR: marker @ #"
                .$markers[0][0]
                ."["
                .$markers[0][1]
                .":"
                .$markers[0][2]
                ."] before offset @ #$string_offset"
        );
    
    // render HTML with markers
    $pos = 0;
    
    foreach($markers as $marker) {        
        // print text before marker
        $l = $marker[0] - $string_offset - $pos;
        $txt = substr($str, $pos, $l);
        
        echo html($txt);
        
        // update character position according to the number of printed (multibyte) characters
        $char_pos += mb_strlen($txt, "UTF-8");
        
        // free memory
        unset($txt);
        
        // update position of marker
        $pos += $l;
        
        // print marker
        echo "<span class=\"content-corpus-control\" data-tippy-content=\"";
        
        $symbol = '';
        
        if($marker[1] == "art_beg") {
            echo "Begin of article";
            $symbol = '[';
        }
        else if($marker[1] == "art_end") {
            echo "End of article";
            $symbol = ']';
        }
        else if($marker[1] == "dat_beg") {
            echo "Begin of date";
            $symbol = '{';
        }
        else if($marker[1] == "dat_end") {
            echo "End of date";
            $symbol = '}';
        }
        else
            die("ERROR: Unknown type of marker encountered");
            
        echo " ".$marker[2]."<br>\n@ byte #".$marker[0]."<br>\n
                @ char #$char_pos\"
                data-tippy-delay=\"0\"
                data-tippy-duration=\"0\"
                data-tippy-arrow=\"true\"
                data-tippy-placement=\"left-start\"
                data-tippy-size=\"small\">$symbol</span>";
    }
    
    // print text behind the last marker if necessary
    if($pos < strlen($str)) {
        $txt = substr($str, $pos);
        
        echo html($txt, ENT_QUOTES | ENT_HTML401, "UTF-8");
        
        // update character position according to the number of printed (multibyte) characters
        $char_pos += mb_strlen($txt, "UTF-8");
    }
}

?>