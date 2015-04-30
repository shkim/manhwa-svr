<?php include("_auth.php"); ?><!DOCTYPE html>
<html lang="ko">
<head>
	<meta charset="UTF-8">
	<title>Title List</title>
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link href="/css/bootstrap.min.css" rel="stylesheet" media="screen">
<style>
body { margin:10px }
tr:hover { background-color: #ddddee; }
</style>

</head>
<body>

<h3>작품 목록</h3>
<table border="1">
<tr>
	<th>ID</th>
	<th>한글 제목</th>
	<th>원판 제목</th>
	<th>레벨</th>
	<th>권 수</th>
	<th>완결</th>
	<th>상태</th>
</tr>
<?php

$rs = mysql_query("SELECT * FROM title");
while($row = mysql_fetch_array($rs))
{
	$completed = $row['completed'] != 0 ? '完':'연재중';
	$ready = $row['ready'] != 0 ? '서비스':'감춤';
?>
<tr onclick="gotoEdit(<?=$row['_id']?>)">
	<td><?=$row['_id']?></td>
	<td><?=$row['name_kor']?></td>
	<td><?=$row['name_orig']?></td>
	<td><?=$row['min_level']?></td>
	<td><?=$row['volume_cnt']?></td>
	<td><?=$completed?></td>
	<td><?=$ready?></td>
</tr>
<?php
}
?>
</table>
</br>
<a href="title_info.php?0">타이틀 추가</a>

	<script src="/js/jquery2.min.js"></script>
	<script src="/js/bootstrap.min.js"></script>
<script>
function gotoEdit(tid)
{
	location.href = "title_info.php?" + tid;
}
</script>
</body>
</html>

