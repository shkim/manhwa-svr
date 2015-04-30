<?php 

$authorId = $_SERVER["QUERY_STRING"];
if (!is_numeric($authorId))
{
	die("Invalid author id: " . $authorId);
}

include("_auth.php"); 

if ($authorId == 0)
{
	$sbmLabel = "등록";
}
else
{
	$rs = mysql_query("SELECT * FROM author WHERE _id=$authorId");
	$row = mysql_fetch_array($rs);
	if ($row == null)
	{
		die("Author not found for ID=$authorId");
	}

	$nameKor = $row['name_kor'];
	$nameOrig = $row['name_orig'];
	$birthYear = $row['birth_year'];

	$sbmLabel = "업데이트";
}

?><!DOCTYPE html>
<html lang="ko">
<head>
	<meta charset="utf-8">
	<title>작가 정보</title>
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link href="/css/bootstrap.min.css" rel="stylesheet" media="screen">
<style>
body { margin:10px }
tr:hover { background-color: #ddddee; }
</style>

</head>
<body>

<a href="author_list.php">Back</a><h3>작가 정보</h3>
<form name="f1">
<table border="1">
<tr>
	<th>ID</th>
	<td><?=$row['_id']?></td>
</tr>
<tr>
	<th>한글 이름</th>
	<td><input type="text" name="name_kor" size="48" value="<?=$nameKor?>"/></td>
</tr>
<tr>
	<th>원어 이름</th>
	<td><input type="text" name="name_orig" size="48" value="<?=$nameOrig?>"/></td>
</tr>
<tr>
	<th>출생년도</th>
	<td><input type="text" name="birth_year" size="4" value="<?=$birthYear?>"/></td>
</tr>
</table>
</br>

<input type="button" value="<?=$sbmLabel?>" onclick="onSubmit()"/>
</form>
<br/>

<?php
?>

	<script src="/js/jquery2.min.js"></script>
	<script src="/js/bootstrap.min.js"></script>
<script>
function onSubmit()
{
	var f1 = document.f1;
	var nameKor = f1.name_kor.value;
	var nameOrig = f1.name_orig.value;
	var birthYear = f1.birth_year.value;

	if (nameKor == '')
	{
		alert('한글 이름을 입력하세요.');
		return;
	}

	if (nameOrig == '')
	{
		alert('원어 이름을 입력하세요.');
		return;
	}

	if (!(birthYear>=1900 && birthYear<2015))
	{
		alert('출생년도 값이 올바르지 않습니다.');
		return;
	}


	var json = {
		"name_kor":nameKor,
		"name_orig":nameOrig,
		"birth_year":birthYear
	};

	var url = "<?=$ComicBaseUrl?>/admin/author/<?=$authorId?>";

	$.ajax({
		'url':url,
		type:"POST",
		contentType: "application/json", 
		data:JSON.stringify(json),
		success:function(data) {
			if (data.result == 'OK')
			{
				var authorId = data.data.author_id;
				alert('요청이 성공하였습니다. (authorId='+authorId+')');
				location.href = "author_info.php?" + authorId;
			}
			else
			{
				alert('요청이 실패했습니다: ' + JSON.stringify(data.error));
			}
		}
	});
}

</script>
</body>
</html>

