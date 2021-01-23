const char * thingDescriptionTemplate = R"(
{
    "@context": "https://www.w3.org/2019/wot/td/v1",
    "id": "urn:dev:ops:32473-WoTPlant-1234",
    "title": "RGBThing1",
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
        }
    },
    "actions": {
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
    }
}
)";