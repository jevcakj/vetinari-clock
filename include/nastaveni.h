#include <WiFi.h>

const char nastaveni_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Hodiny</title>
    <style>
        #range{
            border-style: solid;
            padding: 5px;
            margin: 5px;
        }
    </style>
</head>
<body>
    <header style="margin: 5px;">
        <nav>
            <a href="/">
                <button>Domů</button>
            </a>
            <a href="nastaveni.html">
                <button>Nastavení</button>
            </a>
        </nav>
    </header>
    <main>
        <div>
            <form id="form">
                <label for="time">Změnit čas:</label><br>
                <input type="datetime-local" name="time" id="time">
                <br><br>
                <div id="ranges"></div>
                <button type="button" id="add_range">Přidat časový úsek</button>
                <br><br>
                <input type="reset">
                <input type="submit">
            </form>
        </div>
    </main>
    
    <script>
        const hostname = window.location.hostname;
        function add_range(e=null, start_time='09:00', end='11:00', min='5', sec='0', radio=null){
            const renges = document.querySelector('#ranges');
            const range = `
                <label for="stime">Začátek:</label>
                <input type="time" name="stime" id="stime" required>
                <label for="etime">Konec:</label>
                <input type="time" name="etime" id="etime">
                <br>

                <label>Odchylka:</label><br>
                <label for="diff_min">Minuty</label>
                <input type="number" min="0" max="59" id="diff_min" name="diff_min">
                <label for="diff_sec">Vteřiny</label>
                <input type="number" min="0" max="59" id="diff_sec" name="diff_sec">
                <br>
                <label for="speed">Rychlost:</label>
                
                <label for="slow">-</label>
                <input type="radio" name="speed" id="slow" value="0" required>

                <label for="fast">+</label>
                <input type="radio" name="speed" id="fast" value="1">
                <br>
                <button type="button" id="remove_range">Odebrat</button>
            `;
            const r = document.createElement('div');
            r.innerHTML = range;
            r.setAttribute("id", "range");
            r.querySelector("#remove_range").addEventListener('click', ()=>{
                r.remove();
            });
            const stime = r.querySelector("#stime");
            stime.value = start_time;
            const etime = r.querySelector("#etime");
            etime.value = end;
            r.querySelector("#diff_min").value = min;
            r.querySelector("#diff_sec").value = sec;
            
            if(radio == '0'){
              r.querySelector('#slow').checked = true;
            }  
            else if(radio == '1'){
              r.querySelector('#fast').checked = true;
            }
            renges.appendChild(r);
        }
        const add_btn = document.querySelector('#add_range');
        add_btn.addEventListener('click', add_range);
        function isRangeValid(range){
            const stime = range.querySelector("#stime");
            const etime = range.querySelector("#etime");
            const fast = range.querySelector('#fast');
            const slow = range.querySelector('#slow');
            
            if(stime.value >= etime.value){
                console.log("zacatek pozdeji nez konec");
                return false;
            }
            else if(fast.value == false && slow.value == false){
                console.log("neni vybrana rychlost");
                return false;
            }
            else return true;
        }
        document.querySelector("#form").addEventListener('submit', (e)=>{
            e.preventDefault();
            var data = {
                time: e.target.time.value,
                ranges: []
            }
            for(const range of document.querySelector('#form').querySelectorAll('#range')){
                if(!isRangeValid(range)){
                    e.preventDefault();
                    return;
                }
                data.ranges.push({
                    stime:  e.target.stime.value,
                    etime:  e.target.etime.value,
                    min:    e.target.diff_min.value,
                    sec:    e.target.diff_sec.value,
                    speed:  e.target.speed.value
                });
            }
                console.log(data);
            fetch('/data',{
                method: "POST",
                headers:{
                    "Content-Type": "application/json"
                },
                body: JSON.stringify(data)
            }).then(response =>{
                if(!response.ok){
                    throw new Error(`Odpověď: ${response.status}`);
                }
                return response.text();
            }).then(result => {
                console.log(result);
            }).catch(error => {
                console.error(error.message);
            })
            window.location.href = '/';
        })
        async function get_config(){
            fetch('/config').then(response =>{
                if(!response.ok){
                    throw new Error(`Odpověď: ${response.status}`);
                }
                return response.json();
            }).then(result =>{
                const form = document.querySelector('#form');
                form.querySelector('#time').value = result.time;
                for(const range of result.ranges){
                    add_range(null, range.stime, range.etime, range.min, range.sec, range.speed);
                }
            }).catch(error =>{
                console.error(error.message);
            })
        }
        document.querySelector('#form').addEventListener('reset', (e)=>{
            document.querySelector('#ranges').innerHTML = '';
            get_config();
        })
        window.addEventListener("load", (e)=>{
          get_config();
        })
    </script>
</body>
</html>
)rawliteral";