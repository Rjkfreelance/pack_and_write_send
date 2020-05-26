const mysql = require('mysql')
const mqtt = require('mqtt')
var conDB = mysql.createConnection({
  host: "localhost",
  user: "root",
  password: "rjk3145001",
  database: "mconnect_data"
});

conDB.connect(function(err) {
  if (err) throw err;
  console.log("DB Connected");
});


var server = "m16.cloudmqtt.com" //CloudMqtt Server
var Topic = "DB/talk"
var Tpreply = "DB/run"
var TopicData = "MMM/G00001/#" // All group1
var Tomconn = "RM/G00001/#" //for reply to mconnect board  
var message=""
var client = ""
var Pub = "1"
var topsend = ""
var  Data = ""
var options = {
port:19237,
clientId: 'DBserv_' + Math.random().toString(16).substr(2, 8), //random ClientID
username: "rgyrtmml",
password: "atLu2AKVWPa7",
clean:true
}

client  = mqtt.connect(`mqtt://${server}`, options)
  
client.on('connect', function() { // When connected
       console.log('MQTT Connected '+ client.connected)
       let Msg = "1" 
       client.subscribe(Topic, function(){
            client.on('message', function(topic, message, packet) {      
              if(topic == Topic){ 
                  Msg = message.toString();
                  console.log(message)
                  console.log(Msg)
                   client.publish(Tpreply,Pub,function() {
                      console.log("Message is published "+ Pub);
                   })
                }
             })
           
        })
    client.subscribe(TopicData, function(){
       client.on('message', function(topic, message, packet) { 
         if(topic.match(/MMM/)){
         topsend = topic
         Data = message.toString()
        }
            
            console.log(packet)
            console.log(message)
            console.log(Data)

let date_now = new Date();
 
let date = ("0" + date_now.getDate()).slice(-2);

let month = ("0" + (date_now.getMonth() + 1)).slice(-2);

let year = date_now.getFullYear();

let hours = date_now.getHours();

let minutes = date_now.getMinutes();

let seconds = date_now.getSeconds();

var dmyhms = date+"/"+month+"/"+year+" "+hours+":"+minutes+":"+seconds

console.log(dmyhms)
if(topic.match(/MMM/)){
  var sql = "INSERT INTO rawdata (topic,data,datetime) VALUES ('"+topsend+"','"+Data+"','"+dmyhms+"')";
  conDB.query(sql, function (err, result) {
    if (err) throw err;
    console.log("Insert Data ok");
      
      client.publish(Tomconn,'OK',function() {
         console.log("Reply to Mconnect: OK");
       })

  });
 } 
            
})

})
     
             
})  // on connect
