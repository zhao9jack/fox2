<?php

include_once 'conf.php';

$user = $_COOKIE['user_name'];
$key = $_COOKIE['session_key'];
requestUrl( '127.0.0.1', '127.0.0.1', "/fox2/update_ip.do?password={$foxpassword}&user={$user}&key={$key}&cmd=logout" );

header("Location: /?client_ip={$_COOKIE['client_ip']}&client_port={$_COOKIE['client_port']}&client_version={$_COOKIE['client_version']}&direct_ip={$direct_ip}"); 

?>


Redirecting to /
