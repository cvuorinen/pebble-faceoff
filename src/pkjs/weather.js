// Weather for the complication, from Open-Meteo: no API key, no account, and
// it answers with everything the face needs in one request.
//
// The watch asks (WEATHER_REQUEST) rather than the phone pushing on a timer,
// because the phone side only runs while the watchface is on screen and has no
// reliable clock of its own. Temperature always goes back in Celsius; the watch
// converts, so switching units costs nothing.

// Kept in step with WeatherCondition in src/c/weather.h.
var CONDITIONS = {
  CLEAR_DAY: 0,
  CLEAR_NIGHT: 1,
  PARTLY_CLOUDY_DAY: 2,
  PARTLY_CLOUDY_NIGHT: 3,
  CLOUDY: 4,
  RAIN: 5,
  SNOW: 6,
  FOG: 7,
  THUNDERSTORM: 8,
  UNKNOWN: 9
};

// WMO present weather codes, which is what Open-Meteo reports. There are far
// more of them than there are icons, so each lands on its nearest neighbour:
// drizzle and showers read as rain, ice pellets as snow, and so on.
var WMO = {
  0: 'CLEAR', 1: 'CLEAR',
  2: 'PARTLY_CLOUDY',
  3: 'CLOUDY',
  45: 'FOG', 48: 'FOG',
  51: 'RAIN', 53: 'RAIN', 55: 'RAIN',
  56: 'RAIN', 57: 'RAIN',
  61: 'RAIN', 63: 'RAIN', 65: 'RAIN',
  66: 'RAIN', 67: 'RAIN',
  71: 'SNOW', 73: 'SNOW', 75: 'SNOW', 77: 'SNOW',
  80: 'RAIN', 81: 'RAIN', 82: 'RAIN',
  85: 'SNOW', 86: 'SNOW',
  95: 'THUNDERSTORM', 96: 'THUNDERSTORM', 99: 'THUNDERSTORM'
};

var API = 'https://api.open-meteo.com/v1/forecast';
var REQUEST_TIMEOUT_MS = 20000;
var POSITION_TIMEOUT_MS = 15000;
// A fix from within the last ten minutes is close enough for a whole-degree
// temperature, and saves waking the GPS.
var POSITION_MAX_AGE_MS = 10 * 60 * 1000;

// Only one request in flight: the watch asks again on a retry timer, and two
// overlapping fixes would just race each other to the same answer.
var pending = false;

function conditionFor(code, isDay) {
  var group = WMO[code];
  if (!group) {
    return CONDITIONS.UNKNOWN;
  }
  if (group === 'CLEAR' || group === 'PARTLY_CLOUDY') {
    return CONDITIONS[group + (isDay ? '_DAY' : '_NIGHT')];
  }
  return CONDITIONS[group];
}

function send(current) {
  var code = current.weather_code;
  var temperature = current.temperature_2m;
  if (typeof temperature !== 'number' || isNaN(temperature)) {
    return;
  }

  Pebble.sendAppMessage({
    // Tenths of a degree, so the watch can convert to Fahrenheit and still
    // round to the right whole number.
    WEATHER_TEMPERATURE: Math.round(temperature * 10),
    WEATHER_CONDITION: conditionFor(code, current.is_day !== 0)
  }, function () {}, function (e) {
    console.log('faceoff: could not send weather: ' + JSON.stringify(e));
  });
}

function request(position) {
  var url = API +
    '?latitude=' + position.coords.latitude.toFixed(3) +
    '&longitude=' + position.coords.longitude.toFixed(3) +
    '&current=temperature_2m,weather_code,is_day' +
    '&temperature_unit=celsius';

  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  xhr.timeout = REQUEST_TIMEOUT_MS;
  xhr.onload = function () {
    pending = false;
    if (xhr.status !== 200) {
      console.log('faceoff: forecast returned ' + xhr.status);
      return;
    }
    var body;
    try {
      body = JSON.parse(xhr.responseText);
    } catch (e) {
      console.log('faceoff: forecast was not JSON');
      return;
    }
    if (body && body.current) {
      send(body.current);
    }
  };
  xhr.onerror = xhr.ontimeout = function () {
    pending = false;
    console.log('faceoff: forecast request failed');
  };
  xhr.send();
}

// Nothing is sent when a fetch fails. The watch keeps showing the reading it
// has until that goes stale, and asks again on its own retry timer.
function fetch() {
  if (pending) {
    return;
  }
  pending = true;
  navigator.geolocation.getCurrentPosition(request, function (error) {
    pending = false;
    console.log('faceoff: no position: ' + error.message);
  }, {
    timeout: POSITION_TIMEOUT_MS,
    maximumAge: POSITION_MAX_AGE_MS
  });
}

module.exports = { fetch: fetch, CONDITIONS: CONDITIONS, conditionFor: conditionFor };
