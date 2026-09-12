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
			},
				{
					"type": "radiogroup",
					"messageKey": "TOP_COMPLICATION",
					"label": "Top Right",
					"defaultValue": "1",
					"options": [
						{
							"label": "Nothing",
							"value": "0"
						},
						{
							"label": "Weather",
							"value": "1"
						},
						{
							"label": "Day of week",
							"value": "2"
						},
						{
							"label": "Date",
							"value": "3"
						},
						{
							"label": "Day of week + date",
							"value": "4"
						}
					]
				},
				{
					"type": "radiogroup",
					"messageKey": "BOTTOM_COMPLICATION",
					"label": "Bottom Left",
					"defaultValue": "3",
					"options": [
						{
							"label": "Nothing",
							"value": "0"
						},
						{
							"label": "Weather",
							"value": "1"
						},
						{
							"label": "Day of week",
							"value": "2"
						},
						{
							"label": "Date",
							"value": "3"
						},
						{
							"label": "Day of week + date",
							"value": "4"
						}
					]
				},
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
		"type": "submit",
		"defaultValue": "Save Settings"
	}
]
