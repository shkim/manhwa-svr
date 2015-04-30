<?php include("_auth.php"); ?><!DOCTYPE html>
<html lang="ko">
<head>
	<meta charset="UTF-8">
	<title>작품 선택</title>
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link href="/css/bootstrap.min.css" rel="stylesheet" media="screen">
<style>
body { margin:10px }
tr:hover { background-color: #ddddee; }
</style>

</head>
<body>
<?php
$keyword = $_POST['kwd'];
if ($keyword != '')
{
	$rs = mysql_query("SELECT * FROM title WHERE name_kor LIKE '%$keyword%' OR name_orig LIKE '%$keyword%' LIMIT 30");
	$numRows = mysql_num_rows($rs);
}
?>

<form method="post">
제목 (일부):
<input type="text" name="kwd" value="<?=$keyword?>"/>
<input type="submit" value="검색"/>
</form>

<hr/>

<?php
if ($numRows > 0)
{
?>
<table border="1">
<tr>
	<th>ID</th>
	<th>한글 제목</th>
	<th>원어 제목</th>
	<th>Ready</th>
	<th>Level</th>
</tr>
<?php
	while($row = mysql_fetch_array($rs))
	{
?>
<tr onclick="selectRow(<?=$row['_id']?>)">
	<td><?=$row['_id']?></td>
	<td id="n<?=$row['_id']?>"><?=$row['name_kor']?></td>
	<td><?=$row['name_orig']?></td>
	<td><?=$row['ready']?></td>
	<td><?=$row['min_level']?></td>

</tr>
<?php
	}
?>
</table>
<?php
}
else
{
?>
<center>검색된 결과가 없습니다.</center>
<?php
}
?>

	<script src="/js/jquery2.min.js"></script>
	<script src="/js/bootstrap.min.js"></script>
<script>
function selectRow(tid)
{
	var wo = window.opener;
	if (wo == null)
	{
		alert("Opener is null");
		return;
	}

	var name = document.getElementById('n'+tid).innerHTML;
	if (confirm("'" + name + "'을(를) 선택하시겠습니까?"))
	{
		wo.document.f2.title_id.value = tid;
		wo.document.getElementById('title_name').innerHTML = name;
		window.close();
	}
}
</script>
</body>
</html>

