<?php
$latest_version = "1.0.2";

$client_port = $_GET['client_port'];
$client_ip = $_GET['client_ip'];
$direct_ip = $_GET['direct_ip'];
if ($client_port=='')
	$client_port = '1998';
if ($client_ip=='')
	$client_ip = 'localhost';
$client_version = $_GET['client_version'];
setcookie('direct_ip', $direct_ip );
setcookie('client_ip', $client_ip );
setcookie('client_port', $client_port );
setcookie('client_version', $client_version );
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
				<tr><td><img alt="fox" src="./fox.jpg"></td>
				<td>
					<table style="width:400px">
						<tr><td>
						
						<h1>
						The Fox2
						</h1>
						<p>
						Fox2 is a name of a project focusing on "TCP over HTTP" techniques. ("HTTP over HTTP" in Fox1)
						</p>
						<p>
						It's not open for the public. You need an account to join in the project.
						</p>
						<?php if( strcmp($client_version, $latest_version)<0 ){
							echo '<p>
							<font color="red">You are not using the latest Fox2 Client.<br />';
							if($client_version!='') 
								echo 'The latest version is '.$latest_version.', but your version is '.$client_version ;
							echo '</font><br />
							Click <a href="fox2client.zip">here</a> to download the latest Fox2 Client.</p>';
						      }
						?>
						</td></tr>
						<tr><td>
						<form name="loginForm" method="POST" action="login.php">
							<table id="Framer" class="MainTable" border="0">
								<tr><td>Username</td><td><input type="text" style="width: 150px" name="user_name"></td></tr>
								<tr><td>Password</td><td><input type="password" style="width: 150px" name="user_pass"></td></tr>
								<tr><td>Client IP</td><td><input type="text" style="width: 100px" name="client_ip" value="<?php echo $client_ip;?>"></td></tr>
								<tr><td>Client Port</td><td><input type="text" style="width: 50px" name="client_port" value="<?php echo $client_port;?>"></td></tr>
								<tr><td colspan="2" style="text-align: right">
									<input type="submit" value="Login">
								</td></tr>
								<tr><td>
								<a href="register.php" target="_blank">Sign Up</a>
								</td></tr>
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
