<?

include_once 'conf.php';

if( $_POST['request_code'] != '' ){
	if( $_POST['user_pass']!=$foxpassword )
		die('Password is incorrect.');
	$request_code = $_POST['request_code'];
	
	$path = $code_path . $request_code;
	if( !is_numeric($request_code) || file_exists($path) )
		die('Invalid request code.');
	$fp = fopen($path,"w");
	fclose($fp);
	die('Request code: '.$request_code );
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
						Create Code 
						</h1>
						
						</td></tr>
						<tr><td>
						<form name="registerForm" method="POST" action="createcode.php">
							<table id="Framer" class="MainTable" border="0">
								<tr><td>Request Code</td><td><input type="text" style="width: 150px" name="request_code"></td></tr>
								<tr><td>Password</td><td><input type="password" style="width: 150px" name="user_pass"></td></tr>
								<tr><td colspan="2" style="text-align: right"><input type="submit" value="Create"></td></tr>
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
