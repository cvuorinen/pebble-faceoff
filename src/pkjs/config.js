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
				"defaultValue": "Date Style"
			},
			{
				"type": "toggle",
				"messageKey": "SHOW_DATE",
				"label": "Show Date",
				"defaultValue": true
			},
			{
				"type": "color",
				"capabilities": ["COLOR"],
				"messageKey": "WDAY_COLOR",
				"label": "Day of Week Color",
				"defaultValue": "0xFFFFFF"
			},
			{
				"type": "color",
				"capabilities": ["COLOR"],
				"messageKey": "MDAY_COLOR",
				"label": "Day of Month Color",
				"defaultValue": "0xFFFFFF"
			}
		]
	},
	{
		"type": "submit",
		"defaultValue": "Save Settings"
	}
]
