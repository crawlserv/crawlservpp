
<?php

/*
 * 
 * ---
 * 
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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
 * version.php
 *
 * Version of the PHP/JavaScript frontend for crawlserv++.
 *
 */

$GLOBALS["crawlservpp_version_major"] = 0;
$GLOBALS["crawlservpp_version_minor"] = 0;
$GLOBALS["crawlservpp_version_release"] = 0;
$GLOBALS["crawlservpp_version_suffix"] = "alpha";

$GLOBALS["crawlservpp_version_string"] = 
                                $GLOBALS["crawlservpp_version_major"]
                                . "."
                                . $GLOBALS["crawlservpp_version_minor"]
                                . "."
                                . $GLOBALS["crawlservpp_version_release"]
                                . $GLOBALS["crawlservpp_version_suffix"];

?>