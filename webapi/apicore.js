'use strict';
// Manwha API implementation file
var mysql = require('mysql');
var crypto = require('crypto');
var redis = require("redis");
var async = require('async');
var util = require('util');
var path = require('path');
var fs = require('fs');
var request = require('request');
var charsetDetector = require("node-icu-charset-detector");
var Iconv = require("iconv").Iconv;
var unzip = require("unzip");
var myutil = require('./myutil');


var ERRCODE_DBFAIL = "DB_ERROR";
var ERRCODE_SYSFAIL = "SYS_FAIL";
var ERRCODE_NO_UDID = "NO_UDID";
var ERRCODE_INVALID_ARGS = "INVALID_ARGS";
var ERRCODE_INVALID_PASSWD = "DIFF_PASSWD";
var ERRCODE_INVALID_SESSION = "INVALID_SESSION";
var ERRCODE_INVALID_ARC = "INVALID_FILE";
var ERRCODE_TITLE_DUPLICATE = "DUPLICATED";
var ERRCODE_TIMEOUT = "TIMEOUT";
var ERRCODE_USER_NOTFOUND = "NO_USER";
var ERRCODE_USER_INVALIDID = "INVALID_USER";

var ACCLOG_TYPE_LOGIN = 1;
var ACCLOG_TYPE_VIEW = 2;
var ACCLOG_TYPE_UPLOAD = 3;

var dbConfig;
var db;
var redisClient;
var urlGetArcInfo;
var arcDataDir;

function handleDisconnect()
{
	db = mysql.createConnection(dbConfig);

	db.connect(function(err) {
		if(err) {
			console.log('Error when connecting to db:', err);
			setTimeout(handleDisconnect, 2000);
		}
	});

	db.on('error', function(err) {
		if(err.code === 'PROTOCOL_CONNECTION_LOST') {
			handleDisconnect();
		} else {
			console.log('DB error', err);
			throw err;
		}
	});

}


exports.init = function(config, initDoneCallback)
{
	urlGetArcInfo = config.server.picsvr + '/admin/GetFileList/';
	arcDataDir = config.server.datadir;

	// MySQL
	dbConfig = config.mysql;
	handleDisconnect();

	// Redis
	redisClient = redis.createClient(config.redis.port, config.redis.host);
	redisClient.auth(config.redis.auth, function (err) {
		if (err)
		{
			console.error("Redis auth failure.");
			initDoneCallback(err);
			return;
		}

		onCoreInit(initDoneCallback);
	});

	redisClient.on("error", function(err) {
		console.error("Redis Error: " + err);
	});
}

var verAndroid = null;
var verIos = null;
var API_VERSION = '0.1';

function onCoreInit(initDoneCallback)
{
	async.series([
		function(next) {
			db.query("SELECT value FROM sysinfo WHERE name=?", ['appver_ios'], function(err,rows) {
				if (err)
				{
					next(err);
					return;
				}

				if (rows.length == 0)
				{
					next("iOS AppVersion data not found in DB");
					return;
				}

				verIos = rows[0].value;
				next(null);
			});
		},
		function(next) {
			db.query("SELECT value FROM sysinfo WHERE name=?", ['appver_android'], function(err,rows) {
				if (err)
				{
					next(err);
					return;
				}

				if (rows.length == 0)
				{
					next("Android AppVersion data not found in DB");
					return;
				}

				verAndroid = rows[0].value;
				next(null);
			});
		}
	], function(err,results) {
		if (err)
		{
			initDoneCallback(err);
			return;
		}

		initDoneCallback(null);
	});
}

function apiError(errCode, errMsg)
{
	if (typeof errMsg != 'string')
	{
		errMsg = JSON.stringify(errMsg);
	}

	return {
		"result":errCode,
		"error":errMsg
	};
}

function apiOK(data)
{
	return {
		"result":"OK",
		"data":data
	};
}

function accessLog(type, userId, volumeId)
{
	db.query("INSERT INTO access_log VALUES(NULL, ?, now(), ?, ?)", [type, userId, volumeId], function(err,res) {
		if (err)
		{
			console.error("AccessLog(%d,%d,%d) failed: %s", type, userId, volumeId, err);
		}
	});
}

exports.hello = function(res, params)
{
	var target = params.target;

	res.send(apiOK("Hello, " + target));
}

/*
function sendSysInfo(res, oldValue, jsonKey, dbKey)
{
	var json = {};

	if (oldValue != null)
	{
		json[jsonKey] = oldValue;
		res.send(apiOK(json));
		return;
	}

	console.log("Fetch SysInfo for '%s' ...", dbKey);
	db.query("SELECT value FROM sysinfo WHERE name=?", [dbKey], function(err,rows) {
		if (err)
		{
			console.log("SysInfo '%s' failed: %s", dbKey, err['code']);
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		if (rows.length == 0)
		{
			console.log("DataError: SysInfo '%s' not found.", dbKey);
			res.send(apiError(ERRCODE_SYSFAIL, 'Data Integrity error'));
			return;
		}

		oldValue = rows[0].value;

		if (dbKey == 'appver_android')
		{
			verAndroid = oldValue;
		}
		else if (dbKey == 'appver_ios')
		{
			verIos = oldValue;
		}

		json[jsonKey] = oldValue;
		res.send(apiOK(json));
	});
}
*/

exports.versionAndroid = function(res, params)
{
	res.send(apiOK({'app':verAndroid, 'api':API_VERSION}));
}

exports.versionIos = function(res, params)
{
	res.send(apiOK({'app':verIos, 'api':API_VERSION}));
}

function makeAndSendSession(res, userId, level)
{
	accessLog(ACCLOG_TYPE_LOGIN, userId, 0);

	var usn = 'S' + userId;
	var sessKey = crypto.randomBytes(16).toString('hex');

	async.series([
		function(next) {
			redisClient.get(usn, function(err, oldSessKey) {
				if (err)
				{
					console.error("Sess RedisError1: " + err);
					next(err);
					return;
				}

				if (oldSessKey != null)
				{
					redisClient.del(oldSessKey, function(err, reply) {
						//console.log("(del)sess1.5 reply=" + reply);
					});
				}

				next(null);
			});
		},
		function(next) {
			redisClient.set(usn, sessKey, function(err,reply) {
				if (err)
				{
					console.error("Sess RedisError2: " + err);
					next(err);
					return;
				}

				if (reply != 'OK')
				{
					console.error("Redis Session set(%s) failed: %s", usn, reply);
					next("Session name save fail");
					return;
				}

				next(null);
			});
		},
		function(next) {
			redisClient.hmset(sessKey, {'userId':userId, 'level':level}, function(err,reply) {
				if (err)
				{
					console.error("Sess RedisError3: " + err);
					next(err);
					return;
				}

				if (reply != 'OK')
				{
					console.error("Redis Session hmset(%s,%s) failed: %s", usn, sessKey, reply);
					next("Session data save fail");
					return;
				}

				next(null);
			});
		}
	], function(err) {
		if (err)
		{
			res.send(apiError(ERRCODE_SYSFAIL, err));
			return;
		}

		//console.log("Session %s: %s", usn, sessKey);
		res.send(apiOK({'sess_key':sessKey, 'level':level}));
	});
}

exports.lookupSession = function(res, sessKey, callback)
{
	redisClient.hgetall(sessKey, function(err, reply) {
		if (err)
		{
			console.log("SessLookupError: " + reply);
			res.send(apiError(ERRCODE_SYSFAIL, err));
			return;
		}

		if (reply == null)
		{
			res.send(apiError(ERRCODE_INVALID_SESSION, "Invalid session"));
			return;
		}

		console.log("lookupSession %s: %d,%d", sessKey, reply.userId, reply.level);
		callback(reply);
	});
}

exports.loginFacebook = function(res, emailEnc, fbObjId)
{
	var email = myutil.decryptAES(emailEnc);
	if (email.indexOf('@') < 0)
	{
		res.send(apiError(ERRCODE_SYSFAIL, "Decoding failure"));
		return;
	}

	db.query("SELECT _id, level, fb_obj_id FROM user WHERE email=?", [email], function(err,rows) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		if (rows.length == 0)
		{
			res.send(apiError(ERRCODE_USER_NOTFOUND, 'User not found'));
			return;
		}

		if (rows[0].fb_obj_id != fbObjId)
		{
			console.info("Login UserReqId=%s, DbId=%s", fbObjId, rows[0].fb_obj_id);
			res.send(apiError(ERRCODE_USER_INVALIDID, 'User ID mismatch'));
			return;
		}

		makeAndSendSession(res, rows[0]._id, rows[0].level);
	});
}

exports.registerFacebookUser = function(res, emailEnc, fbObjId, name, profileUrlEnc)
{
	var initialLevel = 1;

	var email = myutil.decryptAES(emailEnc);
	var profileUrl = myutil.decryptAES(profileUrlEnc);
	if (email.indexOf('@') < 0 || profileUrl.indexOf('http') != 0)
	{
		res.send(apiError(ERRCODE_SYSFAIL, "Decoding failure"));
		return;
	}

	db.query("INSERT INTO user (level,member_since,last_visit,name,email,fb_obj_id,profile_url) VALUES(?, now(), now(), ?,?,?,?)", [initialLevel, name, email, fbObjId, profileUrl], function(err,dbRes) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		makeAndSendSession(res, dbRes.insertId, initialLevel);
	});
}

exports.dirListTop = function(res, userLevel)
{
	var sql = "SELECT _id, name FROM dir_folder WHERE parent_id IS NULL AND "
		+ (userLevel <= 1 ? "min_level=?" : "min_level>1 && min_level<=? ORDER BY seq");

	db.query(sql, [userLevel], function(err,rows) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		res.send(apiOK({'dirs':rows}));
	});
}

exports.dirList = function(res, userLevel, folderId)
{
	async.series({
		'dirs':function(callback) {
			db.query("SELECT _id, name FROM dir_folder WHERE parent_id=? AND min_level<=? ORDER BY seq", [folderId, userLevel], function(err,rows) {
				if (err)
				{
					callback(err, null);
				}
				else
				{
					callback(null, rows);
				}
			});
		},
		'titles':function(callback) {
			db.query("SELECT title_id, B.name_kor, completed, C.name_kor author_name FROM dir_item A " +
			"INNER JOIN title B ON A.title_id = B._id " +
			"LEFT JOIN author C ON B.author_id=C._id " +
			"WHERE A.folder_id=? and A.min_level<=? and B.ready=1 ORDER BY A.seq", [folderId, userLevel], function(err,rows) {
				if (err)
				{
					callback(err, null);
				}
				else
				{
					callback(null, rows);
				}
			});
		}
	}, function(err,results) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		res.send(apiOK(results));
	});

}

exports.titleInfo = function(res, userLevel, titleId)
{
	async.series({
		'info':function(callback) {
			db.query("SELECT A.name_kor, A.name_orig, completed, B.name_kor author_name, B._id author_id FROM title A " +
			"LEFT JOIN author B ON A.author_id=B._id " +
			"WHERE A._id=? AND min_level<=? AND ready=1", [titleId, userLevel], function(err,rows) {
				if (err)
				{
					callback(err, null);
				}
				else
				{
					callback(null, rows[0]);
				}
			});
		},
		'volumes':function(callback) {
			db.query("SELECT _id, volume_no FROM volume WHERE title_id=? and ready=1 ORDER BY volume_no", [titleId, userLevel], function(err,rows) {
				if (err)
				{
					callback(err, null);
				}
				else
				{
					var vols = '';
					for (var r=0; r<rows.length; r++)
					{
						if (r > 0) vols += " ";

						var row = rows[r];
						vols += row['volume_no'] +':'+ row['_id'];
					}

					callback(null, vols);
				}
			});
		}
	}, function(err,results) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		res.send(apiOK(results));
	});
}

exports.volumeInfo = function(res, volumeId)
{
	db.query("SELECT reverse_dir, two_paged, pages FROM volume WHERE _id=?", [volumeId], function(err,rows) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		if (rows.length == 0)
		{
			res.send(apiError(ERRCODE_INVALID_ARGS, 'Volume not found'));
			return;
		}

		res.send(apiOK(rows[0]));
	});
}

exports.checkDRM = function(res, sessKey, volumeId)
{
	if (sessKey == 'shkimRULEZ')
	{
		res.send("OK");
		return;
	}

	redisClient.hget(sessKey, 'level', function(err, reply) {
                if (err)
                {
                        console.error("checkDRM redis error: " + err);
                        res.send("NO");
                        return;
                }

                if (reply == null)
                {
                        res.send("NO");
                        return;
                }

		// TODO: volume check

		res.send("OK");
        });
}

function buff2str(buff, charset)
{
	try {
		return buff.toString(charset);
	}
	catch (x)
	{
		var charsetConverter = new Iconv(charset, "utf8");
		return charsetConverter.convert(buff).toString();
	}
}

exports.fillArcInfo = function(res, volumeId)
{
	var reqUrl = urlGetArcInfo + volumeId;
	console.log("FillArc url: " + reqUrl);
	request.get({url:reqUrl, encoding:null}, function(err, res2, bodyBuff) {
	        if (!err && res2.statusCode == 200)
	        {
			var charset = charsetDetector.detectCharset(bodyBuff).toString();
// detection bug?
if (charset == 'ISO-8859-2') charset = 'EUC-KR';
			var rawJson = buff2str(bodyBuff, charset);
//console.log("Volume#%d, charset=%s\n%s", volumeId, charset, rawJson);
			var json = JSON.parse(rawJson);

			var arcFileSize = json['file_size'];
			var arcEntryCnt = json['entries'].length;
			var strEntries = JSON.stringify(json['entries']);

			db.query("REPLACE INTO arc_info (volume_id, arc_size, file_count, entries) VALUES(?,?,?,?)", [
				volumeId, arcFileSize, arcEntryCnt, strEntries], function(err, dbres) {
				if (err)
                		{
					res.send(apiError(ERRCODE_DBFAIL, err));
					return;
		                }

				res.send(apiOK({'volume_id':volumeId, 'file_count':arcEntryCnt}));
			}); 
		}
		else
		{
                        res.send(apiError(ERRCODE_SYSFAIL, err));
		}
	});
}

exports.fillVolumePages = function(res, volumeId)
{
	db.query("SELECT file_count, entries FROM arc_info WHERE volume_id=?", [volumeId], function(err, rows) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		if (rows.length == 0)
                {
                        res.send(apiError(ERRCODE_INVALID_ARGS, 'Volume not found: ' + volumeId));
                        return;
                }

                var fileCount = rows[0].file_count;
		var entries = JSON.parse(rows[0].entries);

		var n2e = {};
		for (var i=0; i<entries.length; i++)
		{
			var entry = entries[i];
			var fname = entry[0];
			var dotpos = fname.lastIndexOf('.');
			if (dotpos < 0)
				continue;

			var ftype;
			switch(fname.substring(dotpos+1).toUpperCase())
			{
			case 'JPG':
			case 'JPEG':
				ftype = 'J';
				break;
			case 'PNG':
				ftype = 'P';
				break;
			case 'GIF':
				ftype = 'G';
				break;
			case 'WEBP':
				ftype = 'W';
				break;
			default:
				ftype = null;
			}

			if (ftype == null)
				continue;

			n2e[fname] = [ i, ftype ];
		}

		var sortedKeys = Object.keys(n2e).sort();
		var pages = '';
		for (var s=0; s<sortedKeys.length; s++)
		{
			if (s>0) pages += ' ';

			var sen = n2e[sortedKeys[s]];
			pages += sen[0] +':'+ sen[1];
		}

		db.query("UPDATE volume SET pages=?, ready=1 WHERE _id=?", [pages, volumeId], function(err, dbres) {
			if (err)
			{
				res.send(apiError(ERRCODE_DBFAIL, err));
				return;
			}

		
			res.send(apiOK({'page_count':sortedKeys.length}));
		});
	});
}

exports.getMaxVolumeId = function(res)
{
	db.query("SELECT MAX(_id) AS maxId FROM volume", function(err, rows) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		var maxId = (rows.length > 0) ? rows[0].maxId : 0;

		res.send(apiOK({'max_id':maxId}));
	});
}

exports.insertNewTitle = function(res, data)
{
	db.query("INSERT INTO title (reg_date,volume_cnt,ready, completed,min_level,name_kor,name_orig) VALUES(now(),0,0, ?,?,?,?)", [data.completed, data.min_level, data.name_kor, data.name_orig], function(err, dbRes) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		res.send(apiOK({'title_id':dbRes.insertId}));
	});
}

exports.updateTitle = function(res, titleId, data)
{
	db.query("UPDATE title SET ready=?, completed=?, min_level=?, author_id=?, name_kor=?, name_orig=? WHERE _id=?", [data.ready, data.completed, data.min_level, data.author_id, data.name_kor, data.name_orig, titleId], function(err, dbRes) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		res.send(apiOK({'title_id':titleId}));
	});
}

exports.insertNewAuthor = function(res, data)
{
	db.query("INSERT INTO author (birth_year,name_kor,name_orig) VALUES(?,?,?)", [data.birth_year, data.name_kor, data.name_orig], function(err, dbRes) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		res.send(apiOK({'author_id':dbRes.insertId}));
	});
}

exports.updateAuthor = function(res, authorId, data)
{
	db.query("UPDATE author SET birth_year=?, name_kor=?, name_orig=? WHERE _id=?", [data.birth_year, data.name_kor, data.name_orig, authorId], function(err, dbRes) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		res.send(apiOK({'author_id':authorId}));
	});
}

exports.updateVolume = function(res, data)
{
	db.query("UPDATE volume SET ready=?, reverse_dir=?, two_paged=? WHERE _id=?", [data.ready, data.reverse_dir, data.two_paged, data.volume_id], function(err, dbRes) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		res.send(apiOK({'volume_id':data.volume_id}));
	});
}

exports.addTitleVolume = function(res, titleId)
{
	db.query("SELECT completed FROM title WHERE _id=?", [titleId], function(err, rows) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		if (rows.length == 0)
                {
                        res.send(apiError(ERRCODE_INVALID_ARGS, 'Title not found: ' + titleId));
                        return;
                }

		if (rows[0].completed)
		{
                        res.send(apiError(ERRCODE_INVALID_ARGS, 'Title is locked(completed): ' + titleId));
                        return;
		}

		db.query("SELECT IFNULL(MAX(volume_no),0)+1 maxid FROM volume WHERE title_id=?", [titleId], function(err2, r2) {
			if (err2)
			{
				res.send(apiError(ERRCODE_DBFAIL, err2));
				return;
			}

			if (r2.length == 0 || r2[0].maxid <= 0)
			{
				res.send(apiError(ERRCODE_INVALID_ARGS, 'Max volume number failed: ' + titleId));
				return;
			}

			db.query("INSERT INTO volume (reg_date, title_id, volume_no, ready, reverse_dir, two_paged, pages) VALUES(now(),?,?,0,1,1,'')", [titleId, r2[0].maxid], function(err3, dbRes) {
				if (err3)
				{
					res.send(apiError(ERRCODE_DBFAIL, err3));
					return;
				}

				db.query("UPDATE title SET volume_cnt=(SELECT COUNT(*) FROM volume WHERE title_id=?) WHERE _id=?", [titleId, titleId]);

				res.send(apiOK({'volume_id':dbRes.insertId}));
			});
		});

	});
}

exports.uploadVolumeArc = function(res, volumeId, uploadFilePath)
{
	accessLog(ACCLOG_TYPE_UPLOAD, 0, volumeId);

	fs.createReadStream(uploadFilePath)
		.pipe(unzip.Parse())
/*
		.on('entry', function (entry) {
			entry.autodrain();
		})
*/
		.on('error', function (err) {
			console.error("Uploaded volume zip error: " + err);
			fs.unlink(uploadFilePath);
			res.send(apiError(ERRCODE_INVALID_ARC, err.toString()));
		})
		.on('close', function () {
			var voldir = Math.round(volumeId / 1000);
			var destVolPath = path.join(arcDataDir, voldir.toString(), volumeId + ".zip");
			if (fs.existsSync(destVolPath))
			{
				console.log("Delete old arc file: " + destVolPath);
				fs.unlinkSync(destVolPath);
			}
			fs.renameSync(uploadFilePath, destVolPath);
			console.log("Volume uploaded: %s (%s)", destVolPath, uploadFilePath);
			res.send(apiOK({'volume_id':volumeId}));
		});
}

exports.addDirFolder = function(res, data)
{
	var parentId = data.parent_id;

	db.query("SELECT IFNULL(MAX(seq),0)+1 nextseq FROM dir_folder WHERE parent_id=?", [parentId], function(err, rows) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		if (rows.length == 0 || rows[0].nextseq <= 0)
		{
			res.send(apiError(ERRCODE_INVALID_ARGS, 'Next seq number failed: ' + parentId));
			return;
		}

		var nextSeq = rows[0].nextseq;
		db.query("INSERT INTO dir_folder (parent_id, seq, min_level, name) VALUES(?,?,?,?)", [parentId, nextSeq, data.min_level, data.name], function(err, dbRes) {
			if (err)
			{
				res.send(apiError(ERRCODE_DBFAIL, err));
				return;
			}
		
			res.send(apiOK({'folder_id':dbRes.insertId}));
		});

	});
}

exports.addDirItem = function(res, data)
{
	db.query("SELECT COUNT(*) cnt FROM dir_item WHERE folder_id=? AND title_id=?", [data.folder_id, data.title_id], function(err,rows) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		if (rows.length != 1 || rows[0].cnt != 0)
		{
			res.send(apiError(ERRCODE_TITLE_DUPLICATE, 'Already has title #' + data.title_id));
			return;
		}

	db.query("SELECT IFNULL(MAX(seq),0)+1 nextseq FROM dir_item WHERE folder_id=?", [data.folder_id], function(err, r2) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		var nextSeq = r2[0].nextseq;
		
	db.query("INSERT INTO dir_item (folder_id, seq, min_level, title_id) VALUES(?,?,?,?)", [data.folder_id, nextSeq, data.min_level, data.title_id], function(err, dbRes) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		res.send(apiOK({'folder_id':data.folder_id, 'item_id':dbRes.insertId}));

	});	// insert
	});	// get nextseq
	});	// dup check
}

exports.logEvent = function(res, json, ipaddr)
{
	var runtype = json.runtype;
	var action = json.action;
	var param = json.param;
	var logmsg = json.msg;

	db.query("INSERT INTO rt_log (logtime,runtype,ipaddr,action,param) VALUES(now(),?,?,?,?)", [runtype,ipaddr,action,param], function(err,dbres) {
		if (err)
		{
			res.send(apiError(ERRCODE_DBFAIL, err));
			return;
		}

		if (logmsg != undefined && logmsg != null)
		{
			db.query("INSERT INTO rt_msg VALUES(?,?)", [dbres.insertId, logmsg], function(err, dbres2) {
				if (err)
				{
					res.send(apiError(ERRCODE_DBFAIL, err));
				}
				else
				{
					res.send(apiOK({'logId':dbres.insertId}));
				}
			});
		}
		else
		{
			res.send(apiOK({'logId':dbres.insertId}));
		}
	});
}


