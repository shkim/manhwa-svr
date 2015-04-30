<?php

$curFolderId = $_SERVER["QUERY_STRING"];
if ($curFolderId == '') $curFolderId = 0;

if (!is_numeric($curFolderId))
{
        die("Invalid folder id: " . $curFolderId);
}

include("_auth.php");

if ($curFolderId == 0)
{
	// top level
	$curFolderLevel = 2;
}
else
{
	$rs = mysql_query("SELECT parent_id, min_level, name FROM dir_folder WHERE _id=$curFolderId");
	$row = mysql_fetch_array($rs);
	$curFolderParentId = $row['parent_id'];
	$curFolderLevel = $row['min_level'];
	$curFolderName = $row['name'];
}


?><!DOCTYPE html>
<html lang="ko">
<head>
	<meta charset="UTF-8">
	<title>폴더 목록</title>
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link href="/css/bootstrap.min.css" rel="stylesheet" media="screen">
<style>
body { margin:10px }
tr:hover { background-color: #ddddee; }
</style>

</head>
<body>

<?php if ($curFolderId > 0) { ?>
<a href="folder_list.php?<?=$curFolderParentId?>">Go to parent of <?=$curFolderName?></a>
<?php } ?>

<h3>폴더 디렉토리 목록</h3>
<table border="1">
<tr>
	<th>ID</th>
	<th>Seq</th>
	<th>레벨</th>
	<th>이름</th>
</tr>
<?php

$sqlWhere = ($curFolderId > 0) ? "=$curFolderId" : "IS NULL";
$rs = mysql_query("SELECT * FROM dir_folder WHERE parent_id $sqlWhere ORDER BY seq");
while($row = mysql_fetch_array($rs))
{
	$completed = $row['completed'] != 0 ? '完':'연재중';
	$ready = $row['ready'] != 0 ? '서비스':'감춤';
?>
<tr onclick="gotoFolder(<?=$row['_id']?>)">
	<td><?=$row['_id']?></td>
	<td><?=$row['seq']?></td>
	<td><?=$row['min_level']?></td>
	<td><?=$row['name']?></td>
</tr>
<?php
}
?>
</table>
<form name="f1">
새 폴더 추가: <input type="text" name="fname" size="20"/>
<input type="button" value="추가" onclick="addFolder()"/>
</form>

<h3>폴더 아이템 목록</h3>
<table border="1">
<tr>
	<th>ID</th>
	<th>Seq</th>
	<th>레벨</th>
	<th>이름</th>
</tr>
<?php

$rs = mysql_query("SELECT A._id, seq, A.min_level d_level, name_kor, B.min_level t_level FROM dir_item A INNER JOIN title B ON A.title_id=B._id WHERE folder_id=$curFolderId ORDER BY seq");
while($row = mysql_fetch_array($rs))
{
?>
<tr>
	<td><?=$row['_id']?></td>
	<td><?=$row['seq']?></td>
	<td><?=$row['d_level']?> (<?=$row['t_level']?>)</td>
	<td><?=$row['name_kor']?></td>
</tr>
<?php
}
?>
</table>
<form name="f2">
새 아이템 추가: <input type="hidden" name="title_id"/>
<span id="title_name"></span>
<input type="button" value="타이틀 선택" onclick="selectTitle()"/>
<input type="button" value="추가" onclick="addItem()"/>
</form>

	<script src="/js/jquery2.min.js"></script>
	<script src="/js/bootstrap.min.js"></script>
<script>
function gotoFolder(fid)
{
	location.href = "folder_list.php?" + fid;
}
function selectTitle()
{
	window.open('select_title.php', 'select', 'width=400,height=200, resizable=Yes,scrollbars=Yes,toolbar=no,menubar=no,location=no,directories=no,status=No');
}
function addFolder()
{
	var fname = document.f1.fname.value;
	if (fname == '')
	{
		alert('폴더명을 입력하세요.');
		return;
	}

	var json = {
		parent_id:<?=$curFolderId?>,
		min_level:<?=$curFolderLevel?>,
		name:fname
	};


	$.ajax({
		'url':"<?=$ComicBaseUrl?>/admin/dir_folder/add",
		type:"POST",
		contentType: "application/json",
		data:JSON.stringify(json),
		success:function(data) {
			if (data.result == 'OK')
			{
				var folderId = data.data.folder_id;
				alert('요청이 성공하였습니다. (folderId='+folderId+')');
				location.href = "folder_list.php?" + folderId;
			}
			else
			{
				alert('요청이 실패했습니다: ' + data.error);
			}
		}
	});
}
function addItem()
{
	if(!confirm("'"+document.getElementById('title_name').innerHTML +"'을 추가하시겠습니까?"))
		return;

	var json = {
		folder_id:<?=$curFolderId?>,
		min_level:<?=$curFolderLevel?>,
		title_id:document.f2.title_id.value
	};

	$.ajax({
		'url':"<?=$ComicBaseUrl?>/admin/dir_item/add",
		type:"POST",
		contentType: "application/json",
		data:JSON.stringify(json),
		success:function(data) {
			if (data.result == 'OK')
			{
				var folderId = data.data.folder_id;
				var itemId = data.data.item_id;
				alert('요청이 성공하였습니다. (itemId='+itemId+')');
				location.href = "folder_list.php?" + folderId;
			}
			else
			{
				alert('요청이 실패했습니다: ' + data.error);
			}
		}
	});

}
</script>
</body>
</html>
