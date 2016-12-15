// Send mesage and beep to waterbed (uses PngMagic)
//
//   Ex: Pm.WbMsg('ALERT', 'Doorbell', 1200, 2000)

wbIP = '192.168.0.104:82'
key = 'password'

function OnCall(msg, event, data, a3)
{
	switch(msg)
	{
		case 'ALERT': // event = text, data = freq, a3 = period
			params = '/s?key=' + key + '&m=' + event + '&f=' + data + '&b=' + a3
			Url = 'http://' + wbIP + params
			Http.Connect( 'wbMsg', Url )
			break
		case 'HTTPDATA':
			break
	}
}
