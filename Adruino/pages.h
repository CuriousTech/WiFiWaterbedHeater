const char page1[] PROGMEM =
   "<!DOCTYPE html><html lang=\"en\">\n"
   "<head>\n"
   "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n"
   "<title>WiFi Waterbed Heater</title>\n"
   "<style type=\"text/css\">\n"
   "table,input{\n"
   "border-radius: 5px;\n"
   "box-shadow: 2px 2px 12px #000000;\n"
   "background-image: -moz-linear-gradient(top, #dfffff, #5050ff);\n"
   "background-image: -ms-linear-gradient(top, #dfffff, #5050ff);\n"
   "background-image: -o-linear-gradient(top, #dfffff, #5050ff);\n"
   "background-image: -webkit-linear-gradient(top, #dfffff, #5050ff);\n"
   "background-image: linear-gradient(top, #dfffff, #5050ff);\n"
   "background-clip: padding-box;\n"
   "}\n"
   "body{width:320px;display:block;margin-left:auto;margin-right:auto;text-align:right;font-family: Arial, Helvetica, sans-serif;}}\n"
   "</style>\n"
   "<script type=\"text/javascript\" src=\"data\"></script>\n"
   "<script type=\"text/javascript\">\n"
   "a=document.all\n"
   "oledon=0\n"
   "avg=1\n"
   "cnt=1\n"
   "function openSocket(){\n"
   "ws=new WebSocket(\"ws://\"+window.location.host+\"/ws\")\n"
   "ws.onopen=function(evt) { }\n"
   "ws.onclose=function(evt) { alert(\"Connection closed.\"); }\n"
   "ws.onmessage=function(evt) {\n"
   " lines=evt.data.split(';')\n"
   " event=lines[0]\n"
   " data=lines[1]\n"
   " if(event=='state')\n"
   " {\n"
   "  d=JSON.parse(data)\n"
   "  dt=new Date(d.t*1000)\n"
   "  a.time.innerHTML=dt.toLocaleTimeString()\n"
   "  waterTemp=d.waterTemp.toFixed(1)\n"
   "  a.temp.innerHTML=waterTemp+'&degF'\n"
   "  a.on.innerHTML=\">\"+d.hiTemp.toFixed(1)+\"&degF \"+(d.on?\"<font color='red'><b>ON</b></font>\":\"OFF\")\n"
   "  a.rt.innerHTML=d.temp.toFixed(1)+'&degF'\n"
   "  a.rh.innerHTML=d.rh.toFixed(1)+'%'\n"
   "  a.tc.innerHTML='$'+d.tc.toFixed(2)\n"
   " }\n"
   " else if(event=='set')\n"
   " {\n"
   "  d=JSON.parse(data)\n"
   "  a.vt.value=d.vt.toFixed(1)\n"
   "  oledon=d.o\n"
   "  a.OLED.value=oledon?'ON ':'OFF'\n"
   "  avg=d.avg\n"
   "  a.AVG.value=avg?'ON ':'OFF'\n"
   "  a.tz.value=d.tz\n"
   "  a.K.value=d.ppkwh/10000\n"
   "  a.vo.value=d.vo?'ON ':'OFF'\n"
   "  cnt=d.cnt\n"
   "  idx=d.idx\n"
   "  for(i=0;i<8;i++){\n"
   "   item=document.getElementById('r'+i); item.setAttribute('style',i<cnt?'':'display:none')\n"
   "   if(i==idx) item.setAttribute('style','background-color:red')\n"
   "   document.getElementById('N'+i).value=d.item[i][0]\n"
   "   document.getElementById('S'+i).value=d.item[i][1]\n"
   "   document.getElementById('T'+i).value=d.item[i][2]\n"
   "   document.getElementById('H'+i).value=d.item[i][3]\n"
   "  }\n"
   "  a.inc.value=Math.min(8,cnt+1)\n"
   "  a.dec.value=Math.max(1,cnt-1)\n"
   "  a.tc2.innerHTML=d.tc2\n"
   "  a.m.innerHTML=d.m+':'\n"
   "  draw()\n"
   " }\n"
   " else if(event=='alert')\n"
   " {\n"
   "  alert(data)\n"
   " }\n"
   "}\n"
   "}\n"
   "function setVar(varName, value)\n"
   "{\n"
   " ws.send('cmd;{\"key\":\"'+a.myKey.value+'\",\"'+varName+'\":'+value+'}')\n"
   "}\n"
   "function oled(){oledon=!oledon\n"
   "setVar('oled',oledon)\n"
   "a.OLED.value=oledon?'ON ':'OFF'\n"
   "}\n"
   "function setavg(){avg=!avg\n"
   "setVar('avg',avg)\n"
   "a.AVG.value=avg?'ON ':'OFF'\n"
   "draw()\n"
   "}\n"
   "function setTZ(){\n"
   "setVar('TZ',a.tz.value)}\n"
   "function setTemp(n){\n"
   "setVar('tadj',n)\n"
   "item=document.getElementById('T'+idx)\n"
   "item.value=+item.value+n\n"
   "}\n"
   "function setVaca(){\n"
   "setVar('vacatemp',a.vt.value)\n"
   "setVar('vaca',(a.vo.value=='OFF')?true:false)\n"
   "a.vo.value=(a.vo.value=='OFF')?'ON ':'OFF'\n"
   "}\n"
   "function setCnt(n){\n"
   "cnt+=n\n"
   "setVar('cnt',cnt)\n"
   "}\n"
   "function setPPK(){\n"
   "setVar('ppkwh',(a.K.value*10000).toFixed())\n"
   "}\n"
   "function setEntry(n){\n"
   "name=document.getElementById('N'+n).value\n"
   "tm=document.getElementById('S'+n).value\n"
   "tp=document.getElementById('T'+n).value\n"
   "th=document.getElementById('H'+n).value\n"
   "ws.send('cmd;{\"key\":\"'+a.myKey.value+'\",\"N'+n+'\":\"'+name+'\",\"S'+n+'\":\"'+tm+'\",\"T'+n+'\":\"'+tp+'\",\"H'+n+'\":\"'+th+'\"}')\n"
   "}\n"
   "\n"
   "var x,y,llh=0,llh2=0\n"
   "function draw(){\n"
   "try {\n"
   "  c2 = document.getElementById('canva')\n"
   "  ctx = c2.getContext(\"2d\")\n"
   "  ctx.fillStyle = \"#66F\"\n"
   "  ctx.fillRect(0,0,c2.width,c2.height)//bg\n"
   "  ctx.strokeStyle = \"#666\"\n"
   "  step = c2.width/24\n"
   "  ctx.beginPath()\n"
   "  for (x = step; x < c2.width; x += step) {  // vertical lines\n"
   "    ctx.moveTo(x, 0)\n"
   "    ctx.lineTo(x, c2.height)\n"
   "  }\n"
   "  ctx.stroke()\n"
   "\n"
   "  ctx.font = \"bold 11px sans-serif\"\n"
   "\n"
   "  ctx.fillStyle = \"#111\" // temps\n"
   "  step=c2.width/cnt\n"
   "  y = c2.height\n"
   "  ctx.fillText(12, c2.width/2, y)//hrs\n"
   "  ctx.fillText(6, c2.width/4, y)\n"
   "  ctx.fillText(18, c2.width/2+c2.width/4, y)\n"
   "\n"
   "  ctx.strokeStyle = \"#000\"\n"
   "  if(avg)\n"
   "  {\n"
   "ctx.beginPath()\n"
   "m=c2.width-s2x(d.item[cnt-1][1])\n"
   "r=m+s2x(d.item[0][1])\n"
   "tt1=tween(+d.item[cnt-1][2],+d.item[0][2],m,r) // get y of midnight\n"
   "th1=tween(+d.item[cnt-1][2]-d.item[cnt-1][3],+d.item[0][2]-d.item[0][3],m,r) // thresh\n"
   "ctx.moveTo(0, t2y(tt1)) //1st point\n"
   "for(i=0;i<cnt;i++){\n"
   "x = s2x(d.item[i][1]) // time to x\n"
   "  linePos(+d.item[i][2])\n"
   "ctx.fillText(d.item[i][2],x,t2y(+d.item[i][2])-4)\n"
   "}\n"
   "i--;\n"
   "  x = c2.width\n"
   "  linePos(tt1) // temp at mid\n"
   "  linePos(th1) // thresh tween\n"
   "for(;i>=0;i--){\n"
   "x = s2x(d.item[i][1])\n"
   "  linePos(d.item[i][2]-d.item[i][3])\n"
   "}\n"
   "x=0\n"
   "  linePos(th1) // thresh at midnight\n"
   "ctx.closePath()\n"
   "ctx.fillStyle = \"#666\"\n"
   "ctx.fill()\n"
   "  }\n"
   "  else\n"
   "  {\n"
   "for(i=0;i<cnt;i++)\n"
   "{\n"
   "x=s2x(d.item[i][1])\n"
   "y=t2y(d.item[i][2]) // temp\n"
   "y2=t2y(d.item[i][2]-d.item[i][3])-y // thresh\n"
   "if(i==cnt-1) x2=c2.width-x\n"
   "else x2=s2x(d.item[i+1][1])-x\n"
   "ctx.fillStyle = \"#666\"\n"
   "ctx.fillRect(x, y, x2, y2)\n"
   "ctx.fillStyle = \"#000\"\n"
   "ctx.fillText(d.item[i][2], x, y-2)\n"
   "}\n"
   "x0 = s2x(d.item[0][1])\n"
   "ctx.fillStyle = \"#666\"\n"
   "ctx.fillRect(0, y, x0, y2) // rollover midnight\n"
   "  }\n"
   "  x = s2x((new Date()).toTimeString()) // now\n"
   "  ctx.beginPath()\n"
   "  y = t2y(waterTemp)\n"
   "  ctx.moveTo(x,y)\n"
   "  ctx.lineTo(x-4,y+6)\n"
   "  ctx.lineTo(x+4,y+6)\n"
   "  ctx.closePath()\n"
   "  ctx.fillStyle=\"#0f0\"\n"
   "  ctx.fill()\n"
   "  ctx.stroke()\n"
   "\n"
   "  ctx.beginPath()\n"
   "  for(i=0;i<tdata.length;i++) // hist data\n"
   "  {\n"
   "if(tdata[i].t==0) continue;\n"
   "x=s2x(tdata[i].tm)\n"
   "y=t2y(tdata[i].t)\n"
   "ctx.lineTo(x,y)\n"
   "ctx.stroke()\n"
   "ctx.beginPath()\n"
   "ctx.moveTo(x,y)\n"
   "switch(tdata[i].s)\n"
   "{\n"
   "case 0: ctx.strokeStyle=\"#00F\";break\n"
   "case 1: ctx.strokeStyle=\"#F00\";break\n"
   "case 2: ctx.beginPath();break\n"
   "}\n"
   "  }\n"
   "}catch(err){}\n"
   "}\n"
   "function linePos(t)\n"
   "{\n"
   "  ctx.lineTo(x,t2y(t))\n"
   "}\n"
   "function s2x(s)\n"
   "{\n"
   "  tm = s.split(':')\n"
   "  return (tm[0]*60 + (+tm[1]))*c2.width/1440\n"
   "}\n"
   "function t2y(t)\n"
   "{\n"
   "  return c2.height-((t-70)/20*c2.height)\n"
   "}\n"
   "function tween(t1, t2, m, r)\n"
   "{\n"
   "t=(t2-t1)*(m*100/r)/100\n"
   "t+=t1\n"
   "return t.toFixed(1)\n"
   "}\n"
   "</script>\n"
   "</head>\n"
   "<body bgcolor=\"silver\" onload=\"{\n"
   "key=localStorage.getItem('key')\n"
   "if(key!=null) document.getElementById('myKey').value=key\n"
   "openSocket()\n"
   "}\">\n"
   "<h3>WiFi Waterbed Heater</h3>\n"
   "<table align=\"right\">\n"
   "<tr><td align=\"center\" colspan=2><div id=\"time\"></div></td><td><div id=\"temp\"></div> </td><td align=\"center\"><div id=\"on\"></div></td></tr>\n"
   "<tr><td align=\"center\" colspan=2>Bedroom: </td><td><div id=\"rt\"></div></td><td align=\"left\"><div id=\"rh\"></div> </td></tr>\n"
   "<tr><td colspan=2>TZ <input id='tz' type=text size=2 value='-5'><input value=\"Set\" type='button' onclick=\"{setTZ()}\"></td><td colspan=2>Display:<input type=\"button\" value=\"ON \" id=\"OLED\" onClick=\"{oled()}\"></td></tr>\n"
   "<tr><td colspan=2>Vaca:<input id='vt' type=text size=2 value='-10'><input type='button' id='vo' onclick=\"{setVaca()}\"></td>\n"
   "<td colspan=2>Average:<input type=\"button\" value=\"OFF\" id=\"AVG\" onClick=\"{setavg()}\"></td></tr>\n"
   "<tr><td colspan=2>Schedule <input id='inc' type='button' onclick=\"{setCnt(1)}\"><br/>\n"
   "Count <input id='dec' type='button' onclick=\"{setCnt(-1)}\"}></td>\n"
   "<td colspan=2>Temp <input type='button' value='Up' onclick=\"{setTemp(1)}\"><br/>\n"
   "Adjust <input type='button' value='Dn' onclick=\"{setTemp(-1)}\"></td></tr>\n"
   "<tr><td align=\"left\">Name</td><td align=\"left\">Time</td><td align=\"left\">Temp</td><td width=\"99px\" align=\"left\">Threshold</td></tr>\n"
   "<tr><td colspan=4 id='r0' style=\"display:none\"><input id=N0 type=text size=6> <input id=S0 type=text size=3> <input id=T0 type=text size=3> <input id=H0 type=text size=2> <input value=\"Set\" type='button' onclick=\"{setEntry(0)}\"></td></tr>\n"
   "<tr><td colspan=4 id='r1' style=\"display:none\"><input id=N1 type=text size=6> <input id=S1 type=text size=3> <input id=T1 type=text size=3> <input id=H1 type=text size=2> <input value=\"Set\" type='button' onclick=\"{setEntry(1)}\"></td></tr>\n"
   "<tr><td colspan=4 id='r2' style=\"display:none\"><input id=N2 type=text size=6> <input id=S2 type=text size=3> <input id=T2 type=text size=3> <input id=H2 type=text size=2> <input value=\"Set\" type='button' onclick=\"{setEntry(2)}\"></td></tr>\n"
   "<tr><td colspan=4 id='r3' style=\"display:none\"><input id=N3 type=text size=6> <input id=S3 type=text size=3> <input id=T3 type=text size=3> <input id=H3 type=text size=2> <input value=\"Set\" type='button' onclick=\"{setEntry(3)}\"></td></tr>\n"
   "<tr><td colspan=4 id='r4' style=\"display:none\"><input id=N4 type=text size=6> <input id=S4 type=text size=3> <input id=T4 type=text size=3> <input id=H4 type=text size=2> <input value=\"Set\" type='button' onclick=\"{setEntry(4)}\"></td></tr>\n"
   "<tr><td colspan=4 id='r5' style=\"display:none\"><input id=N5 type=text size=6> <input id=S5 type=text size=3> <input id=T5 type=text size=3> <input id=H5 type=text size=2> <input value=\"Set\" type='button' onclick=\"{setEntry(5)}\"></td></tr>\n"
   "<tr><td colspan=4 id='r6' style=\"display:none\"><input id=N6 type=text size=6> <input id=S6 type=text size=3> <input id=T6 type=text size=3> <input id=H6 type=text size=2> <input value=\"Set\" type='button' onclick=\"{setEntry(6)}\"></td></tr>\n"
   "<tr><td colspan=4 id='r7' style=\"display:none\"><input id=N7 type=text size=6> <input id=S7 type=text size=3> <input id=T7 type=text size=3> <input id=H7 type=text size=2> <input value=\"Set\" type='button' onclick=\"{setEntry(7)}\"></td></tr>\n"
   "<tr><td colspan=4><canvas id=\"canva\" width=\"288\" height=\"100\" style=\"border:1px dotted;float:left\" onclick=\"draw()\"></canvas></td></tr>\n"
   "<tr height=32><td id='tc2'></td><td id='m'></td><td><div id='tc'>$0.00</div></td><td>$<input id='K' type=text size=2 value='0.1457'><input value=\"Set\" type='button' onclick=\"{setPPK()}\"></td></tr>\n"
   "<tr><td colspan=2></td><td colspan=2><input id=\"myKey\" name=\"key\" type=text size=40 placeholder=\"password\" style=\"width: 90px\"><input type=\"button\" value=\"Save\" onClick=\"{localStorage.setItem('key', key = document.all.myKey.value)}\"></td></tr>\n"
   "</table>\n"
   "</body>\n"
   "</html>\n";
