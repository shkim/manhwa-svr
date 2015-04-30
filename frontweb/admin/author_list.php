<?php include("_auth.php"); ?><!DOCTYPE html>
<html lang="ko">
<head>
	<meta charset="UTF-8">
	<title>작가 목록</title>
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link href="/css/bootstrap.min.css" rel="stylesheet" media="screen">
<style>
body { margin:10px }
tr:hover { background-color: #ddddee; }
</style>

</head>
<body>

<h3>작가 목록</h3>
<table border="1">
<tr>
	<th>ID</th>
	<th>생년</th>
	<th>한글 이름</th>
	<th>원어 이름</th>
</tr>
<?php

$rs = mysql_query("SELECT * FROM author");
while($row = mysql_fetch_array($rs))
{
?>
<tr onclick="gotoEdit(<?=$row['_id']?>)">
	<td><?=$row['_id']?></td>
	<td><?=$row['birth_year']?></td>
	<td><?=$row['name_kor']?></td>
	<td><?=$row['name_orig']?></td>
</tr>
<?php
}
?>
</table>
</br>
<a href="author_info.php?0">작가 추가</a>

	<script src="/js/jquery2.min.js"></script>
	<script src="/js/bootstrap.min.js"></script>
<script>
function gotoEdit(aid)
{
	location.href = "author_info.php?" + aid;
}
</script>
</body>
</html>
