/*
  Node.js application running on Heroku that connects to Salesforce.com
  using the nForce package to query support information for a customer based
  on an RFID custom field for an account.  The application then uses Socket.IO
  to update any clients browing a dashboard at http://localhost:3001

  Resources used
  nForce package - https://github.com/kevinohara80/nforce nforce is a node.js salesforce REST API wrapper for force.com, database.com, and salesforce.com.  Thanks to https://twitter.com/kevino80
  Express web application framework - http://expressjs.com/
  Socket.IO web socket library - http://socket.io/

  more details at www.johnbrunswick.com
*/
var express=require('express');
var nforce = require('nforce');

var app = express()
  , http = require('http')
  , server = http.createServer(app)
  , io = require('socket.io').listen(server);

// the env.PORT values are stored in the .env file, as well as authentication information needed for nForce
var port = process.env.PORT || 3001; // use port 3001 if localhost (e.g. http://localhost:3001)
var oauth;

// configuration
app.use(express.bodyParser());
app.use(express.methodOverride());
app.use(express.static(__dirname + '/public'));  

server.listen(port);

// dashboard.html will be sent to requesting user at root - http://localhost:3001
app.get('/', function (req, res) {
  res.sendfile(__dirname + '/views/dashboard.html');
});

// use the nforce package to create a connection to salesforce.com
var org = nforce.createConnection({
  clientId: process.env.CLIENT_ID,
  clientSecret: process.env.CLIENT_SECRET,
  redirectUri: 'http://localhost:' + port + '/oauth/_callback',
  apiVersion: 'v24.0',  // optional, defaults to v24.0
});

// authenticate using username-password oauth flow
org.authenticate({ username: process.env.USERNAME, password: process.env.PASSWORD }, function(err, resp){
  if(err) {
    console.log('Error: ' + err.message);
  } else {
    console.log('Access Token: ' + resp.access_token);
    oauth = resp;
  }
});

app.put('/card', function(req, res) {
  console.log('PUT CARD received');
  console.log(req);

  // Show incoming card ID from PUT
  console.log(req.body.carddata.cardid);

  var scannedid = req.body.carddata.cardid;
  //var scannedid = '23ad7b';

  console.log(scannedid);

  // SOQL query to get Account and related Cases based on custom field of RFID_Code__c
  org.query("SELECT Account.id, Account.name, (SELECT Case.Id, Case.CaseNumber, Case.IsClosed, Case.LastModifiedDate, Case.Subject, Case.Reason, Case.Description FROM Account.Cases ORDER BY Case.LastModifiedDate DESC) FROM Account WHERE Account.RFID_Code__c LIKE '"+scannedid+"'", oauth, function(err, resp){

    console.log(resp.records[0].Cases);

    // send all response data to client to view all attributes of object within the a browser console
    io.sockets.emit("servicedata", resp.records[0]);

    res.send({});
  });
});

// get feed for a given case to see latest updates noted by salesforce internal users
app.get('/casefeed/:id', function(req, res) {

  org.query("SELECT CaseFeed.Body, CaseFeed.ContentDescription, CaseFeed.CreatedDate, CaseFeed.ParentId FROM CaseFeed WHERE CaseFeed.ParentId = '"+req.params.id+"'", oauth, function(err, resp){

    console.log(resp);

    // send results as JSON to the client applicaiton at http://localhost:3001 (e.g. dashboard.html)
    res.writeHead(200, { 'Content-Type': 'application/json', "Access-Control-Allow-Origin":"*" });
    res.write(JSON.stringify(resp, 0, 4));
    res.end();
  });
});