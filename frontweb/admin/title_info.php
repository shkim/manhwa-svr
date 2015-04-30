<?php 

$titleId = $_SERVER["QUERY_STRING"];
if (!is_numeric($titleId))
{
	die("Invalid title id: " . $titleId);
}

include("_auth.php"); 

if ($titleId == 0)
{
	$sbmLabel = "등록";
	$minLevel = 1;
	$completed = 0;
}
else
{
	$rs = mysql_query("SELECT * FROM title WHERE _id=$titleId");
	$row = mysql_fetch_array($rs);
	if ($row == null)
	{
		die("Title not found for ID=$titleId");
	}

	$nameKor = $row['name_kor'];
	$nameOrig = $row['name_orig'];
	$minLevel = $row['min_level'];
	$volcnt = $row['volume_cnt'];
	$completed = $row['completed'];
	$ready = $row['ready'];

	$sbmLabel = "업데이트";

	$authorId = $row['author_id'];
	if ($authorId > 0)
	{
		$rs = mysql_query("SELECT name_kor FROM author WHERE _id=$authorId");
		$row = mysql_fetch_row($rs);
		$authorName = $row[0];
	}
}

?><!DOCTYPE html>
<html lang="ko">
<head>
	<meta charset="utf-8">
	<title>Title Info</title>
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link href="/css/bootstrap.min.css" rel="stylesheet" media="screen">
<style>
body { margin:10px }
tr:hover { background-color: #ddddee; }
</style>

</head>
<body>

<a href="title_list.php">Back</a><h3>작품 정보</h3>
<form name="f1">
<table border="1">
<tr>
	<th>ID</th>
	<td><?=$titleId?></td>
</tr>
<tr>
	<th>한글 제목</th>
	<td><input type="text" name="name_kor" size="48" value="<?=$nameKor?>"/></td>
</tr>
<tr>
	<th>원판 제목</th>
	<td><input type="text" name="name_orig" size="48" value="<?=$nameOrig?>"/></td>
</tr>
<tr>
	<th>레벨</th>
	<td><input type="text" name="min_level" size="2" value="<?=$minLevel?>"/></td>
</tr>
<tr>
	<th>작가</th>
	<td><input type="hidden" name="author_id" value="<?=$authorId?>"/>
	<span id="author_name"><?=$authorName?></span>
	<input type="button" value="작가 선택" onclick="selectAuthor()"/></td>
</tr>
<tr>
	<th>완결 여부</th>
	<td>
	<input type="radio" value="0" name="completed"<?php if($completed==0) echo " checked"; ?>>미완(연재중) &nbsp;
        <input type="radio" value="1" name="completed"<?php if($completed!=0) echo " checked"; ?>>완결
	</td>
</tr>
<?php if($titleId > 0) { ?>
<tr>
	<th>상태</th>
	<td>
	<input type="radio" value="0" name="ready"<?php if($ready==0) echo " checked"; ?>>감춤 &nbsp;
        <input type="radio" value="1" name="ready"<?php if($ready!=0) echo " checked"; ?>>서비스
	</td>
</tr>
<tr>
	<th>권 수</th>
	<td><?=$volcnt?></td>
</tr>
<?php } ?>
</table>
</br>

<input type="button" value="<?=$sbmLabel?>" onclick="onSubmit()"/>
</form>

<?php
if ($titleId > 0)
{
?>
<table border="1">
<tr>
	<th>No.</th>
	<th>페이지 방향</th>
	<th>양쪽 스캔</th>
	<th>상태</th>
	<th>ID</th>
</tr>
<?php
	$rs = mysql_query("SELECT _id, volume_no, ready, reverse_dir, two_paged FROM volume WHERE title_id=$titleId");
	while ($row = mysql_fetch_array($rs))
	{
		$ready = $row['ready'] != 0 ? '서비스':'감춤';
		$pagedir = $row['reverse_dir'] == 0 ? '좌->우':'우->좌';
		$twopage = $row['two_paged'] == 0 ? "1쪽":"2쪽";
?>
	
		<tr onclick="gotoVolume(<?=$row['_id']?>)">
		<td><?=$row['volume_no']?></td>
		<td><?=$pagedir?></td>
		<td><?=$twopage?></td>
		<td><?=$ready?></td>
		<td><?=$row['_id']?></td>
		</tr>
<?php
	}
}
?>
</table>
<br/>
<input type="button" value="권 추가" onclick="addVolume()"/>
<?php
?>

	<script src="/js/jquery2.min.js"></script>
	<script src="/js/bootstrap.min.js"></script>
<script>
function selectAuthor()
{
        window.open('select_author.php', 'select', 'width=400,height=200, resizable=Yes,scrollbars=Yes,toolbar=no,menubar=no,location=no,directories=no,status=No');
}

function gotoVolume(vid)
{
	location.href = "volume_info.php?" + vid;
}

function onSubmit()
{
	var f1 = document.f1;
	var nameKor = f1.name_kor.value;
	var nameOrig = f1.name_orig.value;
	var minLevel = f1.min_level.value;
	var completed = f1.completed.value;

	var authorId = f1.author_id.value;
	if (authorId == '' || authorId == 0)
	{
		authorId = null;
	}

	if (nameKor == '')
	{
		alert('한글 제목을 입력하세요.');
		return;
	}

	if (nameOrig == '')
	{
		alert('원판 제목을 입력하세요.');
		return;
	}

	if (!(minLevel>0 && minLevel<999))
	{
		alert('레벨 값이 올바르지 않습니다.');
		return;
	}


	var json = {
		"name_kor":nameKor,
		"name_orig":nameOrig,
		"author_id":authorId,
		"min_level":minLevel,
		"completed":completed
	};

<?php if ($titleId == 0) { ?>
<?php } else { ?>
	json['ready'] = f1.ready.value;
<?php } ?>

	var url = "<?=$ComicBaseUrl?>/admin/title/<?=$titleId?>";

	$.ajax({
		'url':url,
		type:"POST",
		contentType: "application/json", 
		data:JSON.stringify(json),
		success:function(data) {
			if (data.result == 'OK')
			{
				var titleId = data.data.title_id;
				alert('요청이 성공하였습니다. (titleId='+titleId+')');
				location.href = "title_info.php?" + titleId;
			}
			else
			{
				alert('요청이 실패했습니다: ' + data.error);
			}
		}
	});
}

function addVolume()
{
	if (!confirm('새로운 Volume 을 추가하시겠습니까?'))
	{
		return;
	}

	var json = { "title_id":<?=$titleId?> };
	var url = "<?=$ComicBaseUrl?>/admin/volume/add";

	$.ajax({
		'url':url,
		type:"POST",
		contentType: "application/json", 
		data:JSON.stringify(json),
		success:function(data) {
			if (data.result == 'OK')
			{
				var volumeId = data.data.volume_id;
				//alert('요청이 성공하였습니다. (volumeId='+volumeId+')');
				location.href = "volume_info.php?" + volumeId;
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

