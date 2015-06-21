<?php
include_once 'conf.php';

if( $_POST['user_name'] != '' ){
	$user_name = safe($_POST['user_name']);
	$user_pass = safe($_POST['user_pass']);
	$user_pass2 = safe($_POST['user_pass2']);
	$request_code = $_POST['request_code'];
	
	$path = $code_path . $request_code;
	if( !is_numeric($request_code) || !file_exists($path) )
		die('Invalid request code.');
	if( $user_pass != $user_pass2 )
		die('Password verification failed.');
	$query   = "SELECT `pass_word` AS user_pass FROM `fox2_user` WHERE `user_name`='$user_name'";
	$result  = mysql_query($query) or die('Error, query failed:'.mysql_error() );
	$row     = mysql_fetch_array($result, MYSQL_ASSOC);
	if($row)
		die('The username is already used by another person. Please choose another.');
	if(strlen($user_name)<3)
		die('The username is too short.');
	mysql_free_result( $result );
	unlink( $path );

	$query   = "INSERT INTO `fox2_user`(`user_name`, `pass_word`, `register_time`) VALUES('{$user_name}', md5('{$user_pass}'), now() )";
	$result  = mysql_query($query) or die('Error, query failed:'.mysql_error() );
	die('Registered OK!');
}
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
	</head>
	<body style="background: #fafafa;">
	<!-- Body Begin -->
		<div id="Main" class="Container" style="width:780px; margin: 0 auto;">
		<!-- Container Begin -->
			<table>
				<td>
					<table style="width:400px">
						<tr><td>
						
						<h1>
						Register 
						</h1>
						
						</td></tr>
						<tr><td>
						<form name="registerForm" method="POST" action="register.php">
							<table id="Framer" class="MainTable" border="0">
								<tr><td>Username</td><td><input type="text" style="width: 150px" name="user_name"></td></tr>
								<tr><td>Request Code</td><td><input type="text" style="width: 150px" name="request_code" value="<?php echo $_GET['code'];?>"></td></tr>
								<tr><td>Password</td><td><input type="password" style="width: 150px" name="user_pass"></td></tr>
								<tr><td>Password Verification</td><td><input type="password" style="width: 150px" name="user_pass2"></td></tr>
								<tr><td colspan="2" style="text-align: right"><input type="submit" value="Register"></td></tr>
							</table>
						</form>
						</td></tr>
					</table>
				</td></tr>
			</table>
		<!-- Container End -->
		</div>
	<!-- Body End -->
	</body>
</html>
