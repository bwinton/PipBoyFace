'use strict';

/* eslint-env browser */
/* global Pebble:false, localStorage:false */
/*eslint no-console:0 */

var savedOptions = {};

var sendOptions = function (options) {
  Pebble.sendAppMessage(
    {'FACE_MODE': options.faceMode,
     'SHOW_GIFS': options.showGifs},
    function() {
      console.log('Sent settings data...');
    },
    function() {
      console.log('Settings feedback failed!');
    }
  );
};

var saveOptions = function (options) {
  localStorage.setItem('faceMode', options.faceMode);
  localStorage.setItem('showGifs', options.showGifs);
};

Pebble.addEventListener('ready',
  function() {
    savedOptions = {
      'faceMode': localStorage.getItem('faceMode') || 1,
      'showGifs': localStorage.getItem('showGifs') || 0
    };
    sendOptions(savedOptions);
  }
);

Pebble.addEventListener('showConfiguration',
  function() {
    //Load the remote config page
    Pebble.openURL('http://bwinton.github.io/PipBoyFace/?options=' + encodeURIComponent(JSON.stringify(savedOptions)));
  }
);

Pebble.addEventListener('webviewclosed',
  function(e) {
    savedOptions = JSON.parse(decodeURIComponent(e.response));
    console.log('Configuration window returned: ' + JSON.stringify(savedOptions));

    saveOptions(savedOptions);
    sendOptions(savedOptions);
  }
);
