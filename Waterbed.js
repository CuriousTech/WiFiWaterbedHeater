// Chart display for waterbed temp, room temp/rh

wb_url = 'http://192.168.0.199:82/'

Pm.Echo()
Pm.Window( 'Waterbed' )
ReadLogs()		// date, temp, rh
moved = 0
pos = 0
dateNow = new Date()
dateStart = dateNow  // start date of mouse down

	wbWatts = 290
	ppkwh = 0.126  // electric price per KWH  (price / KWH)
	ppkwh  += 0.0846 + 0.03 // surcharge 10.1%, school 3%, misc

totalCost = 0
totalTime = 0

var wbJson

Setup()

Draw(0)

Pm.SetTimer(4*60*1000)

function Setup()
{
	h1 = ArrayDuration( listWb ) // max hours of data
	h1++		// max hours of data
	h_show = 8 * 24	// max hours to show
	if(h_show > h1 ) h_show = h1

	// Temperature range
	min = arrayMin(listWb, 1, 200)
	max = arrayMax(listWb, 1, 0)
	wbmin = min
	wbmax = max
	min = arrayMin(listWb, 5, min)
	max = arrayMax(listWb, 5, max)
	Http.Connect('Waterbed', wb_url + 'json')
}

function OnMouse(Msg, x, y)
{
	switch(Msg)
	{
		case 'LDN':		// left button down
			moved = 0
			pos = x
			if(x < left)
			{
				ReadLogs()
			}else{
				dateStart = dateNow
			}
			break
		case 'LUP':
			break
		case 'MOVE':	// drag to slide
			inc = (right - left) / h_show
			dt = new Date()
			if( (pos-x) == 0) break
			dateNow = dateStart - ((6000*((right-left) / inc)) * (-(pos-x) ) ).toFixed(2)
			moved = 1
			break
		case 'WHEEL':		// wheel zoom
			m = (h_show / 10)
			if(m < 1) m = 1
			h_show -= x * m
			h_show = Math.floor(h_show)
			if(h_show < 8) h_show = 8
			if(h_show > 182) h_show = 182
			break
	}
	Draw(0)
}

function OnCall(msg, a1, data) // Called from SparkRemote
{
	switch(msg)
	{
		case 'REFRESH':
			ReadLogs()
			Draw(0)
			break
		case 'BUTTON':
			break
		case 'HTTPDATA':
			wbJson = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
				data.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + data + ')')
			Draw(0)
			break
	}
}

function OnTimer()
{
	ReadLogs()
	Draw(1)
}

// Let's draw this
function Draw(fast)
{
	if(!fast)
	{
		Gdi.Width = 600 // resize drawing area/window
		Gdi.Height = 250

		left = 60 // chart area
		top = 23
		bottom = Gdi.Height - 36
		right = Gdi.Width - 34
		Gdi.Clear(0) // all trasparent

		Gdi.Brush( Gdi.Argb(120, 0, 0, 240) ) // drag bar
		Gdi.FillRectangle(0, 0, Gdi.Width-1, 26)

		// rounded window
		Gdi.Clear(0, 1, 16, Gdi.Width-2, 16) // erase bottom of drag bar
		Gdi.Brush( Gdi.Argb(100, 0,0,0) ) // background
		Gdi.FillRectangle(0, 0, Gdi.Width-1, Gdi.Height-1)
		Gdi.Pen( Gdi.Argb(120, 0, 0, 240), 1 ) // blue borders
		Gdi.Rectangle(0, 0, Gdi.Width-1, Gdi.Height-1)

		// Title
		Gdi.Font( 'Courier New', 14, 'BoldItalic')
		Gdi.Brush( Gdi.Argb(255, 255, 230, 25) )
		Gdi.Text( 'Waterbed', 10, 0 )

		Gdi.Brush(Gdi.Argb(255, 255, 0, 0) ) // close button
		Gdi.Text('X', Gdi.Width - 18, 1)
	}
	// chart BG
	Gdi.Brush( Gdi.Argb(235, 0, 0, 0) )
	Gdi.FillRectangle( left-1, top, right-left, bottom-top )
	Gdi.Font( 'Courier New', 10)
	Gdi.Brush( Gdi.Argb(255, 255, 255, 250) )  // text color
	Gdi.Pen( Gdi.Argb(255, 50, 50, 50), 1 )   // grid color

		// Temp scale and H-lines
		y = top - 6
		incy = (bottom-top)/9
		dec = (max-min)/9
		t = max
		for(i = 0; i <= 9; i++)
		{
			x = right+20-( (t<0) ? 25:19 ) // shift negative left
			if( t > -10 && t < 10) x += 6 // single digit
			Gdi.Text( t.toFixed(0), x, y )
			Gdi.Line(left, y+6, right+2, y+6)
			y += incy
			t -= dec
		}

	inc = (right - left) / (h_show * 60)

	Gdi.Font( 'Courier New', 10) // time totals
	Gdi.Pen(Gdi.Argb(255, 0, 0, 0), 1)


	Gdi.Font( 'Courier New', 10)
	// Days and grid
	days = Array('Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat')
	date1 = new Date(dateNow)
	day = date1.getDay()
	d = date1.getDate()
	hour = date1.getHours()
	// Grid
	Gdi.Brush( Gdi.Argb(255, 255, 255, 0) )
	if(listWb.length)
	{
		date = new Date( listWb[listWb.length-1][0] * 1000)
		date1.setMinutes(date.getMinutes())
		pt = (dateNow - date1) / (1000 * 60)
		x =  (right - (pt * inc))

		for(i = 1000; i >= 0; i--)  // v-lines
		{
			if(x < left) break

			if(hour == 0)
			{
				Gdi.Pen( Gdi.Argb(200, 80, 80, 80), 1 )   // midnight
				Gdi.Line(x, top, x, bottom)
				Gdi.Pen( Gdi.Argb(200, 0, 0, 1), 1 )
				if(d < 1 ) d = getDays(date.getMonth(), date.getYear()) // end of month wrap
				if( x < right) Gdi.OutlineText(d + ' ' + days[day], x + (600*inc), bottom + 1, 5)
				if(--day <0) day = 6
				d--
			}
			if(hour == 12)
			{
				Gdi.Pen( Gdi.Argb(100, 50, 50, 50), 1 )   // noon
				Gdi.Line(x, top, x, bottom)
			}

			h = hour
			if(h > 12) h -= 12		// 12h format
			if(h == 0) h = 12
			if(inc < 0.06 && (hour &7));	// hour skip if too close
			else if(inc < 0.14 && (hour &3));	// hour skip if too close
			else if(inc < 0.24 && (hour &1));
			else Gdi.Text( h, x, top-9, 'Center' )

			if(h_show < 48)	// hourly lines
			{
				Gdi.Pen( Gdi.Argb(100, 50, 50, 50), 1 )
				Gdi.Line(x, top, x, bottom)
			}

			if(--hour < 0) hour = 23
			x -= (inc * 60)
		}
	}
	// Now arrow
	Gdi.Pen( Gdi.Argb(255, 255, 0, 0), 1 )
	date0 = new Date()
	x = dateToX(date0)
	if(x >= left && x <= right)
	{
		Gdi.Line(x, bottom, x+3, bottom+6)
		Gdi.Line(x, bottom, x-3, bottom+6)
		Gdi.Line(x, top, x+3, top-6)
		Gdi.Line(x, top, x-3, top-6)
	}

	Gdi.Font( 'Courier New', 10) // temp peaks font
	if( listWb.length )
	{
		ptTemp = new Array()
		ptThr1 = new Array()
		ptThr2 = new Array()
		ptinTemp = new Array()
		ptRh = new Array()

		totalCost = 0
		dayCost = 0
		date = new Date(listWb[0][0] * 1000)
		lDate = date
		on = false
		tog = 5
		tot = 0
		totalTime = 0
		color = Gdi.Argb(160, 255, 55, 55)
		lastHour = 0

		for (i = 0; i < listWb.length; i++)
		{
			date = new Date(listWb[i][0] * 1000)
			pt = (dateNow - date) / (1000 * 60)
			x = (right - (pt * inc))
			hour = date.getHours()

			if(hour != lastHour)
			{
				if(hour == 0) // midnight to prev noon
				{
					if(dayCost){
						Gdi.Pen( Gdi.Argb(100, 0,0, 0), 1 )
						Gdi.Brush(Gdi.Argb(255, 0,255,100) )
						Gdi.OutlineText(tot, x+20-(600*inc), bottom+12, 3, 'Time')
						Gdi.OutlineText('$' + dayCost.toFixed(2), x+20-(600*inc), bottom+23, 3, 'Right')
					}
					dayCost = 0
					tot = 0
				}
				lastHour = hour
			}

			t = +listWb[i][1]	// temp
			y = valToY(t)

			if(+listWb[i][4] != on)
			{
				color = on ? Gdi.Argb(160, 255, 40, 40) : Gdi.Argb(160, 40, 40, 255)
				on = +listWb[i][4]
				secs = (date.valueOf() - lDate.valueOf()) / 1000
				if(on)
					lDate = date
				else{
					cost = ppkwh * secs / (1000*60*60) * wbWatts
					tot += secs
					totalTime += secs
					totalCost += cost
					dayCost += cost
				}

				Gdi.Pen(color, 1)
				Gdi.Lines( ptTemp )
				ptTemp = new Array()
				ptTemp[ptTemp.length] = x
				ptTemp[ptTemp.length] = y
			}

			if( x >= left)
			{
				ptTemp[ptTemp.length] = x
				ptTemp[ptTemp.length] = y
				tH = +listWb[i][2] // target
				tL = +listWb[i][3]
				ptThr1[ptThr1.length] = x
				ptThr1[ptThr1.length] = valToY(tH)
				ptThr2[ptThr2.length] = 	valToY(tL)
				ptThr2[ptThr2.length] = x

				ptinTemp[ptinTemp.length] = x
				ptinTemp[ptinTemp.length] = valToY( +listWb[i][5] )

				ptRh[ptRh.length] = x
				ptRh[ptRh.length] = bottom - (+listWb[i][6]) * (bottom-top) / 100
			}
		}

		secs = (date.valueOf() -lDate.valueOf()) / 1000
		if(on)
		{
			cost = ppkwh * secs / (1000*60*60) * wbWatts
			totalCost += cost
			dayCost += cost
			tot += secs
			totalTime += secs
		}
		color = on ? Gdi.Argb(160, 255, 55, 55) : Gdi.Argb(160, 55, 55, 255)
		// WB Temp color
		Gdi.Pen(color, 1)
		Gdi.Lines( ptTemp )

		// TargetTemp color
		Gdi.Brush(Gdi.Argb(20, 55, 155, 155))
		for(i = ptThr2.length; i>0; i--)
			ptThr1[ptThr1.length] = ptThr2[i]
		Gdi.FillPolygon( ptThr1 )

		// ambeint Temp color
		Gdi.Pen(Gdi.Argb(160, 200, 50, 150), 1)
		Gdi.Lines( ptinTemp )
		Gdi.Pen(Gdi.Argb(160, 10, 200, 10), 1)
		Gdi.Lines( ptRh )
	}

	if(dayCost)
	{
		Gdi.Pen( Gdi.Argb(100, 0,0, 0), 1 )
		Gdi.Brush(Gdi.Argb(255, 0,255,200) )
//		Gdi.OutlineText(tot, x-20+(600*inc), bottom+12, 3, 'Time')
//		Gdi.OutlineText('$' + dayCost.toFixed(2), x-20+(600*inc), bottom+23, 3, 'Right')
	}

	if(wbJson != undefined)
	{
		Gdi.Font( 'Crystal clear' , 10, 'Regular')
		Gdi.Pen(Gdi.Argb(255, 0, 0, 0), 1 )
		Gdi.Brush( Gdi.Argb(255, 55, 255, 255) )
		Gdi.OutlineText('H ' + wbJson.hiTemp + '°', 6, y = 27, 3)
		Gdi.OutlineText('> ' + wbJson.waterTemp + '°', 6, y += 17, 3)
		Gdi.OutlineText('L ' + wbJson.loTemp + '°', 6, y += 17, 3)
	}

	Gdi.Font( 'Courier New', 11, 'BoldItalic')

	// Legend
	if(!fast)
	{
		Gdi.Pen(Gdi.Argb(255, 0, 0, 0), 1 )
		y = 80
		Gdi.Brush( Gdi.Argb(255, 20, 250, 20) )
		Gdi.OutlineText( 'rH', 2, y+=23, 3)
		Gdi.Brush( Gdi.Argb(255, 250, 0, 200) )
		Gdi.OutlineText( 'Room', 2, y+=23, 3)
		// Run
		y = Gdi.Height - 100
		Gdi.Brush( Gdi.Argb(255, 150, 150, 255) )
		Gdi.OutlineText( 'Total', 2, y+=23, 3)
		Gdi.OutlineText( totalTime, 56, y += 12, 3, 'Time')
		Gdi.OutlineText( '$' + totalCost.toFixed(2), 56, y += 12, 3, 'Right')
	}
}

function dateToX(d)
{
	pt = (dateNow - d) / (1000 * 60)
	return (right - (pt * inc))
}

function valToY(v)
{
	return (bottom - (v - min) * (bottom-top-1) / (max-min))
}

// get days of a month for wraparounds
function getDays(month, year)
{
	var ar = new Array( 31, (year % 4 == 0) ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 )
	return ar[month]
}

// Load all logs, and reduce if needed
function ReadLogs()
{
	fso = new ActiveXObject( 'Scripting.FileSystemObject' )

	listWb = new Array()
	LoadArray('Waterbed.log', listWb)
	SaveArray('Waterbed.log', listWb)

	fso = null
}

// Load an array from a comma delimited file
function LoadArray(name, arr)
{
	if(  !fso.FileExists(name) ) return

	tf = fso.OpenTextFile( name, 1, false)
	lt = 0
	while( !tf.AtEndOfStream)
	{
		arr[arr.length] = tf.ReadLine().split( ',' )
	}
	tf.Close()
}

// Return start to end of array in hours
function ArrayDuration(arr)
{
	if(arr.length < 2) return 0
	r1 = new Date( arr[0][0] * 1000 )
	r2 = new Date( arr[arr.length-1][0] * 1000 )
	return (r2 - r1) / (1000 * 60*60)		// hours long
}

function arrayMin(arr, n, min)
{
		for ( i = 0; i < arr.length; i++)
		{
			t = +arr[i][n]
			if(min > t) min = t
		}
		return min
}
function arrayMax(arr, n, max)
{
		for ( i = 0; i < arr.length; i++)
		{
			t = +arr[i][n]
			if(max < t) max = t
		}
		return max
}

// Save an array to a file
function SaveArray(name, arr)
{
	max_hours = 24 * 10
	if( ArrayDuration( arr ) <  max_hours) return // check array duration

	tf = fso.CreateTextFile(name, true)

	now = new Date()

	for(ai = 0; ai < arr.length; ai++)
	{
		d = new Date( arr[ai][0] * 1000 )
		if( ( (now - d) / (1000*60*60) ) < max_hours) // less than X days old
		{
			line = ''
			for(n = 0; n < arr[ai].length; n++)
			{
				line += arr[ai][n]
				if(arr[ai].length > n+1) line += ','
			}

			t = +arr[ai][0]
			if( t < lt)
				Pm.Echo('Error in ' + name + ' line ' + ai)
			lt = t

			tf.WriteLine( line)
		}
	}
	tf.Close()
}
