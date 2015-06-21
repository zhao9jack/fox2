<?php

include_once 'conf.php';
function generate_key($length)
{
	$pattern = '1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLOMNOPQRSTUVWXYZ';    //字符池
	$len = strlen($pattern);
	for($i=0; $i<$length; $i++)
	{
		$key .= $pattern[mt_rand(0,$len-1)];    //生成php随机数
	}
	return $key;
}

$user_name = safe($_POST['user_name']);
$user_pass = safe($_POST['user_pass']);
$client_port = $_POST['client_port'];
$client_ip = $_POST['client_ip'];
if( $client_port==''||$client_ip=='' )
	die('client_port or client_port is empty.');
$query   = "SELECT `pass_word` AS user_pass, last_login_time, login_count FROM `fox2_user` WHERE `user_name`='$user_name'";
$result  = mysql_query($query) or die('Error, query failed:'.mysql_error() );
$row     = mysql_fetch_array($result, MYSQL_ASSOC);
if(!$row)
	die('Wrong username or password.');
$vpassword = $row['user_pass'];
mysql_free_result( $result );
if( md5($user_pass)!=$vpassword || $user_name=='' )
	die('Wrong username or password.');

$query   = "UPDATE `fox2_user` SET `last_login_ip`='{$direct_ip}', `last_login_time`=now(), login_count=login_count+1 WHERE `user_name`='$user_name'";
mysql_query($query) or die('Error, query failed:'.mysql_error() );

$session_key = generate_key(16);
setcookie('session_key', $session_key );
setcookie('client_port', $client_port );
setcookie('user_name', $user_name );
$url = "/fox2/update_ip.do?password={$foxpassword}&key={$session_key}&user={$user_name}&cmd=login";
requestUrl( '127.0.0.1', '127.0.0.1', $url );

?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
	<head>
		<!-- Meta Begin -->
		<meta name="date" content="2008-08-29T19:41:15+0800">
		<meta http-equiv="content-type" content="text/html;charset=utf-8">
		<meta http-equiv="content-script-type" content="text/javascript">
		<meta http-equiv="content-style-type" content="text/css">
		<!-- Meta End -->
		<title>Fox2</title>
		<script src="http://<?php echo $client_ip.':'.$client_port;?>/update_session.do?user=<?php echo $user_name;?>&key=<?php echo $session_key;?>" type="text/javascript"></script>
		<script language="javascript">
		function logout(){
			document.location = "logout.php";
		}
		</script>
	</head>
	<body style="background: #fafafa;">
	<!-- Body Begin -->
		<div id="Main" class="Container" style="width:780px; margin: 0 auto;">
		<!-- Container Begin -->
			<table>
				<tr><td><img alt="fox" src="./fox3.gif"></td>
				<td>
					<p>
					Login successfully!<br> You are able to access Internet through our proxy server now.</p>
					<?php
					echo "<p>Login count: {$row['login_count']}</p>";
					echo "<p>Last login time: {$row['last_login_time']}</p>";
					?>
					<p><a href="changepwd.php" target="_blank">Change Password</a></p>
					<p><input type="button" onclick="javasript: logout();" value="Logout"></p>
				</td></tr>
			</table>
		<!-- Container End -->
		</div>
	<!-- Body End -->
	</body>
</html>
