var crypto = require('crypto');

// $ openssl enc -aes-256-cbc -k secret -P -md sha1
// salt=2B7186CC2D86EDB0
// key=ED94C926FC4C9690CB889E4D6A24DDDA3C6D3B3292D37035F79E4FED9117CC69
// iv =AE456157ED8F2ED5524450CA9774643D
var aes_key = new Buffer('ED94C926FC4C9690CB889E4D6A24DDDA3C6D3B3292D37035F79E4FED9117CC69', 'hex');
var aes_iv = new Buffer('AE456157ED8F2ED5524450CA9774643D', 'hex');

exports.encryptAES = function(plain)
{
	var p = String(plain);
	while(p.length%32!==0)
		p+=' ';

	var cipher = crypto.createCipheriv('aes-256-cbc', aes_key, aes_iv);
	return cipher.update(p, 'utf8', 'hex') + cipher.final('hex');
}

exports.decryptAES = function(hexStr)
{
	try
	{
		var decipher = crypto.createDecipheriv('aes-256-cbc', aes_key, aes_iv);
		var ret = decipher.update(hexStr, 'hex', 'utf8') + decipher.final('utf8');
		return ret.replace(/ +$/g,'');
	}
	catch(ex)
	{
		console.error("Decrypt fail: " + hexStr);
		return nil;
	}
}

exports.hashSHA1 = function(key, plain)
{
	var ret = crypto.createHmac('sha1', key).update(plain).digest('base64')
	return ret;
}

