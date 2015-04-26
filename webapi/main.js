'use strict';
// Manhwa API server

var express = require('express');
var morgan  = require('morgan');
var cookieParser = require('cookie-parser');
var errorHandler = require('express-error-handler');
var methodOverride = require('method-override');
//var session = require('express-session');
var bodyParser = require('body-parser');
var multer = require('multer'); 
var swig = require('swig');
var async = require('async');
var util = require('util');
var http = require('http');
var path = require('path');
var fs = require('fs');
//var zlib = require('zlib');
var apicore = require('./apicore');

// Global Exception Handler
process.on('uncaughtException', function (err) {
	util.log('[Exception?]___________report_start');
	util.log('[Stack]\n'+err.stack);
	util.log('[Arguments] : '+err.arguments);
	util.log('[Type] : '+err.type);
	util.log('[Message] : '+err.message);
	util.log('[Exception!]___________report_end');
});


var config;
try
{
	var fname = null;
	for (var i = 2; i < process.argv.length; i++)
	{
		var arg = process.argv[i];
		if (arg.indexOf('--') != 0)
		{
			fname = arg;
			break;
		}
	}

	if (fname == null)
	{
		console.error("Config file not specified.");
		return;
	}

	fname = process.cwd() + '/' + fname;
	console.log("Using config file: %s", fname);

	//config = JSON.parse(fs.readFileSync(fname, 'utf8'));
	config = require(fname);
	if (typeof config.server == 'undefined')
	{
		console.error("Invalid config file: %s", fname);
		return;
	}
}
catch (err)
{
	console.error("Parsing failed: " + err);
	return;
}

apicore.init(config, onReadyToListen);

var app = express();

// Configuration
app.set('title', 'SleekManhwa API Server');
app.set('port', config.server.port);

app.engine('.html', swig.renderFile)
app.set('view engine', 'html');
app.set('views', path.join(__dirname, '/views'));
app.set('view options', {layout: false});
app.set('trust proxy', true);

app.use(methodOverride());
//app.use(session({ resave: true, saveUninitialized: true, secret: '@_@(^_^)' }));
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));
app.use(cookieParser());

// see https://github.com/expressjs/multer
app.use(multer({
	dest: config.server.datadir,
	limits: { fileSize:999900000 },
	onFileSizeLimit: function (file) {
		console.log('Upload size limit exceeded: %s (%s)', file.originalname, file.path)
		fs.unlink(file.path);
	}
}));

app.use(express.static(path.join(__dirname, 'public')));

if ('development' == app.get('env'))
{
	console.log("App: DevConfig");
	swig.setDefaults({ cache: false });
	app.set('view cache', false);
	app.use(morgan('dev'));
	app.use(errorHandler());
}

if ('production' == app.get('env'))
{
	console.log("App: ProductionConfig");
	app.use(morgan('combined'));
}




var prefix = "/a";

app.get(prefix+'/v1/hello', function(req,res) {
	var ipaddr = "RealIp:"+req.headers['x-real-ip'] +",ForwardIp:"+req.headers['x-forwarded-for']+",connIp:"+ req.connection.remoteAddress+",reqIp:"+req.ip;
	apicore.hello(res, {
		target:ipaddr
	});
});

app.post(prefix+'/v1/echo', function(req,res) {
	console.log(req.body);
	res.send(req.body);
});

app.post(prefix+'/v1/user/login', function(req,res) {
	apicore.loginFacebook(res, req.body.email, req.body.fbid);
});

app.post(prefix+'/v1/user/register', function(req,res) {
	apicore.registerFacebookUser(res, req.body.email, req.body.fbid, req.body.name, req.body.url);
});

app.post(prefix+'/v1/version/android', function(req,res) {
	apicore.versionAndroid(res, req);
});

app.post(prefix+'/v1/version/ios', function(req,res) {
	apicore.versionIos(res, req);
});

/*
app.get(prefix+'/v1/directory', function(req,res) {
	apicore.dirListTop(res);
});

app.get(prefix+'/v1/directory/:fid', function(req,res) {
	apicore.dirList(res, req.params.fid);
});
*/

app.get(prefix+'/v1/title/:tid', function(req,res) {
	apicore.lookupSession(res, req.query.sid, function(session) {
		apicore.titleInfo(res, session.level, req.params.tid);
	});
});

app.get(prefix+'/v1/volume/:vid', function(req,res) {
	apicore.lookupSession(res, req.query.sid, function(session) {
		apicore.volumeInfo(res, req.params.vid);
	});
});

app.post(prefix+'/v1/directory', function(req,res) {
	var fid = req.body.fid;
	apicore.lookupSession(res, req.body.sid, function(session) {
		if (fid > 0)
			apicore.dirList(res, session.level, fid);
		else
			apicore.dirListTop(res, session.level);
	});
});

app.post(prefix+'/v1/title', function(req,res) {
	apicore.lookupSession(res, req.body.sid, function(session) {
		apicore.titleInfo(res, session.level, req.body.tid);
	});
});

app.post(prefix+'/v1/volume', function(req,res) {
	apicore.lookupSession(res, req.body.sid, function(session) {
		apicore.volumeInfo(res, req.body.vid);
	});
});

// ADMIN functions ==>

app.get(prefix+'/v1/drm/:sid/:vid', function(req, res) {
	apicore.checkDRM(res, req.params.sid, req.params.vid);
});

app.get(prefix+'/admin/getMaxVolume', function(req, res) {
	apicore.getMaxVolumeId(res);
});

app.get(prefix+'/admin/fillArcInfo/:vid', function(req, res) {
	apicore.fillArcInfo(res, req.params.vid);
});

app.get(prefix+'/admin/fillVolPages/:vid', function(req, res) {
	apicore.fillVolumePages(res, req.params.vid);
});

app.post(prefix+'/admin/title/:tid', function(req, res) {
	if (req.params.tid == 0)
	{
		apicore.insertNewTitle(res, req.body);
	}
	else
	{
		apicore.updateTitle(res, req.params.tid, req.body);
	}
});

app.post(prefix+'/admin/author/:aid', function(req, res) {
	if (req.params.aid == 0)
	{
		apicore.insertNewAuthor(res, req.body);
	}
	else
	{
		apicore.updateAuthor(res, req.params.aid, req.body);
	}
});

app.post(prefix+'/admin/volume/add', function(req, res) {
	apicore.addTitleVolume(res, req.body.title_id);
});

app.post(prefix+'/admin/volume/update', function(req, res) {
	apicore.updateVolume(res, req.body);
});


app.post(prefix+'/admin/volume/upload/:vid', function(req,res) {
	if (req.params.vid > 0)
	{
		apicore.uploadVolumeArc(res, req.params.vid, req.files.arcFile.path);
	}
	else
	{
		res.end();
	}
});

app.post(prefix+'/admin/dir_folder/add', function(req, res) {
	apicore.addDirFolder(res, req.body);
});

app.post(prefix+'/admin/dir_item/add', function(req, res) {
	apicore.addDirItem(res, req.body);
});

/*
app.post(prefix+'/v1/upload/zlog', function(req,res) {
	var unz = zlib.createInflate();
	req.pipe(unz);

	var chunks = [];
	unz.on('data', function(chunk) {
		chunks.push(chunk);
	});

	unz.on('end', function() {
		var body = Buffer.concat(chunks);
		console.log("len=" + body.length);
		apicore.logEvent(res, JSON.parse(body.toString()), req.ip);
	});
});

app.post(prefix+'/v1/log', function(req,res) {
	apicore.logEvent(res, req.body, req.ip);
});

app.post(prefix+'/v1/echoFile', function(req,res) {
	req.pipe(zlib.createInflate()).pipe(fs.createWriteStream(__dirname+"/rawEcho"));
	res.send("echo?");
});


app.get('/start/tab1', function(req,res) {
	res.render('start_tab1', { sid: req.query.sid });
});

app.post('/api/v1/register/android', function(req,res) {
	apicore.registerPushToken(res, {
		osid: 1,
		osver: req.body.osver,
		uuid: req.body.uuid,
		token: req.body.token
	});
});
*/

//////////////////////////////////////////////////////////////////////////////

function onReadyToListen(error)
{
	if (error)
	{
		console.error("Failed to init Manwha Web API server: " + error);
		return;
	}

	var server = http.createServer(app);
	server.listen(config.server.port, function() {
	      console.log("Manhwa API Server listening on port %d in %s mode", server.address().port, app.settings.env);
	});
}

