#include <WiFi.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Document</title>
</head>
<body>
    <header>
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
        <h1>Time:<span id="time-disp"></span></h1><br><br>
    </main>
</body>
<script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var ws;
    window.addEventListener('load', onLoad);
    function initWebSocket(){
        ws = new WebSocket(gateway);
        ws.onclose = onClose;
        ws.onmessage = onMessage;
    }
    function onClose(e){
        setTimeout(initWebSocket, 2000);
    }
    function onLoad(e){
        initWebSocket();
    }
    var time = new Date("%DATETIME%");
    Date.prototype.tick = function(){
        this.setTime(this.getTime() + 1000);
        return this
    }
    function onMessage(e){
        time.tick();
        document.querySelector("#time-disp").innerHTML = time.toLocaleString();
    }
</script>
</html>
)rawliteral";