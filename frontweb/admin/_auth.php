<?php

$valid_passwords = array ("shkim" => "comic");
$valid_users = array_keys($valid_passwords);

$AuthUser = $_SERVER['PHP_AUTH_USER'];

$validated = (in_array($AuthUser, $valid_users)) && ($_SERVER['PHP_AUTH_PW'] == $valid_passwords[$AuthUser]);

if (!$validated) {
  header('WWW-Authenticate: Basic realm="Sleek Manhwabang"');
  header('HTTP/1.0 401 Unauthorized');
  die ("Not authorized");
}

mysql_connect("localhost:3306", "manga", "sleeker"); mysql_select_db("manhwa") or die("Could not connect to DBMS."); $ComicBaseUrl = "http://comic.ogp.kr/a";
//mysql_connect("localhost:3306", "dev_manga", "sleeker"); mysql_select_db("dev_comic") or die("Could not connect to DBMS."); $ComicBaseUrl = "http://dev_comic.ogp.kr/a";
mysql_query("SET NAMES 'utf8'");

// If arrives here, is a valid user.
//echo "<p>Welcome $AuthUser.</p>";

?>
