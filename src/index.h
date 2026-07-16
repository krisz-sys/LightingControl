const char index_html[] PROGMEM = R"=====(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP Web Server</title>
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 2.3rem;}
    p {font-size: 1.9rem;}
    body {max-width: 400px; margin:0px auto; padding-bottom: 25px;}
    .sliderStyle { -webkit-appearance: none; margin: 14px; width: 360px; height: 20px; background: #d3d3d3; border-radius: 10px;
      outline: none; -webkit-transition: .2s; transition: opacity .2s;}
    .sliderStyle::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 35px; height: 35px; border-radius: 10px; background: #04AA6D ;  cursor: pointer;}
    .sliderStyle::-moz-range-thumb { width: 35px; height: 35px; border-radius: 10px; background: #04AA6D; cursor: pointer; } 
  </style>
</head>
<body>
  <h2>ESP Web Server</h2>
  <p><span id="textSliderValue">%SLIDERVALUE%</span></p>
 <p><input type="range" id="slider" min="0" max="100" value="0" class="sliderStyle"></p>
  
  <span id="room1Temp"></span>
  <button id="myBtn" value="ON" onclick="ButtonFunction()">D2 ON</button><br><br><br>

<script>

	function ButtonFunction() {
	  var x = button.value;
	  if(x=="ON"){
		button.value="OFF";
		button.innerText="D2 OFF";
	  }
	  else{
		button.value="ON";
		button.innerText="D2 ON";
	  }
	  console.log(button.value);
	}
	// <p><input type="range" onchange="updateSliderPWM(this)" id="slider" min="0" max="100" value="%SLIDERVALUE%" step="1" class="slider"></p>
	
	const ws = new WebSocket('ws://' + window.location.host + '/ws');
	const slider = document.getElementById('slider');
	const temperature=document.getElementById('room1Temp');
	const sliderValText=document.getElementById('textSliderValue');
	const button=document.getElementById("myBtn");
	
	
	ws.onmessage = (event) => {					// geting data from ESP8266, for testing in console: ws.onmessage({ data: "-3, 22.1, 12:25, 12" });
        const myArray = event.data.split(", ");
        temperature.textContent=myArray[1]+" °C";

        let sliderValue = Number(myArray[0].trim());
        if(sliderValue<0){
			    sliderValue=0;
		}
		sliderValText.textContent=sliderValue;
		slider.value=sliderValue;
	}
	
	slider.addEventListener('input', () => {					// sending slider data to ESP8266
		let sliderValue=slider.value
		const msg="Slider: "+ sliderValue.toString();
		console.log(msg);
        ws.send(msg);
	});
	
	button.addEventListener('click', () => {					// sending button data to ESP8266
			const msg="Button: "+button.value;
			console.log(msg);
			ws.send(msg);
	});

</script>
</body>
</html>
)=====";