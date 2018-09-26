const char index_page[] PROGMEM =
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
   "//ws=new WebSocket(\"ws://192.168.0.104:82/ws\")\n"
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
   "  ppkw=d.ppkwh/10000\n"
   "  a.K.value=ppkw\n"
   "  a.vo.value=d.vo?'ON ':'OFF'\n"
   "  watts=a.w\n"
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
   "  tot=0\n"
   "  for(i=0;i<12;i++)\n"
   "   tot+=+d.tc[i]\n"
   "  a.tot.innerHTML='Year: $'+tot.toFixed(2)+' &nbsp '\n"
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
   "  ctx.lineWidth = 1\n"
   "  getLoHi()\n"
   "  ctx.strokeStyle = \"#000\"\n"
   "  ctx.fillStyle = \"#000\"\n"
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
   "ctx.fillStyle = \"rgba(80,80,80,0.5)\" // thresholds\n"
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
   "ctx.fillStyle = \"rgba(80,80,80,0.5)\" // thresh\n"
   "ctx.fillRect(x, y, x2, y2)\n"
   "ctx.fillStyle = \"#000\" // temp\n"
   "ctx.fillText(itms[i][2], x, y-2)\n"
   "}\n"
   "x0 = s2x(itms[0][1])\n"
   "ctx.fillStyle = \"#666\"\n"
   "ctx.fillRect(0, y, x0, y2) // rollover midnight\n"
   "  }\n"
   "\n"
   "  ctx.fillStyle = \"#000\" // temps\n"
   "  step=c2.width/cnt\n"
   "  y = c2.height\n"
   "  ctx.fillText(12,c2.width/2-6, y)//hrs\n"
   "  ctx.fillText(6, c2.width/4-3, y)\n"
   "  ctx.fillText(18,c2.width/2+c2.width/4-6, y)\n"
   "  getLoHi()\n"
   "  ctx.fillStyle = \"#337\" // temp scale\n"
   "  ctx.fillText(hi,0,10)\n"
   "  ctx.fillText(lo,0,c2.height)\n"
   "  hl=hi-lo\n"
   "  ctx.strokeStyle = \"rgba(13,13,13,0.5)\" // h-lines\n"
   "  step = c2.height/hl\n"
   "  ctx.beginPath()\n"
   "  for(y = step,n=hi-1;y<c2.height;y+= step){  // vertical lines\n"
   "    ctx.moveTo(14,y)\n"
   "    ctx.lineTo(c2.width,y)\n"
   "ctx.fillText(n--,0,y+3)\n"
   "  }\n"
   "  ctx.stroke()\n"
   "  step = c2.width/24\n"
   "  ctx.beginPath()\n"
   "  for (x = step; x < c2.width; x += step){ // vertical lines\n"
   "    ctx.moveTo(x,0)\n"
   "    ctx.lineTo(x,c2.height)\n"
   "  }\n"
   "  ctx.stroke()\n"
   "\n"
   "  nowX = s2x((new Date()).toTimeString()) // now\n"
   "  ctx.beginPath()\n"
   "  ctx.strokeStyle=\"#0D0\"\n"
   "  brk=false\n"
   "  for(i=0;i<tdata.length;i++) // rh\n"
   "  {\n"
   "if(tdata[i].t==0) continue\n"
   "x=s2x(tdata[i].tm)\n"
   "y=c2.height-(tdata[i].rh/100*c2.height)\n"
   "ctx.lineTo(x,y)\n"
   "ctx.stroke()\n"
   "ctx.beginPath()\n"
   "if(tdata[i].s!=2)\n"
   "ctx.moveTo(x,y)\n"
   "  }\n"
   "\n"
   "  ctx.beginPath()\n"
   "  ctx.strokeStyle=\"#834\"\n"
   "  for(i=0;i<tdata.length;i++) // room temp\n"
   "  {\n"
   "if(tdata[i].t==0) continue\n"
   "x=s2x(tdata[i].tm)\n"
   "y=t2y(tdata[i].rm)\n"
   "ctx.lineTo(x,y)\n"
   "ctx.stroke()\n"
   "ctx.beginPath()\n"
   "if(tdata[i].s!=2)\n"
   "ctx.moveTo(x,y)\n"
   "  }\n"
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
   "switch(tdata[i].s)\n"
   "{\n"
   "case 0: ctx.strokeStyle=\"#00F\"; ctx.moveTo(x,y); break //off\n"
   "case 1: ctx.strokeStyle=\"#F00\"; ctx.moveTo(x,y); break //on\n"
   "case 2: break\n"
   "}\n"
   "  }\n"
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
   "  lo=99\n"
   "  hi=60\n"
   "  for(i=0;i<cnt;i++){\n"
   "if(itms[i][2]>hi) hi=itms[i][2]\n"
   "if(itms[i][2]-itms[i][3]<lo) lo=itms[i][2]-itms[i][3]\n"
   "  }\n"
   "  for(i=0;i<tdata.length;i++)\n"
   "  {\n"
   "if(tdata[i].t>hi) hi=tdata[i].t\n"
   "if(tdata[i].rm>hi) hi=tdata[i].rm\n"
   "if(tdata[i].t>0&&tdata[i].t<lo) lo=tdata[i].t\n"
   "if(tdata[i].rm>0&&tdata[i].rm<lo) lo=tdata[i].rm\n"
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
   "<tr><td colspan=3 id='r0' style=\"display:none\"><input id=N0 type=text size=10> <input id=S0 type=text size=3> <input id=T0 type=text size=3> <input id=H0 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r1' style=\"display:none\"><input id=N1 type=text size=10> <input id=S1 type=text size=3> <input id=T1 type=text size=3> <input id=H1 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r2' style=\"display:none\"><input id=N2 type=text size=10> <input id=S2 type=text size=3> <input id=T2 type=text size=3> <input id=H2 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r3' style=\"display:none\"><input id=N3 type=text size=10> <input id=S3 type=text size=3> <input id=T3 type=text size=3> <input id=H3 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r4' style=\"display:none\"><input id=N4 type=text size=10> <input id=S4 type=text size=3> <input id=T4 type=text size=3> <input id=H4 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r5' style=\"display:none\"><input id=N5 type=text size=10> <input id=S5 type=text size=3> <input id=T5 type=text size=3> <input id=H5 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r6' style=\"display:none\"><input id=N6 type=text size=10> <input id=S6 type=text size=3> <input id=T6 type=text size=3> <input id=H6 type=text size=2></td></tr>\n"
   "<tr><td colspan=3 id='r7' style=\"display:none\"><input id=N7 type=text size=10> <input id=S7 type=text size=3> <input id=T7 type=text size=3> <input id=H7 type=text size=2></td></tr>\n"
   "<tr><td colspan=3><canvas id=\"canva\" width=\"280\" height=\"140\" style=\"border:1px dotted;float:center\" onclick=\"draw()\"></canvas></td></tr>\n"
   "<tr height=32><td><div id='m'></div></td><td><div id='tc'>$0.00</div></td><td>$<input id='K' type=text size=2 value='0.1457'><input value=\"Set\" type='button' onclick=\"{setPPK()}\"></td></tr>\n"
   "<tr><td colspan=3 id='tm0'></td></tr>\n"
   "<tr><td colspan=3 id='tm1'></td></tr>\n"
   "<tr><td colspan=3 id='tm2'></td></tr>\n"
   "<tr><td colspan=3 id='tm3'></td></tr>\n"
   "<tr><td colspan=3 id='tot'></td></tr>\n"
   "<tr><td align=\"left\"><input value=\"Save Changes\" type='button' onclick=\"save();\"></td><td colspan=2><input id=\"myKey\" name=\"key\" type=text size=40 placeholder=\"password\" style=\"width: 100px\"><input type=\"button\" value=\"Save\" onClick=\"{localStorage.setItem('key', key = document.all.myKey.value)}\"></td></tr>\n"
   "</table>\n"
   "</body>\n"
   "</html>\n";

const uint8_t favicon[] PROGMEM = {
  0x1F, 0x8B, 0x08, 0x08, 0x70, 0xC9, 0xE2, 0x59, 0x04, 0x00, 0x66, 0x61, 0x76, 0x69, 0x63, 0x6F, 
  0x6E, 0x2E, 0x69, 0x63, 0x6F, 0x00, 0xD5, 0x94, 0x31, 0x4B, 0xC3, 0x50, 0x14, 0x85, 0x4F, 0x6B, 
  0xC0, 0x52, 0x0A, 0x86, 0x22, 0x9D, 0xA4, 0x74, 0xC8, 0xE0, 0x28, 0x46, 0xC4, 0x41, 0xB0, 0x53, 
  0x7F, 0x87, 0x64, 0x72, 0x14, 0x71, 0xD7, 0xB5, 0x38, 0x38, 0xF9, 0x03, 0xFC, 0x05, 0x1D, 0xB3, 
  0x0A, 0x9D, 0x9D, 0xA4, 0x74, 0x15, 0x44, 0xC4, 0x4D, 0x07, 0x07, 0x89, 0xFA, 0x3C, 0x97, 0x9C, 
  0xE8, 0x1B, 0xDA, 0x92, 0x16, 0x3A, 0xF4, 0x86, 0x8F, 0x77, 0x73, 0xEF, 0x39, 0xEF, 0xBD, 0xBC, 
  0x90, 0x00, 0x15, 0x5E, 0x61, 0x68, 0x63, 0x07, 0x27, 0x01, 0xD0, 0x02, 0xB0, 0x4D, 0x58, 0x62, 
  0x25, 0xAF, 0x5B, 0x74, 0x03, 0xAC, 0x54, 0xC4, 0x71, 0xDC, 0x35, 0xB0, 0x40, 0xD0, 0xD7, 0x24, 
  0x99, 0x68, 0x62, 0xFE, 0xA8, 0xD2, 0x77, 0x6B, 0x58, 0x8E, 0x92, 0x41, 0xFD, 0x21, 0x79, 0x22, 
  0x89, 0x7C, 0x55, 0xCB, 0xC9, 0xB3, 0xF5, 0x4A, 0xF8, 0xF7, 0xC9, 0x27, 0x71, 0xE4, 0x55, 0x38, 
  0xD5, 0x0E, 0x66, 0xF8, 0x22, 0x72, 0x43, 0xDA, 0x64, 0x8F, 0xA4, 0xE4, 0x43, 0xA4, 0xAA, 0xB5, 
  0xA5, 0x89, 0x26, 0xF8, 0x13, 0x6F, 0xCD, 0x63, 0x96, 0x6A, 0x5E, 0xBB, 0x66, 0x35, 0x6F, 0x2F, 
  0x89, 0xE7, 0xAB, 0x93, 0x1E, 0xD3, 0x80, 0x63, 0x9F, 0x7C, 0x9B, 0x46, 0xEB, 0xDE, 0x1B, 0xCA, 
  0x9D, 0x7A, 0x7D, 0x69, 0x7B, 0xF2, 0x9E, 0xAB, 0x37, 0x20, 0x21, 0xD9, 0xB5, 0x33, 0x2F, 0xD6, 
  0x2A, 0xF6, 0xA4, 0xDA, 0x8E, 0x34, 0x03, 0xAB, 0xCB, 0xBB, 0x45, 0x46, 0xBA, 0x7F, 0x21, 0xA7, 
  0x64, 0x53, 0x7B, 0x6B, 0x18, 0xCA, 0x5B, 0xE4, 0xCC, 0x9B, 0xF7, 0xC1, 0xBC, 0x85, 0x4E, 0xE7, 
  0x92, 0x15, 0xFB, 0xD4, 0x9C, 0xA9, 0x18, 0x79, 0xCF, 0x95, 0x49, 0xDB, 0x98, 0xF2, 0x0E, 0xAE, 
  0xC8, 0xF8, 0x4F, 0xFF, 0x3F, 0xDF, 0x58, 0xBD, 0x08, 0x25, 0x42, 0x67, 0xD3, 0x11, 0x75, 0x2C, 
  0x29, 0x9C, 0xCB, 0xF9, 0xB9, 0x00, 0xBE, 0x8E, 0xF2, 0xF1, 0xFD, 0x1A, 0x78, 0xDB, 0x00, 0xEE, 
  0xD6, 0x80, 0xE1, 0x90, 0xFF, 0x90, 0x40, 0x1F, 0x04, 0xBF, 0xC4, 0xCB, 0x0A, 0xF0, 0xB8, 0x6E, 
  0xDA, 0xDC, 0xF7, 0x0B, 0xE9, 0xA4, 0xB1, 0xC3, 0x7E, 0x04, 0x00, 0x00, 
};
