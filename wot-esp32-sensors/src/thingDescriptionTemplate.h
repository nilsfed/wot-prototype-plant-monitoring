const char * thingDescriptionTemplate = R"(
{
    "@context": "https://www.w3.org/2019/wot/td/v1",
    "id": "urn:dev:ops:32473-WoTPlant-1234",
    "title": "MyPlantThing1",
    "securityDefinitions": {
        "nosec_sc": {
            "scheme": "nosec"
        }
    },
    "security": [
        "nosec_sc"
    ],
    "properties": {
        "status": {
            "type": "string",
            "readOnly" : false,
            "forms": [
                {   "op": "readproperty",
                    "href": "http://&IP_ADDR&/properties/status",
                    "htv:methodName": "GET",
                    "contentType": "application/json"
                },
                {
                    "op": "writeproperty",
                    "href": "http://&IP_ADDR&/properties/status",
                    "htv:methodName": "PUT",
                    "contentType": "application/json"
                }
            ]
        },
        "humidity": {
            "type": "number",
            "unit": "%",
            "readOnly" : true,
            "forms": [
                {
                    "href": "http://&IP_ADDR&/properties/humidity"
                }
            ]
        },
        "temperature": {
            "type": "number",
            "unit": "°C",
            "readOnly" : true,
            "forms": [
                {
                    "href": "http://&IP_ADDR&/properties/temperature"
                }
            ]
        },
        "light-intensity": {
            "type": "number",
            "unit": "Lux",
            "readOnly" : true,
            "forms": [
                {
                    "href": "http://&IP_ADDR&/properties/light-intensity"
                }
            ]
        },
        "moisture": {
            "type": "number",
            "unit": "%",
            "readOnly" : true,
            "forms": [
                {
                    "href": "http://&IP_ADDR&/properties/moisture"
                }
            ]
        },
        "humidityMin": {
            "type": "number",
            "unit": "%",
            "minimum": 0,
            "maximum": 100,
            "readOnly" : false,
            "forms": [
                {
                    "href": "http://&IP_ADDR&/properties/humidityMin"
                }
            ]
        },
        "humidityMax": {
            "type": "number",
            "unit": "%",
            "minimum": 0,
            "maximum": 100,
            "readOnly" : false,
            "forms": [
                {
                    "href": "http://&IP_ADDR&/properties/humidityMax"
                }
            ]
        },
        "tempMin": {
            "type": "number",
            "unit": "°C",
            "minimum": -10,
            "maximum": 100,
            "readOnly" : false,
            "forms": [
                {
                    "href": "http://&IP_ADDR&/properties/tempMin"
                }
            ]
        },
        "tempMax": {
            "type": "number",
            "unit": "°C",
            "minimum": -10,
            "maximum": 100,
            "readOnly" : false,
            "forms": [
                {
                    "href": "http://&IP_ADDR&/properties/tempMax"
                }
            ]
        },
        "lightMin": {
            "type": "number",
            "unit": "Lux",
            "minimum": 0,
            "maximum": 10000,
            "readOnly" : false,
            "forms": [
                {
                    "href": "http://&IP_ADDR&/properties/lightMin"
                }
            ]
        },
        "lightMax": {
            "type": "number",
            "unit": "Lux",
            "minimum": 0,
            "maximum": 10000,
            "readOnly" : false,
            "forms": [
                {
                    "href": "http://&IP_ADDR&/properties/lightMax"
                }
            ]
        },
        "moistureMin": {
            "type": "number",
            "unit": "%",
            "minimum": 0,
            "maximum": 10000,
            "readOnly" : false,
            "forms": [
                {
                    "href": "http://&IP_ADDR&/properties/moistureMin"
                }
            ]
        },
        "moistureMax": {
            "type": "number",
            "unit": "%",
            "minimum": 0,
            "maximum": 10000,
            "readOnly" : false,
            "forms": [
                {
                    "href": "http://&IP_ADDR&/properties/moistureMax"
                }
            ]
        }
    },
    "actions": {
        "toggle-led": {
            "forms": [
                {
                    "href": "http://&IP_ADDR&/actions/toggle-led"
                }
            ]
        },
        "reset-event": {
            "forms": [
                {
                    "href": "http://&IP_ADDR&/actions/reset-event"
                }
            ]
        },
        "set-rgb-led": {
            "forms": [
                {
                    "href": "http://&IP_ADDR&/actions/set-rgb-led{?red,green,blue}"
                }
            ],
            "uriVariables": {
                "red": {
                    "type": "integer",
                    "minimum": 0,
                    "maximum": 255
                },
                "green": {
                    "type": "integer",
                    "minimum": 0,
                    "maximum": 255
                },
                "blue": {
                    "type": "integer",
                    "minimum": 0,
                    "maximum": 255
                }
            }
        }
    },
    "events": {
        "critical-plant-status": {
            "description" : "The plant is in danger! Please check the Sensor Values in the Web App and help your plant.",
            "data": {
                "type": "string"
            },
            "forms": [
                {
                "op": "subscribeevent",
                "href": "http://&IP_ADDR&/events/critical-plant-status",
                "contentType": "text/event-stream",
                "subprotocol": "sse"
                },
                {
                "op": "unsubscribeevent",
                "href": "http://&IP_ADDR&/events/critical-plant-status",
                "contentType": "text/event-stream",
                "subprotocol": "sse"
                }
            ]
        }
    }
}
)";