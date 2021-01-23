require('dotenv').config();

// include dependencies
const express = require('express');
const { createProxyMiddleware } = require('http-proxy-middleware');

const jwt = require('jsonwebtoken');

const fs = require('fs');
const https = require('https');

var privateKey  = fs.readFileSync(process.env.PRIVATE_SSL_KEY_PATH);
var certificate = fs.readFileSync(process.env.CERTIFICATE_PATH);

var credentials = {key: privateKey, cert: certificate};

const auth = {login: process.env.BASIC_AUTH_LOGIN, password: process.env.BASIC_AUTH_PASSWORD}
const accessTokenSecret = process.env.ACCESS_TOKEN_SECRET


const dns_uri = "https://plantfriend.ddns.net"
const local_uri_plant1 = 'http://192.168.178.61'
var localThings = [{title: "MyPlantThing1", localIP: "http://192.168.178.61", lastActive: new Date()}]

var proxyTable = {
  '/gateway/MyPlantThing1': local_uri_plant1,
};


//returns -1 if not found n times
function nthIndex(str, pat, n){
  var L= str.length, i= -1;
  while(n-- && i++<L){
    i= str.indexOf(pat, i);
    if (i < 0) break;
  }
  return i;
}

var pathRewriteFunction = function(path,req) {

  //split Path at 3rd occurence of /
  var indexToSplice = nthIndex(path, '/', 3)

  if (indexToSplice == -1){
    return ''
  }

  var newPath = path.slice(indexToSplice)

  console.log("new path: ", newPath)
  if (newPath) {
    return newPath
  }
  return '';
};

// proxy middleware options
const optionsTDProxy = {
  target: local_uri_plant1,
  changeOrigin: true, // needed for virtual hosted sites
  ws: false, // proxy websockets
  pathRewrite: pathRewriteFunction,
  router: proxyTable,
  logLevel: 'debug',

  onProxyRes: function(proxyRes, req, res) {

    var body = new Buffer.from('');
    proxyRes.on('data', function(data) {
      body = Buffer.concat([body, data]);
    });
    proxyRes.on('end', function() {
      try{
        body = body.toString();
        //console.log("response from proxied server:", body);

        var found = false;
        var found_thing;

        for (let thing of localThings){

          if (body.includes(thing.localIP)) {
            found = true;
            found_thing = thing;
          }
        }
        if (found) {
          console.log("---- Proxy TD for: ", found_thing)

          var re = new RegExp(found_thing.localIP, "g");
          body = body.replace(re, dns_uri + "/gateway/" + found_thing.title);
          console.log("new body: ", body)

          json_body = JSON.parse(body);
          json_body["properties"]["logged-values"] = {type: "text", unit: "unknown", readOnly: true, forms: [{
              href: "https://plantfriend.ddns.net/gateway/" + found_thing.title +"/properties/logged-values"}]};

          json_body["securityDefinitions"] = {
            "bearer_sc": {
              "in": "header",
              "scheme": "bearer",
              "format": "jwt",
              "alg": "HS256"
            }
          }

          json_body["security"] = "bearer_sc";

          body = JSON.stringify(json_body);

          res.end(body);
          return
        }
        else {
          console.log("No Thing description -> proxy without modification")
          res.end(body)
        }
      }
      catch (e){
        console.log("error parsing json: ", e)
        res.end(body);
      }
    });
  },

  selfHandleResponse: true
}

const optionsGeneralProxy = {
  target: local_uri_plant1,
  changeOrigin: true, // needed for virtual hosted sites
  ws: false, // proxy websockets
  pathRewrite: pathRewriteFunction,
  router: proxyTable,
  logLevel: 'debug'
}

const optionsAppProxy = {
  target: process.env.APP_TARGET_ADRESS,
  changeOrigin: true, // needed for virtual hosted sites
  ws: false, // proxy websockets
  pathRewrite: {
  },
  router: {
    '/': process.env.APP_TARGET_ADRESS
  },
  logLevel: 'debug',
  secure: false
}



// create the proxyies
const thingDescriptionProxy = createProxyMiddleware(optionsTDProxy);

const generalProxy = createProxyMiddleware(optionsGeneralProxy);

const appProxy = createProxyMiddleware(optionsAppProxy);

// mount `exampleProxy` in web server
const app = express();


app.all('/*', (req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*')
  res.header("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS")
  res.header("Access-Control-Allow-Headers", "Content-Type, Access-Control-Allow-Headers, Authorization, X-Authorization,  X-Requested-With, Cache-Control, last-event-id")
  res.header('Content-Type', 'application/json')

  console.log("---- New Request: " + JSON.stringify(req.body));

  let s;
  try {s = req.body}
  catch (e){
    s = "catch"
  };
  console.log(s);

  //for CORS pre-flight: OPTIONS must return 200 OK
  if (req.method === "OPTIONS") {
    return res.status(200).end();
  }

  next()
})


const authenticateJWT = (req, res, next) => {
  if (String(req.originalUrl).includes("/events")) {
    next()
    return
  }

  const authHeader = req.header("Authorization");

  if (authHeader) {
    const token = authHeader.split(' ')[1];

    jwt.verify(token, accessTokenSecret, (err, user) => {
      if (err) {

        console.log("original url: ", req.originalUrl);

        console.log(err, "--- Wrong Token: ", token);
        return res.sendStatus(403);
      }

      req.user = user;
      next();
    });
  } else {
    res.sendStatus(401);
  }
};

app.all('/gateway/*', authenticateJWT);

app.post('/login', express.json());
app.post('/login', function (req, res, next) {
  // -----------------------------------------------------------------------
  // authentication middleware

  //no ":" in pwd supported
  const [login, password] = [req.body.username, req.body.password];

  // Verify login and password are set and correct
  if (login && password && login === auth.login && password === auth.password) {
    // Access granted...
    // Generate an access token
    const accessToken = jwt.sign({ username: login}, accessTokenSecret);

    res.json({
      accessToken
    });

    return next()
  }

  // Access denied...
  res.status(401).send('Authentication required.') // custom message
})

app.post('/gateway/add-thing', express.json());
app.post('/gateway/add-thing', function (req, res, next) {
  // -----------------------------------------------------------------------
  // Add new Thing to Gateway


  const [title, localIP] = [req.body.title, req.body.localIP];

  // duplicate, already existing in list
  for (let thing of localThings){
    if (thing.title == title){
      console.log("update from existing thing");

      proxyTable["/gateway/" + thing.title] = localIP
      thing.lastActive = new Date();

      console.log("Updated local IP and lastActive... New proxy Table: ")
      console.log(proxyTable);
      res.status(201).send("Successfully created: " +  title)
      return
    }
  }


  localThings.push({title: title, localIP: localIP, lastActive: new Date()});

  console.log("localThings now: ", JSON.stringify(localThings));

  proxyTable["/gateway/" + title] = localIP

  console.log("Added new Thing/IP Pair -> New proxy Table: ")
  console.log(proxyTable);

  res.status(201).send("Successfully created: " +  title)
})

app.get('/gateway/things', function (req, res, next){
  things = []

  for (let thing of localThings){
    things.push(thing.title)
  }
  res.send(things)
})

app.all('/gateway/:thingTitle/properties/logged-values', function (req, res, next) {
  res.send('placeholder data')
  next()
})


//Proxies are not calling next(), so that the last middleware for App related stuff (/*) is not executed
app.all('/gateway/:thingTitle', function (req, res, next) {
  console.log("TD Proxy - thingTitle: ", req.params.thingTitle)
  next()
}, thingDescriptionProxy)

app.all('/gateway/:thingTitle/*', function (req, res, next) {
  console.log("General Thing Proxy - thingTitle: ", req.params.thingTitle)
  next()
}, generalProxy)


//every other path/route should proxy to web app
app.all('/*', appProxy);

var httpsServer = https.createServer(credentials, app);

httpsServer.listen(process.env.GATEWAY_PORT);



// filter inactive things (no message from them in last 15 seconds)

function filterInactiveThings(thing) {
  var timeNow = new Date();

  if ((timeNow - thing.lastActive) < 15000){
    return true
  }
  else {
    delete proxyTable["/gateway/" + thing.title]
    return false
  }

}

function removeInactiveThings() {
  console.log("INACTIVE THING INTERVAL! ----------------------", JSON.stringify(localThings));

  localThings = localThings.filter(filterInactiveThings);

  console.log("cleared Inactive Things -> new localThings: ", JSON.stringify(localThings));
}

var inactiveThingInterval = setInterval(removeInactiveThings, 15000);
