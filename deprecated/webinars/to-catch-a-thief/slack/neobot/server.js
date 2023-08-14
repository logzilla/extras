// Import express and request modules
var express = require("express");
var request = require("request");
var router = express.Router();
var bodyParser = require("body-parser")
var urlencodedParser = bodyParser.urlencoded({ extended: false })
var clientId = "XXXXXXXX65.XXXXXXX3636";
var clientSecret = "0XXXXXXXX5fd67XXXXXXXX03968c0283";
var YOUR_APP_VERIFICATION_TOKEN = "LXNXXXXXXXXXXXXXXXX7G6K9";
var app = express();
const PORT=4390;
app.listen(PORT, function () {
    console.log("NEOBot is listening on port " + PORT);
});
app.get("/", function(req, res) {
    res.send("Ngrok is working! Path Hit: " + req.url);
});
app.get("/oauth", function(req, res) {
    if (!req.query.code) {
        res.status(500);
        res.send({"Error": "Looks like we're not getting code."});
        console.log("Looks like we're not getting code.");
    } else {
        request({
            url: "https://slack.com/api/oauth.access",
            qs: {code: req.query.code, client_id: clientId, client_secret: clientSecret},
            method: "GET",

        }, function (error, response, body) {
            if (error) {
                console.log(error);
            } else {
                res.json(body);

            }
        })
    }
});

app.post("/status", function(req, res) {
    res.send("Your ngrok tunnel is up and running!");
});

function sendMessageToSlackResponseURL(responseURL, JSONmessage){
  var postOptions = {
    uri: responseURL,
    method: "POST",
    headers: {
      "Content-type": "application/json"
    },
    json: JSONmessage
  }
  request(postOptions, (error, response, body) => {
    if (error){
      // add error handling...
    }
  })
}

app.post("/slack/actions", urlencodedParser, (req, res) =>{
  res.status(200).end() 
  var actionJSONPayload = JSON.parse(req.body.payload)
  var deviceStatus = actionJSONPayload.original_message.attachments[0].fields[1].value;
  var caseID = actionJSONPayload.original_message.attachments[0].fields[2].value;
  var detectiveName = actionJSONPayload.original_message.attachments[0].fields[3].value;
  var detectiveMobile = actionJSONPayload.original_message.attachments[0].fields[5].value;
  var detectiveEmail = actionJSONPayload.original_message.attachments[0].fields[6].value;
  var deviceLocation = actionJSONPayload.original_message.attachments[0].fields[7].value;
  var message = {
    "text": actionJSONPayload.user.name+" confirmed notification to " + detectiveName + "<" +detectiveEmail+ "> that a " +deviceStatus+ " device has been located at " +deviceLocation,
    "replace_original": false
  }
  sendMessageToSlackResponseURL(actionJSONPayload.response_url, message)
  var email = detectiveEmail.replace(/.+:([^\|]+).*/g, '$1');

  var post_data = {
    "body": "Hi " +detectiveName+ ",\nWe've located a " +deviceStatus+ " device at " +deviceLocation+ ".\nPlease contact us at 555-3421 if you need assistance.\nScramble the jets!",
    "is_html": false,
    "mail_to": email,
    "subject" : "URGENT: Case # " +caseID+ ", " +deviceStatus+ " device"
  };
  sendToNeo("/api/mailer", post_data);
})

function sendToNeo(url, JSONmessage){
  var postOptions = {
    uri: "http://localhost" + url,
    method: "POST",
    headers: {
      "Content-type": "application/json",
      "Authorization": "token XXXXXXXX22a6e121707XXXXXXXX1fe3c7XXXXXXXXf107900"
    },
    json: JSONmessage
  }
  request(postOptions, (error, response, body) => {
    if (error){
      // add error handling
    }
    console.log("Status: " + response.statusCode);
    console.log("Headers: " + JSON.stringify(response.headers));
    response.setEncoding("utf8");
    response.on("data", function (body) {
      console.log("Body: " + body);
    });
  })
}
