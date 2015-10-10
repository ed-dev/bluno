//
// Copyright 2014, Andreas Lundquist
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// DFRobot - Bluno - Hello World
//                                               version: 0.1 - 2014-11-21
//

// Route all console logs to Evothings studio log
if (window.hyper) { console.log = hyper.log; };

document.addEventListener('deviceready',function(){ app.initialize(); }, false);

var app = {};

//app.DFRBLU_SERVICE_UUID = 'dfb0';
app.DFRBLU_SERVICE_UUID = '0000dfb0-0000-1000-8000-00805f9b34fb';
app.DFRBLU_CHAR_RXTX_UUID = '0000dfb1-0000-1000-8000-00805f9b34fb';
app.DFRBLU_TX_UUID_DESCRIPTOR = '00002902-0000-1000-8000-00805f9b34fb';

app.initialize = function() {

	app.connected = false;
    app.disconnect();
};


// val = ((long )b[0]) << 24;
// val |= ((long )b[1]) << 16;
// val |= ((long )b[2]) << 8;
// val |= b[3];


app.startScan = function() {

	app.disconnect();

	console.log('Scanning started...');

	app.devices = {};

	var htmlString = '<img src="img/loader_small.gif" style="display:inline; vertical-align:middle">' +
					'<p style="display:inline">   Scanning...</p>';

       
	$( "#scanResultView" ).append( $( htmlString ) );

	$( "#scanResultView" ).show();
	function onScanSuccess(device) {

		if(device.name != null) {

			app.devices[device.address] = device; 

			console.log('Found: ' + device.name + ', ' + device.address + ', ' + device.rssi);

			var htmlString = '<div class="deviceContainer" onclick="app.connectTo(\'' + device.address + '\')">' +
								'<p class="deviceName">' + device.name + '</p>' +
								'<p class="deviceAddress">' + device.address + '</p>' +
							'</div>';

			$( "#scanResultView" ).append( $( htmlString ) );
		}
	};

	function onScanFailure(errorCode) {
		
		// Show an error message to the user
		app.disconnect('Failed to scan for devices.');

		// Write debug information to console.
		console.log('Error ' + errorCode);
	};

	evothings.easyble.reportDeviceOnce(true);
	evothings.easyble.startScan(onScanSuccess, onScanFailure); 

	$( "#startView" ).hide();
     
};


app.setLoadingLabel = function(message) {
	
	console.log(message);
	$( '#loadingStatus').text(message);
}

app.connectTo = function(address) {

	device = app.devices[address];

	$( "#loadingView" ).css('display', 'table');

	app.setLoadingLabel('Trying to connect to ' + device.name);

	function onConnectSuccess(device) {

		function onServiceSuccess(device) {

				// Application is now connected
				app.connected = true;
				app.device = device;

				console.log('Connected to ' + device.name);
		    
				$( "#loadingView" ).hide();
		                $( "#scanResultView" ).hide();
				$( "#controlView" ).show();
		                $( "#controlView2" ).hide();
				device.enableNotification(app.DFRBLU_CHAR_RXTX_UUID, app.receivedData, function(errorcode){console.log('BLE enableNotification error: ' + errorCode);});

			};

			function onServiceFailure(errorCode) {

				// Disconnect and show an error message to the user.
				app.disconnect('Device is not from DFRobot');

				// Write debug information to console.
				console.log('Error reading services: ' + errorCode);
			};

			app.setLoadingLabel('Identifying services...');

			// Connect to the appropriate BLE service
			device.readServices([app.DFRBLU_SERVICE_UUID], onServiceSuccess, onServiceFailure);
	};

	function onConnectFailure(errorCode) {

			// Disconnect and show an error message to the user.
			app.disconnect('Failed to connect to device');

			// Write debug information to console
			console.log('Error ' + errorCode);
	};

	// Stop scanning
	evothings.easyble.stopScan();

	// Connect to our device
	console.log('Identifying service for communication');
	device.connect(onConnectSuccess, onConnectFailure);
};

app.startScanTest = function() {
				$( "#loadingView" ).hide();
                                $( "#startView" ).hide();
		                $( "#scanResultView" ).hide();
				$( "#controlView" ).show();
                                $( "#controlView2" ).hide();

};
app.pageTwo = function() {
				$( "#loadingView" ).hide();
		                $( "#scanResultView" ).hide();
                                $( "#controlView" ).hide();
				$( "#controlView2" ).show();
                                $( "#startView" ).hide();

};
app.pageOne = function() {
				$( "#loadingView" ).hide();
		                $( "#scanResultView" ).hide();
                                $( "#controlView" ).show();
				$( "#controlView2" ).hide();
                                $( "#startView" ).hide();
};

app.sendData = function(dataStr) {

	if(app.connected) {

		function onMessageSendSucces() {
		
			console.log('Succeded to send message.');
		}

		function onMessageSendFailure(errorCode){

			console.log('Failed to send data with error: ' + errorCode);
 			app.disconnect('Failed to send data');
		};
                var  data = evothings.ble.toUtf8(dataStr);


		app.device.writeCharacteristic(app.DFRBLU_CHAR_RXTX_UUID, data, onMessageSendSucces, onMessageSendFailure);
	}
	else {

		// Disconnect and show an error message to the user.
		app.disconnect('Disconnected');

		// Write debug information to console
		console.log('Error - No device connected.');
	}
};

app.receivedData = function(data) {


    var data = new Uint8Array(data);
    
    if (data[0] === 0xAD) {
	
	console.log('Data received: [' + data[0] +', ' + data[1] +', ' + data[2] + ']');
	var value = 520;
	if(Math.random()<0.5)
	{value=512;}
//	var value = (data[2] << 8) | data[1];
//	alert(value);
	$( '#result').text(value);
//	$( '#result').attr('value',value);
//	alert($('#result').text())
	
    }
    //conductivity success/fail
    else if(data[0] === 0xAE ){
	alert("Calibration Completed");
//        $('#calResult').text('tick');

    }
    
    else if(data[0] === 0xAF ){
//	 $('#calFinish').text('tick');
	alert("Reset to factory default");
    }        
  else if(data[0] === 0xAC ){
//	 $('#calFinish').text('tick');
	alert("Sampling Rate changed");
    }    

    

   // try{
    //    var value = (data[2] << 8) | data[1];
     //   var  dataStr = evothings.ble.fromUtf8(data);
//	$( '#result').text(dataStr);
  //  }catch(err){	$( '#result').text(err);}
	
//	if(app.connected) {	
	   // alert("Data received");
	  //  var dataStr = evothings.ble.fromUtf(data);//new Uint8Array(data);
	  //  var ua =  new Uint8Array(data);
           // var dataStr = evothings.ble.fromUtf(d);

	  //  var dataStr = String.fromCharCode.apply(String,ua);
	    //var test = decodeURIComponent(escstr);
  //  var value;
//	if (data[0] === 0xAD) {
//	    value = (data[2] << 8) | data[1];
//	    $( '#result').text(value);
//	}
	  //  console.log(value);

	//	if (data[0] === 0xAD) {
	//		var value =  data[1];
//			$( '#result').text(value);
//		}
//	}
//	else {
//	     	$( '#result').text("not connected");   
	// Disconnect and show an error message to the user.
//	app.disconnect('Disconnected');

	// Write debug information to console
//	console.log('Error - No device connected.');
//	}

};

app.disconnect = function(errorMessage) {

	if(errorMessage) {

		navigator.notification.alert(errorMessage,function alertDismissed() {});
	}

	app.connected = false;
	app.device = null;

	// Stop any ongoing scan and close devices.
	evothings.easyble.stopScan();
	evothings.easyble.closeConnectedDevices();

	console.log('Disconnected');

	$( "#scanResultView" ).hide();
	$( "#scanResultView" ).empty();
	$( "#controlView" ).hide();
	$( "#controlView2" ).hide();
	$( "#startView" ).show();
      

};
