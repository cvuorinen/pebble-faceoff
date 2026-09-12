var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var customClay = require('./custom-clay');
var weather = require('./weather');

var clay = new Clay(clayConfig, customClay);

Pebble.addEventListener('ready', function () {
  weather.fetch();
});

Pebble.addEventListener('appmessage', function (e) {
  if (e.payload && e.payload.WEATHER_REQUEST !== undefined) {
    weather.fetch();
  }
});
