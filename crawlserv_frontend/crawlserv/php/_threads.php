
<!-- 

   ***

    Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version in addition to the terms of any
    licences already herein identified.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
    
   ***
   
   _threads.php
    
   Use the database to show all of the module-specific threads
    on the crawlserv++ command-and-control server.

-->

<?php

/* * 
 * ---
 * 
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *  
 * ---
 * 
 */

$db_init = true;

require "../config.php";

// function to format the remainig time
function formatTime($seconds) {
    $result = "";
    $years = 0;
    $months = 0;
    $weeks = 0;
    $days = 0;
    $hours = 0;
    $minutes = 0;
    
    if($seconds > 31557600) {
        $years = floor($seconds / 31557600);
        
        $result .= $years."y ";
        
        $seconds -= $years * 31557600;
    }
    
    if($seconds > 2629746) {
        $months = floor($seconds / 2629746);
        
        $result .= $months."mth ";
        
        $seconds -= $months * 2629746;
    }
    
    if($years)
        return substr($result, 0, strlen($result) - 1);
    
    if($seconds > 604800) {
        $weeks = floor($seconds / 604800);
        
        $result .= $weeks."w ";
        
        $seconds -= $weeks * 604800;
    }
    
    if($months)
        return substr($result, 0, strlen($result) - 1);
    
    if($seconds > 86400) {
        $days = floor($seconds / 86400);
        
        $result .= $days."d ";
        
        $seconds -= $days * 86400;
    }
    
    if($weeks)
        return substr($result, 0, strlen($result) - 1);
    
    if($seconds > 3600) {
        $hours = floor($seconds / 3600);
        
        $result .= $hours."h ";
        
        $seconds -= $hours * 3600;
    }
    
    if($days)
        return substr($result, 0, strlen($result) - 1);
    
    if($seconds > 60) {
        $minutes = floor($seconds / 60);
        
        $result .= $minutes."min ";
        
        $seconds -= $minutes * 60;
    }
    
    if(strlen($result))
        return substr($result, 0, strlen($result) - 1);
    
    return "<1s";
}

$result = $dbConnection->query(
    "SELECT id,".
    " module,".
    " status,".
    " progress,".
    " last,".
    " paused,".
    " website,".
    " urllist,".
    " config,".
    " (".
        "SELECT (AVG(tmp.runtime) * (1 - a.progress)) / AVG(tmp.progress)".
        " FROM crawlserv_threads AS tmp".
        " WHERE tmp.module LIKE a.module".
        " AND tmp.website = a.website".
        " AND tmp.urllist = a.urllist".
        " AND tmp.config = a.config".
    " ) AS remaining".
    " FROM crawlserv_threads AS a"
);

if(!$result)
    http_response_code(503);

$num = $result->num_rows;

if($num) {
    echo "<div class=\"content-block\">\n";
    echo "<div class=\"entry-row\">\n";
    
    if($num == 1)
        echo "<div class=\"value\">One thread active.";
    else
        echo "<div class=\"value\">$num threads active.";
    
    echo "\n<span class=\"action-link-box\">\n";
    
    if($num) {
        echo "<a href=\"#\" class=\"action-link cmd\" data-cmd=\"pauseall\">Pause all</a>";
        echo " &middot; <a href=\"#\" class=\"action-link cmd\" data-cmd=\"unpauseall\">Unpause all</a>";
    }
    
    echo "</span>\n";
    echo "<span id=\"threads-ping\"></span>\n";
    
    echo "</div>\n";
    echo "</div>\n";
    echo "</div>\n";

    while($row = $result->fetch_assoc()) {
        // get website namespace
        $result2 = $dbConnection->query(
                "SELECT namespace".
                " FROM crawlserv_websites".
                " WHERE id=".$row["website"].
                " LIMIT 1"
        );
        
        if(!$result2)
            http_response_code(503);
        
        $row2 = $result2->fetch_assoc();
        
        if(!$row2)
            http_response_code(503);
        
        $website = $row2["namespace"];
        
        $result2->close();
        
        // get URL list name
        $result2 = $dbConnection->query(
                "SELECT namespace".
                " FROM crawlserv_urllists".
                " WHERE id=".$row["urllist"].
                " AND website=".$row["website"].
                " LIMIT 1"
        );
        
        if(!$result2)
            http_response_code(503);
        
        $row2 = $result2->fetch_assoc();
        
        if(!$row2)
            http_response_code(503);
        
        $urllist = $row2["namespace"];
        
        $result2->close();
        
        // get config namespace
        $result2 = $dbConnection->query(
                "SELECT name".
                " FROM crawlserv_configs".
                " WHERE id=".$row["config"].
                " AND website=".$row["website"].
                " LIMIT 1"
        );
        
        if(!$result2)
            http_response_code(503);
        
        $row2 = $result2->fetch_assoc();
        
        if(!$row2)
            http_response_code(503);
        
        $config = $row2["name"];
        
        $result2->close();
        
        echo "<div class=\"thread\">\n";
        echo "<div class=\"content-block\">\n";
        echo "<div class=\"entry-row small-text\">\n";
        
        echo "<b>".$row["module"]." #".$row["id"]."</b>"
            ." - <i>".$config." on ".$website."_".$urllist."</i>\n";
        
        // calculate remaining time
        $tooltip = "Estimated time until completion.\n&gt; Click to jump to specific ID.";
        
        echo "<span class=\"remaining\" title=\"$tooltip\" label=\"$tooltip\" "
            ."data-id=\"".$row["id"]."\" data-module=\"".$row["module"]."\" " 
            ."data-last=\"".$row["last"]."\">";
        
        if($row["remaining"] === null)
            echo "+&infin;";
        else    
            echo "+".formatTime($row["remaining"]);
            
        echo "</span>";
        echo "</div>\n";
        
        echo "<div class=\"entry-row";
        
        if($row["paused"])
            echo " value";
        
        echo "\">\n";
        
        echo "<span class=\"nowrap";
        
        // cut ERROR, INTERRUPTED or IDLE keyword and use CSS classes instead        
        $start = false;
        
        if($row["status"][0] == "[")
            $start = strpos($row["status"], "]", 1);
        
        if($start === false)
            $start = 0;
        else
            $start += 2;
        
        if(substr($row["status"], $start, 7) == "PAUSED ")
            $start += 7;
        
        if(substr($row["status"], $start, 6) == "ERROR ") {
            $cut = htmlentities(
                    substr(
                            $row["status"],
                            0,
                            $start
                    ).substr(
                            $row["status"],
                            $start + 6
                    )
            );
            
            echo " error\" title=\"$cut\">$cut";
        }
        else if(substr($row["status"], $start, 12) == "INTERRUPTED ") {
            $cut = htmlentities(
                    substr(
                            $row["status"],
                            0,
                            $start
                    ).substr(
                            $row["status"],
                            $start + 12
                    )
            );
            
            echo " interrupted\" title=\"$cut\">$cut";
        }
        else if(substr($row["status"], $start, 5) == "IDLE ") {            
            $cut = htmlentities(
                    substr(
                            $row["status"],
                            0,
                            $start
                    ).substr(
                            $row["status"],
                            $start + 5
                    )
            );
            
            echo " idle\" title=\"$cut\">$cut";
        }
        else
            echo "\" title=\"".
                 htmlentities($row["status"]).
                 "\">".
                 htmlentities($row["status"]);
        
        echo "</span>\n";
        echo "</div>\n";
        
        echo "<div class=\"action-link-box\">\n";
        echo "<div class=\"action-link\">\n";
        
        if($row["paused"])
            echo "<a href=\"#\" class=\"action-link thread-unpause\" data-id=\""
                .$row["id"]
                ."\" data-module=\""
                .$row["module"]."\">"
                ."Unpause</a>\n";
        else {
            echo "<progress value=\"";
            
            if(floatval($row["progress"]))
                echo $row["progress"]."\" title=\""
                    .number_format(round(floatval($row["progress"]) * 100, 2), 2)
                    ."%\n&gt; #"
                    .number_format($row["last"])
                    ."\" max=\"1";
            
            echo "\"></progress>\n";
            
            echo "<a href=\"#\" class=\"action-link thread-pause\" data-id=\""
                .$row["id"]
                ."\" data-module=\""
                .$row["module"]."\">"
                ."Pause</a>\n";
        }
        
        echo " &middot; ";
        
        echo "<a href=\"#\" class=\"action-link thread-stop\" data-id=\""
            .$row["id"]
            ."\" data-module=\""
            .$row["module"]
            ."\">Stop</a>\n";
        
        echo "</div>\n";
        echo "</div>\n";
        echo "</div>\n";
        echo "</div>\n";
    }
}
else {
    echo "<div class=\"content-block\">\n";
    echo "<div class=\"entry-row\">\n";
    echo "<div class=\"value\">No threads active. <span id=\"threads-ping\"></span></div>";
    
    echo "</div>\n";
    echo "</div>\n";
}

$result->close();

?>
