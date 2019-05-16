
<?php

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

