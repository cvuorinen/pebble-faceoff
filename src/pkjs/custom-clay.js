// Runs on the generated config page, not here: Clay injects it by calling
// .toString() on it, so it can close over nothing and must require nothing.

module.exports = function () {
  var clayConfig = this;

  // Kept in step with Complication in src/c/settings.h. Radiogroup values come
  // back as strings.
  var WEATHER = '1';
  var SLOTS = ['TOP_COMPLICATION', 'BOTTOM_COMPLICATION'];

  function weatherIsShowing() {
    for (var i = 0; i < SLOTS.length; i++) {
      var slot = clayConfig.getItemByMessageKey(SLOTS[i]);
      if (slot && String(slot.get()) === WEATHER) {
        return true;
      }
    }
    return false;
  }

  // Clay cannot hide a section, only the items in one, hence the group.
  function syncWeatherSection() {
    var showing = weatherIsShowing();
    clayConfig.getItemsByGroup('weather').forEach(function (item) {
      if (showing) {
        item.show();
      } else {
        item.hide();
      }
    });
  }

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    syncWeatherSection();
    SLOTS.forEach(function (key) {
      var slot = clayConfig.getItemByMessageKey(key);
      if (slot) {
        slot.on('change', syncWeatherSection);
      }
    });
  });
};
