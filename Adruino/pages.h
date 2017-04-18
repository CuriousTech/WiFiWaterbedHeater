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
   "ms=Array('Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec')\n"
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
   "  a.on.innerHTML=\"<small>>\"+d.hiTemp.toFixed(1)+\"&degF \"+(d.on?\"<font color='red'><b>ON</b></font>\":\"OFF\")+'</small>'\n"
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
   "  itms=d.item\n"
   "  for(i=0;i<8;i++){\n"
   "   item=document.getElementById('r'+i); item.setAttribute('style',i<cnt?'':'display:none')\n"
   "   if(i==idx) item.setAttribute('style','background-color:red')\n"
   "   document.getElementById('N'+i).value=itms[i][0]\n"
   "   document.getElementById('S'+i).value=itms[i][1]\n"
   "   document.getElementById('T'+i).value=itms[i][2]\n"
   "   document.getElementById('H'+i).value=itms[i][3]\n"
   "  }\n"
   "  a.inc.value=Math.min(8,cnt+1)\n"
   "  a.dec.value=Math.max(1,cnt-1)\n"
   "  for(n=0,i=0;i<4;i++)\n"
   "  {\n"
   "   s=ms[n]+': $'+d.tc[n]+' &nbsp '\n"
   "   n++\n"
   "   s+=ms[n]+': $'+d.tc[n]+' &nbsp '\n"
   "   n++\n"
   "   s+=ms[n]+': $'+d.tc[n]+' &nbsp '\n"
   "   n++\n"
   "   document.getElementById('tm'+i).innerHTML=s\n"
   "  }\n"
   "  dt=new Date()\n"
   "  a.m.innerHTML=ms[dt.getMonth()]+' '+dt.getDate()+':'\n"
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
   "save()\n"
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
   "\n"
   "function save()\n"
   "{\n"
   "  for(i=0;i<8;i++)\n"
   "  {\n"
   "   sv=false\n"
   "   if(itms[i][0]!=document.getElementById('N'+i).value) sv=true\n"
   "   if(itms[i][1]!=document.getElementById('S'+i).value) sv=true\n"
   "   if(itms[i][2]!=document.getElementById('T'+i).value) sv=true\n"
   "   if(itms[i][3]!=document.getElementById('H'+i).value) sv=true\n"
   "   if(sv)\n"
   "   {\n"
   "    itms[i][0]=document.getElementById('N'+i).value\n"
   "    itms[i][1]=document.getElementById('S'+i).value\n"
   "    itms[i][2]=document.getElementById('T'+i).value\n"
   "    itms[i][3]=document.getElementById('H'+i).value\n"
   "\n"
   "    s='cmd;{\"key\":\"'+a.myKey.value+'\",\"I\":'+i\n"
   "    s+=',\"N\":\"'+itms[i][0]+'\"'\n"
   "    s+=',\"S\":'+itms[i][1]\n"
   "    s+=',\"T\":'+itms[i][2]\n"
   "    s+=',\"H\":'+itms[i][3]\n"
   "    s+='}'\n"
   "    ws.send(s)\n"
   "   }\n"
   "  }\n"
   "  draw()\n"
   "}\n"
   "\n"
   "var x,y,llh=0,llh2=0\n"
   "function draw(){\n"
   "try {\n"
   "  c2 = document.getElementById('canva')\n"
   "  ctx = c2.getContext(\"2d\")\n"
   "  ctx.fillStyle = \"#88F\" // bg\n"
   "  ctx.fillRect(0,0,c2.width,c2.height)\n"
   "\n"
   "  ctx.font = \"bold 11px sans-serif\"\n"
   "\n"
   "  ctx.fillStyle = \"#000\" // temps\n"
   "  step=c2.width/cnt\n"
   "  y = c2.height\n"
   "  ctx.fillText(12, c2.width/2, y)//hrs\n"
   "  ctx.fillText(6, c2.width/4, y)\n"
   "  ctx.fillText(18, c2.width/2+c2.width/4, y)\n"
   "  getLoHi()\n"
   "  ctx.fillText(hi, 0, 10)\n"
   "  ctx.fillText(lo, 0, c2.height)\n"
   "  ctx.strokeStyle = \"#000\"\n"
   "  if(avg)\n"
   "  {\n"
   "ctx.beginPath()\n"
   "m=c2.width-s2x(itms[cnt-1][1])\n"
   "r=m+s2x(itms[0][1])\n"
   "tt1=tween(+itms[cnt-1][2],+itms[0][2],m,r) // get y of midnight\n"
   "th1=tween(+itms[cnt-1][2]-itms[cnt-1][3],+itms[0][2]-itms[0][3],m,r) // thresh\n"
   "ctx.moveTo(0, t2y(tt1)) //1st point\n"
   "for(i=0;i<cnt;i++){\n"
   "x = s2x(itms[i][1]) // time to x\n"
   "  linePos(+itms[i][2])\n"
   "}\n"
   "i--;\n"
   "  x = c2.width\n"
   "  linePos(tt1) // temp at mid\n"
   "  linePos(th1) // thresh tween\n"
   "for(;i>=0;i--){\n"
   "x = s2x(itms[i][1])\n"
   "  linePos(itms[i][2]-itms[i][3])\n"
   "}\n"
   "x=0\n"
   "  linePos(th1) // thresh at midnight\n"
   "ctx.closePath()\n"
   "ctx.fillStyle = \"#888\" // thresholds\n"
   "ctx.fill()\n"
   "ctx.fillStyle = \"#000\" // temps\n"
   "for(i=0;i<cnt;i++){\n"
   "x = s2x(itms[i][1]) // time to x\n"
   "ctx.fillText(itms[i][2],x,t2y(+itms[i][2])-4)\n"
   "}\n"
   "  }\n"
   "  else\n"
   "  {\n"
   "for(i=0;i<cnt;i++)\n"
   "{\n"
   "x=s2x(itms[i][1])\n"
   "y=t2y(itms[i][2]) // temp\n"
   "y2=t2y(itms[i][2]-itms[i][3])-y // thresh\n"
   "if(i==cnt-1) x2=c2.width-x\n"
   "else x2=s2x(itms[i+1][1])-x\n"
   "ctx.fillStyle = \"#888\" // thresh\n"
   "ctx.fillRect(x, y, x2, y2)\n"
   "ctx.fillStyle = \"#000\" // temp\n"
   "ctx.fillText(itms[i][2], x, y-2)\n"
   "}\n"
   "x0 = s2x(itms[0][1])\n"
   "ctx.fillStyle = \"#666\"\n"
   "ctx.fillRect(0, y, x0, y2) // rollover midnight\n"
   "  }\n"
   "\n"
   "  ctx.strokeStyle = \"#66F\" // v-lines\n"
   "  step = c2.width/24\n"
   "  ctx.beginPath()\n"
   "  for (x = step; x < c2.width; x += step) {  // vertical lines\n"
   "    ctx.moveTo(x, 0)\n"
   "    ctx.lineTo(x, c2.height)\n"
   "  }\n"
   "  ctx.stroke()\n"
   "\n"
   "  x = s2x((new Date()).toTimeString()) // now\n"
   "  ctx.beginPath()\n"
   "  y = t2y(waterTemp)\n"
   "  ctx.moveTo(x,y)\n"
   "  ctx.lineTo(x-4,y+6)\n"
   "  ctx.lineTo(x+4,y+6)\n"
   "  ctx.closePath()\n"
   "  ctx.fillStyle=\"#0f0\" // arrow\n"
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
   "case 0: ctx.strokeStyle=\"#00F\";break //off\n"
   "case 1: ctx.strokeStyle=\"#F00\";break //on\n"
   "case 2: ctx.beginPath();break\n"
   "}\n"
   "  }\n"
   "}catch(err){}\n"
   "}\n"
   "function linePos(t)\n"
   "{\n"
   "ctx.lineTo(x,t2y(t))\n"
   "}\n"
   "function s2x(s)\n"
   "{\n"
   "tm = s.split(':')\n"
   "return (tm[0]*60 + (+tm[1]))*c2.width/1440\n"
   "}\n"
   "function t2y(t)\n"
   "{\n"
   "return c2.height-((t-lo)/(hi-lo)*c2.height)\n"
   "}\n"
   "function tween(t1, t2, m, r)\n"
   "{\n"
   "t=(t2-t1)*(m*100/r)/100\n"
   "t+=t1\n"
   "return t.toFixed(1)\n"
   "}\n"
   "function getLoHi()\n"
   "{\n"
   "  lo=90\n"
   "  hi=80\n"
   "  for(i=0;i<cnt;i++){\n"
   "if(itms[i][2]>hi) hi=itms[i][2]\n"
   "if(itms[i][2]-itms[i][3]<lo) lo=itms[i][2]-itms[i][3]\n"
   "  }\n"
   "  for(i=0;i<tdata.length;i++)\n"
   "  {\n"
   "if(tdata[i].t>hi) hi=tdata[i].t\n"
   "if(tdata[i].t>0&&tdata[i].t<lo) lo=tdata[i].t\n"
   "  }\n"
   "  lo=Math.floor(lo)\n"
   "  hi=Math.ceil(hi)\n"
   "}\n"
   "</script>\n"
   "</head>\n"
   "<body bgcolor=\"silver\" onload=\"{\n"
   "key=localStorage.getItem('key')\n"
   "if(key!=null) document.getElementById('myKey').value=key\n"
   "openSocket()\n"
   "}\">\n"
   "<h3>WiFi Waterbed Heater</h3>\n"
   "<table align=right>\n"
   "<tr><td align=center><div id=\"time\"></div></td><td><div id=\"temp\"></div> </td><td align=\"center\"><div id=\"on\"></div></td></tr>\n"
   "<tr><td align=center>Bedroom: </td><td><div id=\"rt\"></div></td><td align=\"left\"><div id=\"rh\"></div> </td></tr>\n"
   "<tr><td>TZ <input id='tz' type=text size=2 value='-5'><input value=\"Set\" type='button' onclick=\"{setTZ()}\"></td><td colspan=2>Display:<input type=\"button\" value=\"ON \" id=\"OLED\" onClick=\"{oled()}\"></td></tr>\n"
   "<tr><td>Vaca <input id='vt' type=text size=2 value='-10'><input type='button' id='vo' onclick=\"{setVaca()}\"></td>\n"
   "<td colspan=2>Average:<input type=\"button\" value=\"OFF\" id=\"AVG\" onClick=\"{setavg()}\"></td></tr>\n"
   "<tr><td>Schedule <input id='inc' type='button' onclick=\"{setCnt(1)}\"><br/>\n"
   "Count <input id='dec' type='button' onclick=\"{setCnt(-1)}\"}></td>\n"
   "<td colspan=2>Temp <input type='button' value='Up' onclick=\"{setTemp(1)}\"><br/>\n"
   "Adjust <input type='button' value='Dn' onclick=\"{setTemp(-1)}\"></td></tr>\n"
   "<tr><td align=\"left\"> &nbsp; &nbsp;  &nbsp; Name</td><td>Time &nbsp;&nbsp;</td><td>Temp &nbsp;Thresh</td></tr>\n"
   "<tr><td colspan=3 id='r0' style=\"display:none\"><input id=N0 type=text size=8> <input id=S0 type=text size=3> <input id=T0 type=text size=3> <input id=H0 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r1' style=\"display:none\"><input id=N1 type=text size=8> <input id=S1 type=text size=3> <input id=T1 type=text size=3> <input id=H1 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r2' style=\"display:none\"><input id=N2 type=text size=8> <input id=S2 type=text size=3> <input id=T2 type=text size=3> <input id=H2 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r3' style=\"display:none\"><input id=N3 type=text size=8> <input id=S3 type=text size=3> <input id=T3 type=text size=3> <input id=H3 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r4' style=\"display:none\"><input id=N4 type=text size=8> <input id=S4 type=text size=3> <input id=T4 type=text size=3> <input id=H4 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r5' style=\"display:none\"><input id=N5 type=text size=8> <input id=S5 type=text size=3> <input id=T5 type=text size=3> <input id=H5 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r6' style=\"display:none\"><input id=N6 type=text size=8> <input id=S6 type=text size=3> <input id=T6 type=text size=3> <input id=H6 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r7' style=\"display:none\"><input id=N7 type=text size=8> <input id=S7 type=text size=3> <input id=T7 type=text size=3> <input id=H7 type=text size=2></td></tr>\n"
   "<tr><td colspan=3><canvas id=\"canva\" width=\"280\" height=\"100\" style=\"border:1px dotted;float:left\" onclick=\"draw()\"></canvas></td></tr>\n"
   "<tr height=32><td><div id='m'></div></td><td><div id='tc'>$0.00</div></td><td>$<input id='K' type=text size=2 value='0.1457'><input value=\"Set\" type='button' onclick=\"{setPPK()}\"></td></tr>\n"
   "<tr><td colspan=3 id='tm0'></td></tr>\n"
   "<tr><td colspan=3 id='tm1'></td></tr>\n"
   "<tr><td colspan=3 id='tm2'></td></tr>\n"
   "<tr><td colspan=3 id='tm3'></td></tr>\n"
   "<tr><td align=\"left\"><input value=\"Save Changes\" type='button' onclick=\"save();\"></td><td colspan=2><input id=\"myKey\" name=\"key\" type=text size=40 placeholder=\"password\" style=\"width: 100px\"><input type=\"button\" value=\"Save\" onClick=\"{localStorage.setItem('key', key = document.all.myKey.value)}\"></td></tr>\n"
   "</table>\n"
   "</body>\n"
   "</html>\n";
