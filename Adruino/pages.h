const char index_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en">
<head>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>WiFi Waterbed Heater</title>
<style type="text/css">
table{
border-radius: 2px;
box-shadow: 2px 2px 12px #000000;
background-image: -moz-linear-gradient(top, #f0f0f0, #506090);
background-image: -ms-linear-gradient(top, #f0f0f0, #506090);
background-image: -o-linear-gradient(top, #f0f0f0, #506090);
background-image: -webkit-linear-gradient(top, #f0f0f0, #506090);
background-image: linear-gradient(top, #f0f0f0, #506090);
background-clip: padding-box;
}
input{
border-radius: 5px;
box-shadow: 2px 2px 12px #000000;
background-image: -moz-linear-gradient(top, #dfffff, #505050);
background-image: -ms-linear-gradient(top, #dfffff, #505050);
background-image: -o-linear-gradient(top, #dfffff, #505050);
background-image: -webkit-linear-gradient(top, #dfffff, #505050);
background-image: linear-gradient(top, #dfffff, #505050);
background-clip: padding-box;
}
div{
border-radius: 5px;
box-shadow: 2px 2px 12px #000000;
background-image: -moz-linear-gradient(top, #df9f0f, #f050ff);
background-image: -ms-linear-gradient(top, #df9f0f, #f050ff);
background-image: -o-linear-gradient(top, #df9f0f, #f050ff);
background-image: -webkit-linear-gradient(top, #f0a040, #505050);
background-image: linear-gradient(top, #df9f0f, #f050ff);
background-clip: padding-box;
}
body{width:300px;display:block;margin-left:auto;margin-right:auto;text-align:right;font-family: Arial, Helvetica, sans-serif;}}
</style>
<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.6.1/jquery.min.js" type="text/javascript" charset="utf-8"></script>
<script type="text/javascript" src="data"></script>
<script type="text/javascript">
// src="http://192.168.31.86/data"
a=document.all
oledon=0
avg=1
cnt=1
eco=0
cf='F'
function openSocket(){
ws=new WebSocket("ws://"+window.location.host+"/ws")
//ws=new WebSocket("ws://192.168.31.158/ws")
ws.onopen=function(evt) { }
ws.onclose=function(evt) { alert("Connection closed."); }
ws.onmessage=function(evt) {
  console.log(evt.data)
 d=JSON.parse(evt.data)
 if(d.cmd=='state')
 {
  dt=new Date(d.t*1000)
  a.time.innerHTML=dt.toLocaleTimeString()
  waterTemp=+d.waterTemp
  a.temp.innerHTML=(d.on?"<font color='red'><b>":"")+waterTemp+"&deg"+cf+(d.on?"</b></font>":"")
  a.tgt.innerHTML="> "+d.hiTemp+"&deg"+cf
  a.rt.innerHTML=+d.temp+'&deg'+cf
  a.rh.innerHTML=+d.rh+'%'
  a.eta.innerHTML='ETA '+t2hms(+d.eta)
 }
 else if(d.cmd=='set')
 {
  a.vt.value=+d.vt
  oledon=d.o
  a.OLED.value=oledon?'ON ':'OFF'
  avg=d.avg
  a.AVG.value=avg?'ON ':'OFF'
  a.tz.value=d.tz
  ppkw=d.ppkwh/1000
  a.K.value=ppkw
  a.vo.value=d.vo?'ON ':'OFF'
  a.eco.value=d.e?'ON ':'OFF'
  eco=d.e
  watts=d.w
  cnt=d.cnt
  idx=d.idx
  itms=d.item
  for(i=0;i<8;i++){
   item=document.getElementById('r'+i); item.setAttribute('style',i<cnt?'':'display:none')
   if(i==idx) item.setAttribute('style','background-color:red')
   document.getElementById('N'+i).value=itms[i][0]
   h=Math.floor(itms[i][1]/60)
   m=itms[i][1]%60
   if(m<10) m='0'+m
   document.getElementById('S'+i).value=h+':'+m
   document.getElementById('T'+i).value=itms[i][2]
   document.getElementById('H'+i).value=itms[i][3]
   document.getElementById('N'+i).onchange=function(){onChangeSched(this)}
   document.getElementById('S'+i).onchange=function(){onChangeSched(this)}
   document.getElementById('T'+i).onchange=function(){onChangeSched(this)}
   document.getElementById('H'+i).onchange=function(){onChangeSched(this)}
  }
  a.inc.value=Math.min(8,cnt+1)
  a.dec.value=Math.max(1,cnt-1)
  draw_bars(d.ts,d.ppkwm)
  draw()
 }
 else if(d.cmd=='alert')
 {
  alert(d.data)
 }
}
}
function onChangeSched(ent)
{
 id=ent.id.substr(0,1)
 idx=ent.id.substr(1)
 val=ent.value
 if(id=='S')
 {
tm=val.split(':')
val=(+tm[0]*60)+(+tm[1])
 }
 setVar('I',idx)
 setVar(id,val)
 draw()
}
function setVar(varName, value)
{
 ws.send('{"key":"'+a.myKey.value+'","'+varName+'":'+value+'}')
}
function oled(){
setVar('oled',oledon?0:1)
}
function setavg(){avg=!avg
setVar('avg',avg?1:0)
draw()
}
function setTZ(){
setVar('TZ',a.tz.value)}
function setTemp(n){
setVar('tadj',n)
}
function setAllTemp(n){
setVar('aadj',n)
}
function setEco(n){
eco=!eco
setVar('eco',eco?1:0)
}
function setVaca(){
setVar('vacatemp',a.vt.value)
setVar('vaca',(a.vo.value=='OFF')?1:0)
}
function setCnt(n){
cnt+=n
setVar('cnt',cnt)
}
function setPPK(){
setVar('ppkwh',(a.K.value*1000).toFixed())
}

var x,y,llh=0,llh2=0
function draw(){
try {
  c2 = document.getElementById('canva')
  ctx = c2.getContext("2d")
  ctx.fillStyle = "#88f" // bg
  ctx.fillRect(0,0,c2.width,c2.height)

  ctx.font = "bold 11px sans-serif"
  ctx.lineWidth = 1
  getLoHi()
  ctx.strokeStyle = "#000"
  ctx.fillStyle = "#000"
  if(avg)
  {
ctx.beginPath()
m=c2.width-tm2x(itms[cnt-1][1])
r=m+tm2x(itms[0][1])
tt1=tween(+itms[cnt-1][2],+itms[0][2],m,r) // get y of midnight
th1=tween(+itms[cnt-1][2]-itms[cnt-1][3],+itms[0][2]-itms[0][3],m,r) // thresh
ctx.moveTo(0, t2y(tt1)) //1st point
for(i=0;i<cnt;i++){
x=tm2x(itms[i][1]) // time to x
  linePos(+itms[i][2])
}
i--;
  x = c2.width
  linePos(tt1) // temp at mid
  linePos(th1) // thresh tween
for(;i>=0;i--){
x=tm2x(itms[i][1])
  linePos(itms[i][2]-itms[i][3])
}
x=0
  linePos(th1) // thresh at midnight
ctx.closePath()
ctx.fillStyle = "rgba(80,80,80,0.5)" // thresholds
ctx.fill()
ctx.fillStyle = "#000" // temps
for(i=0;i<cnt;i++){
x=tm2x(itms[i][1]) // time to x
ctx.fillText(itms[i][2],x,t2y(+itms[i][2])-4)
}
  }
  else
  {
for(i=0;i<cnt;i++)
{
x=tm2x(itms[i][1])
y=t2y(itms[i][2]) // temp
y2=t2y(itms[i][2]-itms[i][3])-y // thresh
if(i==cnt-1) x2=c2.width-x
else x2=tm2x(itms[i+1][1])-x
ctx.fillStyle = "rgba(80,80,80,0.4)" // thresh
ctx.fillRect(x, y, x2, y2)
ctx.fillStyle = "#000" // temp
ctx.fillText(itms[i][2], x, y-2)
}
x0=tm2x(itms[0][1])
ctx.fillStyle = "#666"
ctx.fillRect(0, y, x0, y2) // rollover midnight
  }

  ctx.fillStyle = "#000" // temps
  step=c2.width/cnt
  y = c2.height
  ctx.fillText(12,c2.width/2-6, y)//hrs
  ctx.fillText(6, c2.width/4-3, y)
  ctx.fillText(18,c2.width/2+c2.width/4-6, y)
  getLoHi()
  ctx.fillStyle = "#337" // temp scale
  ctx.fillText(hi,0,10)
  ctx.fillText(lo,0,c2.height)
  hl=hi-lo
  ctx.strokeStyle = "rgba(13,13,13,0.1)" // h-lines
  step=c2.height/hl
  ctx.beginPath()
  for(y=step,n=hi-1;y<c2.height;y+= step){  // vertical lines
    ctx.moveTo(14,y)
    ctx.lineTo(c2.width,y)
ctx.fillText(n--,0,y+3)
  }
  ctx.stroke()
  step = c2.width/24
  ctx.beginPath()
  for (x = step; x < c2.width; x += step){ // vertical lines
    ctx.moveTo(x,0)
    ctx.lineTo(x,c2.height)
  }
  ctx.stroke()
  ctx.beginPath()
  ctx.strokeStyle="#0D0"
  brk=false
  for(i=0;i<tdata.length;i++) // rh
  {
if(tdata[i].t==0) continue
x=tm2x(tdata[i].tm)
y=c2.height-(tdata[i].rh/100*c2.height)
ctx.lineTo(x,y)
ctx.stroke()
ctx.beginPath()
if(tdata[i].s!=2)
ctx.moveTo(x,y)
  }

  ctx.beginPath()
  ctx.strokeStyle="#834"
  for(i=0;i<tdata.length;i++) // room temp
  {
if(tdata[i].t==0) continue
x=tm2x(tdata[i].tm)
y=t2y(tdata[i].rm)
ctx.lineTo(x,y)
ctx.stroke()
ctx.beginPath()
if(tdata[i].s!=2)
ctx.moveTo(x,y)
  }

  ctx.beginPath()
  for(i=0;i<tdata.length;i++) // hist data
  {
if(tdata[i].t==0) continue;
x=tm2x(tdata[i].tm)
y=t2y(tdata[i].t)
ctx.lineTo(x,y)
ctx.stroke()
ctx.beginPath()
switch(+tdata[i].s)
{
case 0: ctx.strokeStyle="#00F"; ctx.moveTo(x,y); break //off
case 1: ctx.strokeStyle="#F00"; ctx.moveTo(x,y); break //on
case 2: break
}
  }
  dt=new Date()
  x=tm2x((dt.getHours()*60)+dt.getMinutes())
  ctx.beginPath()
  y=t2y(waterTemp)
  ctx.moveTo(x,y)
  ctx.lineTo(x-4,y+6)
  ctx.lineTo(x+4,y+6)
  ctx.closePath()
  ctx.fillStyle="#0f0" // arrow
  ctx.fill()
  ctx.stroke()
}catch(err){}
}
function linePos(t)
{
ctx.lineTo(x,t2y(t))
}
function tm2x(t)
{
return t*c2.width/1440
}
function t2y(t)
{
return c2.height-((t-lo)/(hi-lo)*c2.height)
}
function tween(t1, t2, m, r)
{
t=(t2-t1)*(m*100/r)/100
t+=t1
return t.toFixed(1)
}
function getLoHi()
{
  lo=99
  hi=60
  for(i=0;i<cnt;i++){
if(itms[i][2]>hi) hi=itms[i][2]
if(itms[i][2]-itms[i][3]<lo) lo=itms[i][2]-itms[i][3]
  }
  for(i=0;i<tdata.length;i++)
  {
if(tdata[i].t>hi) hi=tdata[i].t
if(tdata[i].rm>hi) hi=tdata[i].rm
if(tdata[i].t>0&&tdata[i].t<lo) lo=tdata[i].t
if(tdata[i].rm>0&&tdata[i].rm<lo) lo=tdata[i].rm
  }
  lo=Math.floor(lo)
  hi=Math.ceil(hi)
}
function draw_bars(ar,pp)
{
    graph = $('#graph')
var c=document.getElementById('graph')
rect=c.getBoundingClientRect()
canvasX=rect.x
canvasY=rect.y

    tipCanvas=document.getElementById("tip")
    tipCtx=tipCanvas.getContext("2d")
    tipDiv=document.getElementById("popup")

ctx=c.getContext("2d")
ctx.fillStyle="#FFF"
ctx.font="10px sans-serif"
ctx.lineCap="round"

    dots=[]
    date=new Date()
ht=c.height
ctx.lineWidth=10
draw_scale(ar,pp,c.width-4,ht-2,2,1,date.getMonth())

// request mousemove events
graph.mousemove(function(e){handleMouseMove(e);})

// show tooltip when mouse hovers over dot
function handleMouseMove(e){
rect=c.getBoundingClientRect()
mouseX=e.clientX-rect.x
mouseY=e.clientY-rect.y
var hit = false
for(i=0;i<dots.length;i++){
dot=dots[i]
if(mouseX>=dot.x && mouseX<=dot.x2 && mouseY>=dot.y && mouseY<=dot.y2){
tipCtx.clearRect(0,0,tipCanvas.width,tipCanvas.height)
tipCtx.fillStyle="#000000"
tipCtx.strokeStyle='#333'
tipCtx.font='italic 8pt sans-serif'
tipCtx.textAlign="left"
tipCtx.fillText(dot.tip, 4,15)
tipCtx.fillText(dot.tip2,4,29)
tipCtx.fillText(dot.tip3,4,44)
popup=document.getElementById("popup")
popup.style.top =(dot.y)+"px"
x=dot.x-60
if(x<10)x=10
popup.style.left=x+"px"
hit=true
}
}
if(!hit){popup.style.left="-1000px"}
}

function getMousePos(cDom, mEv){
rect = cDom.getBoundingClientRect();
return{
 x: mEv.clientX-rect.left,
 y: mEv.clientY-rect.top
}
}
}

function draw_scale(ar,pp,w,h,o,p,ct)
{
ctx.fillStyle="#336"
ctx.fillRect(2,o,w,h-3)
ctx.fillStyle="#FFF"
max=0
tot=0
costTot=0
for(i=0;i<ar.length;i++)
{
if(ar[i]>max) max=ar[i]
tot+=ar[i]
}
ctx.textAlign="center"
lw=ctx.lineWidth
clr='#55F'
mbh=0
bh=0
for(i=0;i<ar.length;i++)
{
x=i*(w/ar.length)+4+ctx.lineWidth
ctx.strokeStyle='#55F'
if(ar[i]){
    bh=ar[i]*(h-28)/max
    y=(o+h-20)-bh
ctx.beginPath()
    ctx.moveTo(x,o+h-22)
    ctx.lineTo(x,y)
ctx.stroke()
}
ctx.strokeStyle="#FFF"
ctx.fillText(i+p,x,o+h-7)

if(i==ct)
{
ctx.strokeStyle="#000"
ctx.lineWidth=1
ctx.beginPath()
    ctx.moveTo(x+lw+1,o+h-2)
    ctx.lineTo(x+lw+1,o+1)
ctx.stroke()
ctx.lineWidth=lw
}
bh=+bh.toFixed()+5
x=+x.toFixed()
if(pp[i]==0) pp[i]=ppkw*1000
cost=+((pp[i]/1000)*ar[i]*(watts/3600000)).toFixed(2)
costTot+=cost
if(ar[i])
  dots.push({
x: x-lw/2,
y: (o+h-20)-bh,
y2: (o+h),
x2: x+lw/2,
tip: t2hms(ar[i]),
tip2: '$'+cost,
tip3: '@ $'+pp[i]/1000
})
}
ctx.fillText((tot*watts/3600000).toFixed(1)+' KWh',w/2,o+10)
ctx.fillText('$'+costTot.toFixed(2),w/2,o+22)
}
function t2hms(t)
{
s=t%60
t=Math.floor(t/60)
if(t==0) return s
if(s<10) s='0'+s
m=t%60
t=Math.floor(t/60)
if(t==0) return m+':'+s
if(m<10) m='0'+m
h=t%24
t=Math.floor(t/24)
if(t==0) return h+':'+m+':'+s
return t+'d '+h+':'+m+':'+s
}
</script>
<style type="text/css">
#wrapper {
  width: 100%;
  height: 100px;
  position: relative;
}
#graph {
  width: 100%;
  height: 100%;
  position: relative;
}
#popup {
  position: absolute;
  top: -100px;
  left: -1000px;
  z-index: 10;
}
</style>
</head>
<body bgcolor="silver" onload="{
key=localStorage.getItem('key')
if(key!=null) document.getElementById('myKey').value=key
openSocket()
}">
<h3 align="center">WiFi Waterbed Heater</h3>
<table align=right>
<tr><td align=center id="time"></td><td align=center><div id="temp"></div> </td><td align=center><div id="tgt"></div></td></tr>
<tr><td align=center>Bedroom: </td><td align=center><div id="rt"></div></td><td align=center><div id="rh"></div> </td></tr>
<tr><td id="eta"></td><td>TZ <input id='tz' type=text size=2 value='-5' onchange="{setTZ()}"></td>
<td>Display:<input type="button" value="ON " id="OLED" onClick="{oled()}"></td></tr>
<tr><td colspan=2>Vacation <input id='vt' type=text size=2 value='-10'><input type='button' id='vo' onclick="{setVaca()}"> &nbsp &nbsp </td>
<td>Avg: <input type="button" value="OFF" id="AVG" onClick="{setavg()}"></td></tr>
<tr><td colspan=2></td>
<td>Eco: <input type="button" value="OFF" id="eco" onClick="{setEco()}"></td></tr>
<tr><td>Schedule <input id='inc' type='button' onclick="{setCnt(1)}">Count <input id='dec' type='button' onclick="{setCnt(-1)}"}></td>
<td> Temperature<br>Adjust</td>
<td><input type='button' value='Up' onclick="{setTemp(1)}"> <input type='button' value='All Up' onclick="{setAllTemp(1)}"><br><input type='button' value='Dn' onclick="{setTemp(-1)}"> <input type='button' value='All Dn' onclick="{setAllTemp(-1)}"></td></tr>
<tr><td align="left"> &nbsp; &nbsp;  &nbsp; Name</td><td>Time &nbsp;&nbsp;</td><td>Temp &nbsp;Thresh</td></tr>
<tr><td colspan=3 id='r0' style="display:none"><input id=N0 type=text size=12> <input id=S0 type=text size=3> <input id=T0 type=text size=3> <input id=H0 type=text size=2></td></tr>
<tr><td colspan=3 id='r1' style="display:none"><input id=N1 type=text size=12> <input id=S1 type=text size=3> <input id=T1 type=text size=3> <input id=H1 type=text size=2></td></tr>
<tr><td colspan=3 id='r2' style="display:none"><input id=N2 type=text size=12> <input id=S2 type=text size=3> <input id=T2 type=text size=3> <input id=H2 type=text size=2></td></tr>
<tr><td colspan=3 id='r3' style="display:none"><input id=N3 type=text size=12> <input id=S3 type=text size=3> <input id=T3 type=text size=3> <input id=H3 type=text size=2></td></tr>
<tr><td colspan=3 id='r4' style="display:none"><input id=N4 type=text size=12> <input id=S4 type=text size=3> <input id=T4 type=text size=3> <input id=H4 type=text size=2></td></tr>
<tr><td colspan=3 id='r5' style="display:none"><input id=N5 type=text size=12> <input id=S5 type=text size=3> <input id=T5 type=text size=3> <input id=H5 type=text size=2></td></tr>
<tr><td colspan=3 id='r6' style="display:none"><input id=N6 type=text size=12> <input id=S6 type=text size=3> <input id=T6 type=text size=3> <input id=H6 type=text size=2></td></tr>
<tr><td colspan=3 id='r7' style="display:none"><input id=N7 type=text size=12> <input id=S7 type=text size=3> <input id=T7 type=text size=3> <input id=H7 type=text size=2></td></tr>
<tr><td colspan=3><canvas id="canva" width="300" height="140" style="border:1px dotted;float:center" onclick="draw()"></canvas></td></tr>
<tr><td colspan=3>
<div id="wrapper">
<canvas id="graph" width="300" height="100"></canvas>
<div id="popup"><canvas id="tip" width="70" height="48"></canvas></div>
</div>
</td></tr>
<tr><td colspan=2 align="left">PPKWH $<input id='K' type=text size=2 value='0.1457' onchange="{setPPK()}"> </td><td> <input id="myKey" name="key" type=text size=40 placeholder="password" style="width: 100px" onChange="{localStorage.setItem('key', key = document.all.myKey.value)}"></td></tr>
</table>
</body>
</html>
)rawliteral";

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
