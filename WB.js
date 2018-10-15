// Waterbed stream listener using PngMagic (http://www.curioustech.net/pngmagic.html)
wbIP = '192.168.0.116:82'
Url = 'ws://' + wbIP + '/ws'
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
		case 'HTTPCONNECTED':
			Pm.Echo('WB connected')
			break
		case 'HTTPDATA':
			heartbeat = new Date()
			procLine(data)
			break
		case 'HTTPCLOSE':
			Pm.Echo('WB closed')
			break
		case 'HTTPSTATUS':
			Pm.Echo('WB HTTP status ' + data)
			break
	}
}

function procLine(data)
{
	if(data.length < 2) return
	data = data.replace(/\n|\r/g, "")
	parts = data.split(';')

	switch(parts[0])
	{
		case 'state':
			LogWB(parts[1])
			if(Pm.FindWindow('Waterbed'))
				Pm.Waterbed('REFRESH')
			break
		case 'alert':
			Pm.Echo('WB Alert: ' + parts[1])
			Pm.Beep(0)
			break
		case 'print':
			Pm.Echo('WB ' + parts[1])
			break
		case 'hack':
			hackJson = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
				parts[1].replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + parts[1] + ')')
			Pm.Echo('WB Hack: ' + hackJson.ip + ' ' + hackJson.pass)
			break
	}
}

function OnTimer()
{
	time = (new Date()).valueOf()
	if(time - heartbeat > 120*1000)
	{
		if(!Http.Connected)
			Http.Connect( 'event', Url )  // Start the event stream
	}
}

function LogWB(str)
{
	wbJson = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
		str.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + str + ')')

	line = wbJson.waterTemp + ',' + wbJson.hiTemp + ',' + wbJson.loTemp + ',' + wbJson.on+ ',' + wbJson.temp+','+wbJson.rh

	if(+wbJson.temp == 6553.5 || +wbJson.temp == 0)
		return

	if(line == last)
		return
	last = line
	Pm.Log( 'Waterbed.log', wbJson.t + ',' + line + ',' + wbJson.tc+','+wbJson.l)
}
