<template>
  <v-container>
    <v-container v-if="!userIsLoggedIn">
      <v-text-field v-model="loginData.userName" type="text" placeholder="Username"></v-text-field>
      <v-text-field v-model="loginData.password" type="password" placeholder="Password" v-on:keyup.enter="login()"></v-text-field>
      <v-btn @click="login()">Login</v-btn>
    </v-container>

    <v-container v-if="userIsLoggedIn">
      <v-col class="text-right">
        <v-btn v-on:click="updateThingsListFromGatewayAndUpdate()">Update Things List
          <v-icon> mdi-refresh </v-icon>
        </v-btn>
        <v-btn v-on:click="logout()">Logout
          <v-icon> mdi-logout </v-icon>
        </v-btn>
      </v-col>


      <v-img
          :src="require('../assets/plant-svgrepo-com.svg')"
          class="my-3"
          contain
          height="200"
      />

      <v-row class="text-center">

            <v-col class="mb-4" v-for="(UI_thing, idx) in UI_things" :key="idx">
              <h1 class="display-2 font-weight-bold mb-3">
                {{ UI_thing.thing_title }}
              </h1>

              <v-col
                  justify="center"
              >

                <v-btn class="ma-3" v-on:click="updateThing()">Update Thing</v-btn>

                <v-btn class="ma-3" v-on:click="notifyMe('Test!')">Notify me! (Test)</v-btn>

                <v-col>
                  <h2 class="headline font-weight-bold mb-3">
                    Sensor Values and Configuration
                  </h2>
                  <v-simple-table>
                    <template v-slot:default>
                      <thead>
                      <tr>
                        <th class="text-left">
                          Name
                        </th>
                        <th class="text-left">
                          Value
                        </th>
                        <th class="text-left">
                          Unit
                        </th>
                        <th class="text-left">
                          Read Only
                        </th>
                      </tr>
                      </thead>
                      <tbody>
                      <tr
                          v-for="item in UI_thing.properties"
                          :key="item.name">
                        <td class="text-left">{{ item.name }}</td>

                        <td>

                          <div v-if=item.readOnly>
                        <td v-if=item.readOnly class="text-left">{{ item.value }}</td>
                        </div>

                        <div v-else>
                          <td v-if="item.type==='number' || item.type==='integer'">
                            <v-text-field  v-model="item.value" type="number" :min="item.minimum"
                                           :max="item.maximum" append-icon="mdi-content-save"
                                           @click:append="item.updateWritableProperty(item.name, item.value)"></v-text-field>
                          </td>
                          <td v-else>
                            <v-text-field v-model="item.value" type="text" append-icon="mdi-content-save"
                                          @click:append="item.updateWritableProperty(item.name, item.value)"></v-text-field>
                          </td>
                        </div>
                        </td>

                        <td class="v-text-field">{{ item.unit }}</td>
                        <td class="text-left">{{ item.readOnly }}</td>
                      </tr>
                      </tbody>
                    </template>
                  </v-simple-table>
                </v-col>


                <h2 class="headline font-weight-bold mb-3">
                  Events
                </h2>

                <v-col
                    v-for="(event, event_key) in UI_thing.events_functions" :key="event_key"
                >
                  <v-btn class="mb-5" v-on:click="event.subscribeEvent(event_key)">Subscribe Event: {{event_key}}</v-btn>
                  <v-btn class="mb-5" v-on:click="event.unsubscribeEvent(event_key)">Unsubscribe Event: {{event_key}}</v-btn>
                </v-col>
              </v-col>


              <h2 class="headline font-weight-bold mb-3">
                Actions
              </h2>

              <v-col v-for="(action, action_key) in UI_thing.actions_functions" :key="action_key">
                <v-col v-for="(uriVariable, uriVariable_key) in action.uriVariables" :key="uriVariable_key">
                  <v-text-field :label="action_key +' with Param: '+ uriVariable_key + ' (Type:' +uriVariable.info.type +')'"
                                :value="UI_thing.actions_functions[action_key].uriVariables[uriVariable_key].value"
                                @input="updateUriVariableValues(action_key, uriVariable_key, $event, idx )"></v-text-field>
                  <p>Param value: {{ UI_thing.actions_functions[action_key].uriVariables[uriVariable_key].value }}</p>
                </v-col>
                <v-btn v-on:click="action.button_function(action_key)">{{ action_key }}</v-btn>
              </v-col>

          </v-col>
      </v-row>
    </v-container>
  </v-container>
</template>

<script>

import axios from 'axios';

// Required steps to create a servient
const Servient = require('@node-wot/core').Servient
const HttpClientFactory = require('@node-wot/binding-http').HttpClientFactory
const HttpsClientFactory = require('@node-wot/binding-http').HttpsClientFactory
const Helpers = require('@node-wot/core').Helpers

export default {
  async mounted() {
    document.title = "Plant Friend"
  },
  beforeDestroy() {
    clearInterval(this.timer30Secs)
  },

  name: 'HelloWorld',

  metaInfo: {
    // if no subcomponents specify a metaInfo.title, this title will be used
    title: 'Plant Friend',
    // all titles will be injected into this template
    titleTemplate: '%s | PlantFriend'
  },

  data: () => ({
    loginData: {
      userName: '',
      password: ''
    },
    user: {accessToken: ''},

    UI_things: [],

    thingURI: 'https://plantfriend.ddns.net/gateway/plant-thing1',
    thingTitle: "My Thing",
    thingURLs: [],
    actions_functions: [],
    events_functions: [],
    thing_titles : [],

    subscribedEvents: [],

    mySensorData: [
      {
        name: "temperature",
        value: 24,
        unit: "degree celsius",
        readOnly: true,
        type: "number"
      }
    ]
  }),

  computed: {
    userIsLoggedIn : function () {
      return !!this.user.accessToken
    }
  },


  methods: {

    login: async function (){

      await axios
          .post('https://plantfriend.ddns.net/login', {"username": this.loginData.userName, "password": this.loginData.password})
          .then(async response => {
            if (response.data.accessToken) {
              this.user.accessToken = response.data.accessToken;

              this.servient = new Servient()
              this.servient.addCredentials({"urn:dev:ops:32473-WoTPlant-1234": {
                  token: this.user.accessToken
                }})


              console.log("Received Access Token");
              this.servient.addClientFactory(new HttpsClientFactory(null))
              this.servient.addClientFactory(new HttpClientFactory(null))

              this.wotHelper = new Helpers(this.servient)
              this.WoT = await this.servient.start()

              this.updateThingsListFromGatewayAndUpdate();

              this.timer30Seconds = setInterval(this.updateThingsListFromGatewayAndUpdate, 30000);

              this.loginData = {userName: '', password: ''};

            }
            else{
              confirm("Login failed.")
            }

            return response.data;
          }
          )
          .catch( () => {confirm("Login failed.")});
    },

    updateThingsListFromGatewayAndUpdate: async function() {
      let _this = this;
      await axios.get(
          "https://plantfriend.ddns.net/gateway/things",
          {
            headers: {
              "Authorization": ("Bearer " + _this.user.accessToken)}})
          .then(async (response) => {
            // using await for serial execution (note 'async' in then() of fetch())
            try {
              console.log(response)

              _this.thingURLs = [];
              for (const [index, thingTitle] of response.data.entries()) {
                console.log(thingTitle, index)
                _this.thingURLs[index] = "https://plantfriend.ddns.net/gateway/" + thingTitle
              }
              console.log("Thing URLS array: ", _this.thingURLs)
            } catch (err) {
              console.error("Script error with WoT Consume:", err);
            }
          }).catch((err) => {
            console.error("Fetch error:", err);
          });

          this.updateThing();
    },

    logout: function() {
      this.user.accessToken = null;

      clearInterval(this.timer30Seconds);

      this.servient.shutdown();
      this.servient = null;

      for (let event of this.subscribedEvents){
        this.thing.unsubscribeEvent(event)
      }
      this.thing = null;
    },

    notifyMe: function(str) {
      // Let's check if the browser supports notifications
      if (!("Notification" in window)) {
        alert("This browser does not support desktop notification");
      }

      // Let's check whether notification permissions have already been granted
      else if (Notification.permission === "granted") {
        // If it's okay let's create a notification
        new Notification(str);
      }

      // Otherwise, we need to ask the user for permission
      else if (Notification.permission !== "denied") {
        Notification.requestPermission().then(function (permission) {
          // If the user accepts, let's create a notification
          if (permission === "granted") {
            new Notification(str);
          }
        });
      }
    },

    updateThing: async  function(){

      this.mySensorData = [];
      this.actions_functions = [];
      this.events_functions = [];
      this.thing_titles = [];

      this.UI_things.length = this.thingURLs.length;

      for (let [index, thingTitle] of this.thingURLs.entries()) {
        //console.log("index: ",  index)
        //console.log("value: ",  thingTitle)

        await axios.get(
            this.thingURLs[index],
            {headers: {
                "Authorization" : ("Bearer " + this.user.accessToken)
              }
            }
        )
            .then(async (td) => {
              // using await for serial execution (note 'async' in then() of fetch())
              try {
                let thing = await this.WoT.consume(td.data);


                await this.updateThingProperties(thing, index);
                await this.updateThingActions(thing, index);
                await this.updateThingEvents(thing, index);

                this.UI_things.splice(index, 1,
                    { id: this.thing_titles[index],
                      thing_title: this.thing_titles[index],
                      properties: this.mySensorData[index],
                      actions_functions: this.actions_functions[index],
                      events_functions: this.events_functions[index]
                    });

                console.log("UI_THINGS:  ", this.UI_things);

              } catch (err) {
                console.error("Script error with WoT Consume:", err);
                console.log("error thing title", thingTitle)
              }
            }).catch((err) => {
              console.error("Fetch error:", err);
            });


      }


    },


    updateThingProperties: async function (thing, index) {

      let thing_description = await thing.getThingDescription();
      let all_properties = await thing.readAllProperties();

      this.thing_titles[index] =  thing_description["title"]



      this.mySensorData[index] = [];
      for (var key in all_properties) {
        this.mySensorData[index].push({
          name: key,
          value: all_properties[key],
          unit: thing_description["properties"][key]["unit"],
          readOnly: thing_description["properties"][key]["readOnly"],
          type: thing_description["properties"][key]["type"],
          minimum: thing_description["properties"][key]["minimum"],
          maximum: thing_description["properties"][key]["maximum"],
          updateWritableProperty: async (propertyName, value) => {
            console.log("update property " +propertyName +" value to " + value)
            await thing.writeProperty(propertyName, {[propertyName]: value}); }
        });
      }

    },


    updateThingActions: async function (thing, index) {

      let thing_description = await thing.getThingDescription();


      this.actions_functions[index] = {}

      for (var action in thing_description["actions"]) {
        this.actions_functions[index][action] = (
            {
              button_function: async (action) => {
                // see: https://forum.vuejs.org/t/is-not-a-function/12444 -> => or self
                let uriVariableValues = this.getUriVariablesValues(action, index);

                await thing.invokeAction(action, undefined, {uriVariables: uriVariableValues});
                console.log("TRIGGERED ACTION: ", action)
              }, uriVariables: this.getUriVariables(thing_description, action, index)
            })
      }
    },


    updateThingEvents: async function (thing, index) {

      let thing_description = await thing.getThingDescription();


      this.events_functions[index] = {};


      for (var event in thing_description["events"]) {

        this.events_functions[index][event] = (
            {subscribeEvent:
                  async (event) => {
                    thing.subscribeEvent(event, data => {
                      data = JSON.parse(data);
                      console.log("critical-plant-status event: ", data);
                      this.notifyMe(data.data);
                    }, )
                        .then(() => {
                          console.info("Subscribed to Event: ", event);
                          this.subscribedEvents.push(event);
                        })
                        .catch((e) => {
                          console.log("onError: %s", e);
                        });
                    } ,
              unsubscribeEvent: async (event) => {
                thing.unsubscribeEvent(event);
                console.info("Unsubscribed from Event: ", event);
              }
            }
        )
      }
    },

    getUriVariables: function (thing_description, action) {
      var uriVariables = {}
      for (var uriVariable in thing_description["actions"][action]["uriVariables"]) {
        uriVariables[uriVariable] = ({
          info: thing_description["actions"][action]["uriVariables"][uriVariable],
          value: null
        })
      }
      return uriVariables
    },

    getUriVariablesValues: function (action, index) {
      var filledUriVariables = {}
      for (var uriVariable in this.actions_functions[index][action].uriVariables) {
        filledUriVariables[uriVariable] = this.actions_functions[index][action].uriVariables[uriVariable].value
      }
      return filledUriVariables

    },
    updateUriVariableValues: function (action_key, uriVariable_key, target, index) {
      this.actions_functions[index][action_key].uriVariables[uriVariable_key].value = target
      this.$forceUpdate();
    }
  }
}
</script>
