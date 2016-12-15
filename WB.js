// Waterbed stream listener using PngPagic (http://www.curioustech.net/pngmagic.html)
wbIP = '192.168.0.104'
Url = 'http://' + wbIP + ':82/events'
var event
var last

heartbeat = (new Date()).valueOf()

if(!Http.Connected)
{
	Http.Connect( 'event', Url )  // Start the event stream
}

Pm.SetTimer(60*1000)

// Handle published events
function OnCall(msg, event, data)
{
	switch(msg)
	{
		case 'HTTPDATA':
			heartbeat = new Date()
			if(data.length <= 2) break // keep-alive heartbeat
			lines = data.split('\n')
			for(i = 0; i < lines.length; i++)
				procLine(lines[i])
			break
		case 'HTTPCLOSE':
			Http.Connect( 'event', Url )  // Start the event stream
			break
		case 'HTTPSTATUS':
			Pm.Echo('WB HTTP status ' + event)
			break
	}
}

function procLine(data)
{
	data = data.replace(/\n|\r/g, "")
	if(data.length < 2) return
	if(data == ':ok' )
	{
		Pm.Echo( 'WB stream started')
		return
	}

	if( data.indexOf( 'event' ) >= 0 )
	{
		event = data.substring( data.indexOf(':') + 2)
		return
	}
	else if( data.indexOf( 'data' ) >= 0 )
	{
		data = data.substring( data.indexOf(':') + 2)
	}
	else
	{
		return // headers
	}

	switch(event)
	{
		case 'state':
			LogWB(data)
			if(Pm.FindWindow('Waterbed'))
				Pm.Waterbed('REFRESH')
			break
		case 'alert':
			Pm.Echo('WB Alert: ' + data)
			Pm.Beep(0)
			break
		case 'print':
			Pm.Echo('WB ' + data)
			break
		case 'request':
			dumpReq(data)
			break
		case 'hack':
			hackJson = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
				data.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + data + ')')
			Pm.Echo('WB Hack: ' + hackJson.ip + ' ' + hackJson.pass)
			break
	}
	event = ''
}

function OnTimer()
{
	time = (new Date()).valueOf()
	if(time - heartbeat > 120*1000)
	{
		Pm.Echo('WB timeout' )
		if(!Http.Connected)
			Http.Connect( 'event', Url )  // Start the event stream
	}
}

function 	LogWB(str)
{
	wbJson = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
		str.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + str + ')')

	line = wbJson.waterTemp + ',' + wbJson.hiTemp + ',' + wbJson.loTemp + ',' + wbJson.on+ ',' + wbJson.temp

	if(+wbJson.temp == 6553.5 || +wbJson.temp == 0)
		return

	if(line == last)
		return
	last = line

	fso = new ActiveXObject( 'Scripting.FileSystemObject' )
	tf = fso.OpenTextFile( 'Waterbed.log', 8, true)
	tf.WriteLine( wbJson.t + ',' + line + ',' + wbJson.rh)
	tf.Close()
	fso = null
}

function dumpReq(data)
{
	Json = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
		data.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + data + ')')

	Pm.Echo('WB Req: R=' + Json.remote + ' H=' + Json.host)
	Pm.Echo('  URI=' + Json.url)
//	if(Json.header)
//	 for(i = 0; i < Json.header.length; i++)
//		Pm.Echo('  Header ' + i + ': ' +Json.header[i])
	if(Json.params)
	 for(i = 0; i < Json.params.length; i++)
		Pm.Echo('  Param ' + i + ': ' +Json.params[i])

	ip = Json.host
	if(ip.search('.com')) // reduce to just IP
	{
		ip = ip.replace( /[a-z.]/g, '' )
		ip = ip.replace( /[-]/g, '.' )
		ip = ip.slice(1)
	}

	if( ValidateIPaddress(ip) && Reg.localIP != ip && ip != wbIP)
	{
		Reg.localIP = ip
		Pm.Echo('New IP: ' + Reg.localIP)
	}
}

function ValidateIPaddress(ip) 
{  
	return  (/^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(ip))
}
