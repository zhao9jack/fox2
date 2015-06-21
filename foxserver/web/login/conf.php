<?php
$dbhost = 'localhost';
$dbuser = 'fox';
$dbpass = '';
$foxpassword = 'password';
$code_path = '/home/fox/www/code/';
$conn = mysql_connect($dbhost, $dbuser, $dbpass) or die('Error connecting to mysql');

$dbname = 'fox';
mysql_select_db($dbname);

function safe($s)
{
	if(!get_magic_quotes_gpc())
	{
		if(is_array($s))
			foreach($s as $key=>$value)
				$s[$key] = addslashes($value);
		else
			$s=addslashes($s);
	}
	return $s;
}

function requestUrl($ip, $host, $url)    
{
	$fp = fsockopen ($ip, 80, $errno, $errstr, 90);    
	if (!$fp)
		return false;   
	$out = "GET {$url} HTTP/1.1\r\n";    
	$out .= "Host:{$host}\r\n";    
	$out .= "Connection: close\r\n\r\n";    
	fputs ($fp, $out);
	fclose( $fp );
}

$direct_ip = safe($_COOKIE['direct_ip']);

?>
