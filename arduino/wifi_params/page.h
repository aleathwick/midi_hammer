const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<style>
.card{
    max-width: 400px;
     min-height: 250px;
     background: #02b875;
     padding: 30px;
     box-sizing: border-box;
     color: #FFF;
     margin:20px;
     box-shadow: 0px 2px 18px -4px rgba(0,0,0,0.75);
}
</style>
<body>

<div class="card">
  <h4>A simple timer, updating without refresh</h4><br>
  <h1>Sensor Value:<span id="ADCValue">0</span></h1><br>
  <form action="/control">
    input1: <input type="text" name="input1">
    <input type="submit" value="Submit">
  </form><br>
</div>
<script>

setInterval(function() {
  // Call a function repetatively with 30 ms interval
  getData();
}, 30); //30mSeconds update rate

function getData() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("ADCValue").innerHTML =
      this.responseText;
    }
  };
  xhttp.open("GET", "readADC", true);
  xhttp.send();
}
</script>
</body>
</html>
)=====";