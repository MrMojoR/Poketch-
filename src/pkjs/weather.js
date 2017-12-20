// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
    console.log('AppMessage received!');
    getWeather();
  }                     
);

var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};


function getWeather() {
   // Construct URL
  var url = 'https://api.darksky.net/forecast/9206a49f3f5b989f4d5e6bf840c39ee0/46.0642927,18.1927753?exclude=[minutely,hourly,daily,alerts,flags]&units=si';

  // Send request to OpenWeatherMap
  xhrRequest(url, 'GET', 
    function(responseText) {
      // responseText contains a JSON object with weather info
      var json = JSON.parse(responseText);

      // Temperature in Kelvin requires adjustment
      var temp = json.currently.apparentTemperature;
      var temperature = temp>0?Math.floor(temp):Math.ceil(temp);
      console.log('Temperature is ' + temperature);
      
      var rest = Math.abs((temp*10)%10);
      
      console.log('REST is ' + rest);

      // Conditions
      var conditions = json.currently.summary;      
      console.log('Conditions are ' + conditions);
      // Assemble dictionary using our keys
var dictionary = {
  'TEMPERATURE': temperature,
  'REST':rest,
  'CONDITIONS': conditions
};

// Send to Pebble
Pebble.sendAppMessage(dictionary,
  function(e) {
    console.log('Weather info sent to Pebble successfully!');
  },
  function(e) {
    console.log('Error sending weather info to Pebble!');
  }
);
    }      
  );
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log('PebbleKit JS ready!');

    // Get the initial weather
    getWeather();
  }
);

