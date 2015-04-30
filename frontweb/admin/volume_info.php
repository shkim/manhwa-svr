<?php 

$volumeId = $_SERVER["QUERY_STRING"];
if (!is_numeric($volumeId))
{
	die("Invalid volume id: " . $volumeId);
}

include("_auth.php"); 

$rs = mysql_query("SELECT * FROM volume WHERE _id=$volumeId");
$row = mysql_fetch_array($rs);
if ($row == null)
{
	die("Volume not found for ID=$titleId");
}

$titleId = $row['title_id'];
$volNo = $row['volume_no'];
$ready = $row['ready'];
$revDir = $row['reverse_dir'];
$twoPaged = $row['two_paged'];
$pages = $row['pages'];

$rs = mysql_query("SELECT name_kor FROM title WHERE _id=$titleId");
$row = mysql_fetch_row($rs);
$titleName = $row[0];

?><!DOCTYPE html>
<html lang="ko">
<head>
	<meta charset="utf-8">
	<title>Volume Info</title>
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link href="/css/bootstrap.min.css" rel="stylesheet" media="screen">
<style>
body { margin:10px }
tr:hover { background-color: #ddddee; }
.ca { text-align:center }

section {
    margin: auto;
}
div#pgtbl {
    width: 300px;
    float: left;
}
div#arcinfo {
    margin-left: 320px;
}

div#maintbl {
	width:350px;
	float:left;
}
div#upload {
    margin-left: 370px;
}
</style>

</head>
<body>

<h3><a href="title_info.php?<?=$titleId?>"><?=$titleName?></a>의 Volume #<?=$volNo?> 정보</h3>

<section>
<div id="maintbl">
<form name="f1">
<table border="1">
<tr>
	<th>ID</th>
	<td><?=$volumeId?></td>
</tr>
<tr>
	<th>권 번호</th>
	<td><?=$volNo?></td>
</tr>
<tr>
	<th>페이지 넘김 방향</th>
	<td>
	<input type="radio" value="0" name="r2l"<?php if($revDir==0) echo " checked"; ?>>좌->우 (한국식) &nbsp;
	<input type="radio" value="1" name="r2l"<?php if($revDir!=0) echo " checked"; ?>>우->좌 (일본식)
	</td>
</tr>
<tr>
	<th>두쪽 여부</th>
	<td>
	<input type="radio" value="0" name="twopage"<?php if($twoPaged==0) echo " checked"; ?>>1쪽씩 스캔 &nbsp;
	<input type="radio" value="1" name="twopage"<?php if($twoPaged!=0) echo " checked"; ?>>2쪽씩 스캔
	</td>
</tr>

<tr>
	<th>서비스 상태</th>
	<td>
	<input type="radio" value="0" name="ready"<?php if($ready==0) echo " checked"; ?>>감춤 &nbsp;
	<input type="radio" value="1" name="ready"<?php if($ready!=0) echo " checked"; ?>>노출(서비스)
	</td>
</tr>

</table>
</br>

<input type="button" value="업데이트" onclick="onUpdateVolume()"/>
</form>
</div>

<div id="upload" class="container">
<div class="row">
    <div class="span7">
	<form method="post" action="/">
	    <legend>Volume ZIP 파일 올리기</legend>
	    <input type="file" name="arcFile" id="arcFile">
	    <p></p>
	    <div class="form-actions">
		<input type="submit" value="Upload" class="btn btn-primary">
	    </div>
	</form>
    </div>
</div>
<hr>
<div class="row">
    <div class="span7">
	<div class="progress progress-striped active hide">
	    <div style="width: 0%" class="bar"></div>
	</div>
    </div>
</div>
<div class="row">
    <div class="span7">
	<div class="alert hide">
	    <button type="button" data-dismiss="alert" class="close">x</button><span><strong class="message"></strong></span>
	</div>
    </div>
</div>
</div>
</section>



<?php
$arrPages = split(' ',$pages);
if (count($arrPages) > 0)
{
	$rs = mysql_query("SELECT * FROM arc_info WHERE volume_id=$volumeId");
	$row = mysql_fetch_array($rs);

	$arcSize = $row['arc_size'];
	$fileCount = $row['file_count'];

	$rawEntries = $row['entries'];
?>
<section>
<div id="pgtbl">
<h3>페이지 목록</h3>
<table border="1">
<tr>
	<th>페이지 번호</th>
	<th>ZIP 파일 인덱스</th>
	<th>파일 포맷</th>
	<th>&nbsp;</th>
</tr>
<?php
	$pgSeq = 0;
	foreach($arrPages as $spec)
	{
		list($pgId, $fmt) = split(':',$spec);
		++$pgSeq;
?>
<tr>
	<td class="ca"><?=$pgSeq?></td>
	<td class="ca"><?=$pgId?></td>
	<td class="ca"><?=$fmt?></td>
	<td><input type="button" value="보기" onclick="showPage(<?=$pgId?>)"/></td>
</tr>
<?php
	}
?>
</table>
</div>

<div id="arcinfo">
<h3>ZIP File info</h3>
ZIP file size: <?=$arcSize?> Bytes<br/>
File entry count in ZIP: <?=$fileCount?><br/>
<table border="1">
<tr>
	<th>Index in ZIP</th>
	<th>File name</th>
	<th>File size</th>
</tr>
<?php
	$entries = json_decode($rawEntries);
	$zipIdx = 0;
	foreach($entries as $entry)
	{
?>
<tr>
	<td class="ca"><?=$zipIdx?></td>
	<td class="ca"><?=$entry[0]?></td>
	<td class="ca"><?=$entry[1]?></td>
</tr>
<?php
		++$zipIdx;
	}
?>
</table>
</div>
</section>
<?php
}
?>

	<script src="/js/jquery2.min.js"></script>
	<script src="/js/bootstrap.min.js"></script>
<script>
function showPage(pid)
{
	var url = 'http://comic.ogp.kr/pic/shkim=handsome/<?=$volumeId?>/'+pid;
	window.open(url, 'showPage', 'resizable=Yes,scrollbars=Yes,toolbar=no,menubar=no,location=no,directories=no,status=No');
}

function onUpdateVolume()
{
	var json = {
		"reverse_dir":document.f1.r2l.value,
		"two_paged":document.f1.twopage.value,
		"ready":document.f1.ready.value,
		"volume_id":<?=$volumeId?>
	};

	$.ajax({
		'url':"<?=$ComicBaseUrl?>/admin/volume/update",
		type:"POST",
		contentType: "application/json",
		data:JSON.stringify(json),
		success:function(data) {
			if (data.result == 'OK')
			{
				var volumeId = data.data.volume_id;
				alert('요청이 성공하였습니다. (volumeId='+volumeId+')');
				location.href = "volume_info.php?" + volumeId;
			}
			else
			{
				alert('요청이 실패했습니다: ' + data.error);
			}
		}
	});
}

function onFillPageSuccess()
{
	$.ajax({
		'url':"<?=$ComicBaseUrl?>/admin/fillVolPages/<?=$volumeId?>",
                type:"GET",
                success:function(ret) {
                        if (ret.result == 'OK')
			{
				if (ret.data.page_count > 0)
                        	{
					alert('업로드 성공');
					location.href = "volume_info.php?<?=$volumeId?>";
				}
				else
				{
					alert("FillPage unknown response: " + JSON.stringify(ret));
				}
                        }
                        else
                        {
                                alert('요청이 실패했습니다: ' + ret.result + ' (' + ret.error.toString() + ')');
                        }
                }
	});
}

function onUploadSuccess()
{
	$.ajax({
		'url':"<?=$ComicBaseUrl?>/admin/fillArcInfo/<?=$volumeId?>",
                type:"GET",
                success:function(ret) {
                        if (ret.result == 'OK')
			{
				if (ret.data.volume_id == <?=$volumeId?>)
                        	{
					onFillPageSuccess();
				}
				else
				{
					alert("FillArc unknown response: " + JSON.stringify(ret));
				}
                        }
                        else
                        {
                                alert('요청이 실패했습니다: ' + ret.result + ' (' + ret.error.toString() + ')');
                        }
                }
	});
}


$(function() {
  
  var showInfo = function(message) {
    $('div.progress').hide();
    $('strong.message').text(message);
    $('div.alert').show();
  };
  
  $('input[type="submit"]').on('click', function(evt) {
    evt.preventDefault();
    $('div.progress div.bar').css('width', '0%');
    $('div.progress').show();
    var formData = new FormData();
    var file = document.getElementById('arcFile').files[0];
    formData.append('arcFile', file);
    
    var xhr = new XMLHttpRequest();
    
    xhr.open('POST', '<?=$ComicBaseUrl?>/admin/volume/upload/<?=$volumeId?>', true);
    //xhr.open('post', '/a/admin/volume/upload/<?=$volumeId?>', true);
    
    xhr.upload.onprogress = function(e) {
      if (e.lengthComputable) {
        var percentage = (e.loaded / e.total) * 100;
        $('div.progress div.bar').css('width', percentage + '%');
      }
    };
    
    xhr.onerror = function(e) {
      showInfo('Upload error occurred: ' + e);
    };
    
    xhr.onload = function() {
      showInfo(this.statusText);
	if (this.statusText == 'OK')
	{
    		$('div.alert').hide();

		var json = JSON.parse(this.responseText);
		if (json.result != 'OK')
		{
			alert(json.error);
		}
		else if(json.data.volume_id == <?=$volumeId?>)
		{
			onUploadSuccess();
		}
		else
		{
			alert("Upload unknown response: " + JSON.stringify(json.data));
		}
	}
    };
    
    xhr.send(formData);
    
  });
  
});






</script>
</body>
</html>

