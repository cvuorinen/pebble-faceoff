var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var weather = require('./weather');

var clay = new Clay(clayConfig);

Pebble.addEventListener('ready', function () {
  weather.fetch();
});

Pebble.addEventListener('appmessage', function (e) {
  if (e.payload && e.payload.WEATHER_REQUEST !== undefined) {
    weather.fetch();
  }
});
