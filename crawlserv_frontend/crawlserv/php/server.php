
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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * server.php
 *
 * Status of and control over the command-and-control server.
 *
 */

$cc_init = true;

require "config.php";

?>

<h2>Status</h2>

<div class="content-block">

<div id="server-status">

<div class="checking-server-status">checking status...</div>

</div> <span id="server-ping-value"></span><span id="server-ping"></span>

</div>

<h2>Control</h2>

<div class="content-block">
<div class="entry-row">

<div class="entry-label">IP address:</div><div class="entry-input">

<input id="cmd-allow-form-ip" class="entry-input trigger" data-trigger="cmd-allow" type="text" /></div>

</div>

<div class="action-link-box">

<div class="action-link"><a id="cmd-allow" href="#" class="action-link">Allow access to server</a></div>
<div class="action-link"><a href="#" class="action-link cmd" data-cmd="disallow">Reset allowed IP addresses</a></div>

</div>
</div>

<div class="content-block">
<div class="entry-row">

<div class="entry-label">Command:</div><div class="entry-input">

<input id="cmd-custom-form-cmd" class="entry-input trigger" data-trigger="cmd-custom" type="text" /></div>

</div>

<div id="cmd-args"></div>

<div class="action-link-box">

<div class="action-link"><a id="cmd-custom-addarg" href="#" class="action-link">Add argument</a></div>
<div class="action-link"><a id="cmd-custom" href="#" class="action-link">Run command</a></div>

</div>
</div>

<div class="content-block"><a href="#" class="cmd kill" data-cmd="kill"><span id="kill-server">Kill server</span></a></div>

