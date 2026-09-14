// Complication pickers are built rather than written out: the health readings
// only exist on a watch that has a HealthService, and Clay gates whole items,
// not individual options. Two items may share a messageKey as long as their
// capabilities cannot both be satisfied, so each slot is emitted twice -- once
// for health watches and once for the rest.
var COMPLICATIONS = [
	{ "label": "Nothing", "value": "0" },
	{ "label": "Weather", "value": "1" },
	{ "label": "Day of week", "value": "2" },
	{ "label": "Date", "value": "3" },
	{ "label": "Day of week + date", "value": "4" }
];

var HEALTH_COMPLICATIONS = [
	{ "label": "Steps", "value": "5" },
	{ "label": "Heart rate", "value": "6" }
];

function complicationPicker(messageKey, label, defaultValue, withHealth) {
	return {
		"type": "select",
		"messageKey": messageKey,
		"label": label,
		"defaultValue": defaultValue,
		"capabilities": [withHealth ? "HEALTH" : "NOT_HEALTH"],
		"options": withHealth
			? COMPLICATIONS.concat(HEALTH_COMPLICATIONS)
			: COMPLICATIONS
	};
}

function complicationPickers(messageKey, label, defaultValue) {
	return [
		complicationPicker(messageKey, label, defaultValue, true),
		complicationPicker(messageKey, label, defaultValue, false)
	];
}

module.exports = [
	{
		"type": "heading",
		"defaultValue": "Settings"
	},
	{
		"type": "section",
		"capabilities": ["COLOR"],
		"items": [
			{
				"type": "heading",
				"defaultValue": "Background Style"
			},
			{
				"type": "color",
				"messageKey": "TOP_STRIPE_COLOR",
				"label": "Top Stripe Color",
				"defaultValue": "0xAA0055"
			},
			{
				"type": "color",
				"messageKey": "BOTTOM_STRIPE_COLOR",
				"label": "Bottom Stripe Color",
				"defaultValue": "0x5555FF"
			},
			{
				"type": "color",
				"messageKey": "BACKGROUND_COLOR",
				"label": "Background Color",
				"defaultValue": "0x000000"
			},
			{
				"type": "toggle",
				"messageKey": "FILL_CORNERS",
				"label": "Fill Corners (Square Watches Only)",
				"defaultValue": false
			}
		]
	},
	{
		"type": "section",
		"capabilities": ["BW"],
		"items": [
			{
				"type": "heading",
				"defaultValue": "Background Style"
			},
			{
				"type": "radiogroup",
				"messageKey": "BW_STRIPE_STYLE",
				"label": "Stripes",
				"defaultValue": "0",
				"options": [
					{
						"label": "Dark top, light bottom",
						"value": "0"
					},
					{
						"label": "Light top, dark bottom",
						"value": "1"
					}
				]
			},
			{
				"type": "toggle",
				"messageKey": "FILL_CORNERS",
				"label": "Fill Corners (Square Watches Only)",
				"defaultValue": false
			}
		]
	},
	{
		"type": "section",
		"items": [
			{
				"type": "heading",
				"defaultValue": "Time Style"
			},
			{
				"type": "radiogroup",
				"messageKey": "TIME_FORMAT",
				"label": "Time format",
				"defaultValue": "0",
				"options": [
					{
						"label": "Use system setting",
						"value": "0"
					},
					{
						"label": "12-hour",
						"value": "1"
					},
					{
						"label": "24-hour",
						"value": "2"
					}
				]
			},
			{
				"type": "color",
				"capabilities": ["COLOR"],
				"messageKey": "HOUR_COLOR",
				"label": "Hour Color",
				"defaultValue": "0xFFFFFF"
			},
			{
				"type": "color",
				"capabilities": ["COLOR"],
				"messageKey": "MINUTE_COLOR",
				"label": "Minute Color",
				"defaultValue": "0xFFFFFF"
			}
		]
	},
	{
		"type": "section",
		"items": [
			{
				"type": "heading",
				"defaultValue": "Complications"
			},
			{
				"type": "text",
				"defaultValue": "The two corners beside the time. Weather needs a location fix from your phone."
			}
		].concat(
			complicationPickers("TOP_COMPLICATION", "Top Right", "1"),
			complicationPickers("BOTTOM_COMPLICATION", "Bottom Left", "3"),
			[
				{
					"type": "color",
					"capabilities": ["COLOR"],
					"messageKey": "WDAY_COLOR",
					"label": "Top Right Color",
					"defaultValue": "0xFFFFFF"
				},
				{
					"type": "color",
					"capabilities": ["COLOR"],
					"messageKey": "MDAY_COLOR",
					"label": "Bottom Left Color",
					"defaultValue": "0xFFFFFF"
				}
			]
		)
	},
	{
		"type": "section",
		"items": [
			{
				"type": "heading",
				"group": "weather",
				"defaultValue": "Weather"
			},
			{
				"type": "radiogroup",
				"group": "weather",
				"messageKey": "TEMPERATURE_UNIT",
				"label": "Temperature unit",
				"defaultValue": "0",
				"options": [
					{
						"label": "Celsius",
						"value": "0"
					},
					{
						"label": "Fahrenheit",
						"value": "1"
					}
				]
			}
		]
	},
	{
		"type": "section",
		"items": [
			{
				"type": "heading",
				"defaultValue": "Animation"
			},
			{
				"type": "toggle",
				"messageKey": "INTRO_ANIMATION",
				"label": "Slide in on launch",
				"defaultValue": true,
				"description": "The two halves slide in when the watchface opens. Takes effect the next time it does."
			},
			{
				"type": "toggle",
				"messageKey": "TICK_ANIMATION",
				"label": "Slide on time change",
				"defaultValue": false,
				"description": "Animate every time change, old minute/hour slides off and the new one in."
			}
		]
	},
	{
		"type": "submit",
		"defaultValue": "Save Settings"
	}
]
